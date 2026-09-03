#include "nimble_gattc.h"

#ifdef USE_ESP32
#ifdef USE_BLE_GATT_CLIENT

#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "nimble_uuid.h"

#include "os/os_mbuf.h"

#include <cstring>

namespace esphome::nimble_ble {

static const char *const TAG = "nimble_ble.gattc";

// HCI "Remote User Terminated Connection" (0x13) -- not in a public NimBLE
// header (only a private #define inside ble_hs_hci.c), so spelled out here.
static constexpr uint8_t HCI_ERR_REM_USER_CONN_TERM = 0x13;

namespace {
/// Reverse of ble_device_base::uint64_to_mac_msb_first: unpack this
/// codebase's uint64 address convention into NimBLE's controller-order
/// (LSB-first) 6-byte ble_addr_t.val.
inline void uint64_to_mac_lsb_first(uint64_t address, uint8_t out[6]) {
  for (int i = 0; i < 6; i++)
    out[i] = (address >> (i * 8)) & 0xFF;
}
}  // namespace

void drain_gatt_events() {
  // Bounded per call so one connection's burst (e.g. a large service
  // discovery) cannot starve the rest of the main loop indefinitely; the
  // queue keeps whatever does not fit for the next tick.
  static constexpr int MAX_EVENTS_PER_DRAIN = 32;
  GattEvent event;
  for (int i = 0; i < MAX_EVENTS_PER_DRAIN; i++) {
    if (!gatt_event_queue(ESPHOME_BLE_GATT_CLIENT_COUNT * 16).pop(&event))
      return;
    if (event.owner != nullptr)
      static_cast<NimbleGattEngine *>(event.owner)->handle_gatt_event_(event);
    delete[] event.data;
  }
}

// ---------------------------------------------------------------------------
// NimBLE callback trampolines -- host task context. These ONLY copy fields
// out of NimBLE-owned data into a GattEvent and push it; they must never
// touch a NimbleGattEngine's own state directly (see nimble_event.h).
// ---------------------------------------------------------------------------

int NimbleGattEngine::gap_event_cb_(struct ble_gap_event *event, void *arg) {
  GattEvent qevent;
  qevent.owner = arg;
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
      qevent.type = GattEventType::CONNECT;
      qevent.conn_handle = event->connect.conn_handle;
      qevent.status = event->connect.status;
      break;
    case BLE_GAP_EVENT_DISCONNECT:
      qevent.type = GattEventType::DISCONNECT;
      qevent.conn_handle = event->disconnect.conn.conn_handle;
      qevent.status = event->disconnect.reason;
      break;
    case BLE_GAP_EVENT_MTU:
      qevent.type = GattEventType::MTU;
      qevent.conn_handle = event->mtu.conn_handle;
      qevent.handle2 = event->mtu.value;
      break;
    case BLE_GAP_EVENT_NOTIFY_RX: {
      qevent.type = GattEventType::NOTIFY_RX;
      qevent.conn_handle = event->notify_rx.conn_handle;
      qevent.handle = event->notify_rx.attr_handle;
      qevent.handle2 = event->notify_rx.indication;
      if (event->notify_rx.om != nullptr) {
        uint16_t len = OS_MBUF_PKTLEN(event->notify_rx.om);
        if (len > 0) {
          qevent.data = new uint8_t[len];
          os_mbuf_copydata(event->notify_rx.om, 0, len, qevent.data);
          qevent.data_len = len;
        }
      }
      break;
    }
    case BLE_GAP_EVENT_ENC_CHANGE:
      qevent.type = GattEventType::ENC_CHANGE;
      qevent.conn_handle = event->enc_change.conn_handle;
      qevent.status = event->enc_change.status;
      break;
    case BLE_GAP_EVENT_CONN_UPDATE:
      qevent.type = GattEventType::CONN_UPDATE;
      qevent.conn_handle = event->conn_update.conn_handle;
      qevent.status = event->conn_update.status;
      break;
    default:
      return 0;  // not one we track
  }
  gatt_event_queue(0).push_from_host_task(qevent);
  return 0;
}

namespace {
inline void copy_uuid_(GattEvent *qevent, const ble_uuid_any_t &uuid) {
  qevent->uuid_type = uuid.u.type;
  switch (uuid.u.type) {
    case BLE_UUID_TYPE_16:
      memcpy(qevent->uuid128, &uuid.u16.value, sizeof(uuid.u16.value));
      break;
    case BLE_UUID_TYPE_32:
      memcpy(qevent->uuid128, &uuid.u32.value, sizeof(uuid.u32.value));
      break;
    default:
      memcpy(qevent->uuid128, uuid.u128.value, sizeof(uuid.u128.value));
      break;
  }
}
}  // namespace

int NimbleGattEngine::disc_svc_cb_(uint16_t conn_handle, const struct ble_gatt_error *error,
                                   const struct ble_gatt_svc *service, void *arg) {
  GattEvent qevent;
  qevent.owner = arg;
  qevent.conn_handle = conn_handle;
  if (error->status == 0 && service != nullptr) {
    qevent.type = GattEventType::DISC_SVC;
    qevent.handle = service->start_handle;
    qevent.handle2 = service->end_handle;
    copy_uuid_(&qevent, service->uuid);
  } else {
    qevent.type = GattEventType::DISC_SVC_DONE;
    qevent.status = error->status;
  }
  gatt_event_queue(0).push_from_host_task(qevent);
  return 0;
}

int NimbleGattEngine::disc_chr_cb_(uint16_t conn_handle, const struct ble_gatt_error *error,
                                   const struct ble_gatt_chr *chr, void *arg) {
  GattEvent qevent;
  qevent.owner = arg;
  qevent.conn_handle = conn_handle;
  if (error->status == 0 && chr != nullptr) {
    qevent.type = GattEventType::DISC_CHR;
    qevent.handle = chr->def_handle;
    qevent.handle2 = chr->val_handle;
    qevent.properties = chr->properties;
    copy_uuid_(&qevent, chr->uuid);
  } else {
    qevent.type = GattEventType::DISC_CHR_DONE;
    qevent.status = error->status;
  }
  gatt_event_queue(0).push_from_host_task(qevent);
  return 0;
}

int NimbleGattEngine::disc_dsc_cb_(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t chr_val_handle,
                                   const struct ble_gatt_dsc *dsc, void *arg) {
  GattEvent qevent;
  qevent.owner = arg;
  qevent.conn_handle = conn_handle;
  if (error->status == 0 && dsc != nullptr) {
    qevent.type = GattEventType::DISC_DSC;
    qevent.handle = dsc->handle;
    copy_uuid_(&qevent, dsc->uuid);
  } else {
    qevent.type = GattEventType::DISC_DSC_DONE;
    qevent.status = error->status;
  }
  gatt_event_queue(0).push_from_host_task(qevent);
  return 0;
}

int NimbleGattEngine::attr_cb_(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr,
                               void *arg) {
  GattEvent qevent;
  qevent.owner = arg;
  qevent.conn_handle = conn_handle;
  qevent.type = GattEventType::ATTR_OP;
  qevent.status = error != nullptr ? error->status : 0;
  qevent.handle = attr != nullptr ? attr->handle : 0;
  if (attr != nullptr && attr->om != nullptr) {
    uint16_t len = OS_MBUF_PKTLEN(attr->om);
    if (len > 0) {
      qevent.data = new uint8_t[len];
      os_mbuf_copydata(attr->om, 0, len, qevent.data);
      qevent.data_len = len;
    }
  }
  gatt_event_queue(0).push_from_host_task(qevent);
  return 0;
}

// ---------------------------------------------------------------------------
// Main-loop side -- everything below runs only from handle_gatt_event_() or
// from a direct API call, both always on the ESPHome main loop.
// ---------------------------------------------------------------------------

int NimbleGattEngine::connect(uint64_t address, uint8_t addr_type) {
  if (this->conn_handle_ != BLE_HS_CONN_HANDLE_NONE || this->fsm_.state() == BleConnState::CONNECTING)
    return BLE_HS_EALREADY;

  ble_addr_t peer_addr;
  peer_addr.type = addr_type;
  uint64_to_mac_lsb_first(address, peer_addr.val);

  struct ble_gap_conn_params conn_params;
  memset(&conn_params, 0, sizeof(conn_params));
  conn_params.scan_itvl = 0x0010;
  conn_params.scan_window = 0x0010;
  conn_params.itvl_min = ble_device_base::FAST_MIN_CONN_INTERVAL;
  conn_params.itvl_max = ble_device_base::FAST_MAX_CONN_INTERVAL;
  conn_params.latency = 0;
  conn_params.supervision_timeout = ble_device_base::FAST_CONN_TIMEOUT;

  // A generous NimBLE-level duration: our own FSM deadline (set right below)
  // is the real timeout authority and can cancel this in-flight attempt via
  // gatt_disconnect() -- see loop(). This just bounds how long NimBLE itself
  // would wait if our FSM never got a chance to poll.
  int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &peer_addr, 10000, &conn_params, &NimbleGattEngine::gap_event_cb_,
                           this->event_owner());
  if (rc != 0) {
    ESP_LOGW(TAG, "ble_gap_connect failed: %d", rc);
    return rc;
  }
  this->connect_result_reported_ = false;
  this->fsm_.handle_event(BleConnEvent::CONNECT_REQUEST, millis());
  return 0;
}

int NimbleGattEngine::retry_connect(uint64_t address, uint8_t addr_type) {
  if (this->fsm_.state() == BleConnState::BACKOFF)
    this->fsm_.handle_event(BleConnEvent::BACKOFF_ELAPSED, millis());
  return this->connect(address, addr_type);
}

int NimbleGattEngine::gatt_disconnect() {
  if (this->fsm_.state() == BleConnState::CONNECTING) {
    // No conn_handle yet: cancel the pending attempt itself. Its own
    // gap_event_cb_ callback still delivers a CONNECT event (nonzero
    // status) for this, so finish_connect_() does the state/listener work
    // -- nothing to do here beyond issuing the cancel.
    return ble_gap_conn_cancel();
  }
  if (this->conn_handle_ == BLE_HS_CONN_HANDLE_NONE)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;  // nothing to tear down, no completion will follow
  this->fsm_.handle_event(BleConnEvent::DISCONNECT_REQUEST, millis());
  return ble_gap_terminate(this->conn_handle_, HCI_ERR_REM_USER_CONN_TERM);
}

bool NimbleGattEngine::cancel_gatt_disconnect() {
  // NimBLE's ble_gap_terminate has no "undo" once issued (unlike some stacks'
  // two-phase teardown) -- once DISCONNECT_REQUEST has been sent there is
  // nothing left to cancel.
  return false;
}

int NimbleGattEngine::discover_services() {
  if (this->conn_handle_ == BLE_HS_CONN_HANDLE_NONE)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  this->services_.clear();
  this->characteristics_.clear();
  this->descriptors_.clear();
  this->discover_svc_index_ = 0;
  this->discover_chr_index_ = 0;
  return ble_gattc_disc_all_svcs(this->conn_handle_, &NimbleGattEngine::disc_svc_cb_, this->event_owner());
}

int NimbleGattEngine::read_characteristic(uint16_t handle) {
  if (this->conn_handle_ == BLE_HS_CONN_HANDLE_NONE)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  if (this->pending_op_ != PendingOp::NONE)
    return BLE_HS_EBUSY;
  this->pending_op_ = PendingOp::READ_CHR;
  int rc = ble_gattc_read(this->conn_handle_, handle, &NimbleGattEngine::attr_cb_, this->event_owner());
  if (rc != 0)
    this->pending_op_ = PendingOp::NONE;
  return rc;
}

int NimbleGattEngine::read_descriptor(uint16_t handle) {
  if (this->conn_handle_ == BLE_HS_CONN_HANDLE_NONE)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  if (this->pending_op_ != PendingOp::NONE)
    return BLE_HS_EBUSY;
  this->pending_op_ = PendingOp::READ_DSC;
  int rc = ble_gattc_read(this->conn_handle_, handle, &NimbleGattEngine::attr_cb_, this->event_owner());
  if (rc != 0)
    this->pending_op_ = PendingOp::NONE;
  return rc;
}

int NimbleGattEngine::write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response) {
  if (this->conn_handle_ == BLE_HS_CONN_HANDLE_NONE)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  if (!response) {
    // Write-without-response has no ATT-level completion at all: report the
    // synchronous accept immediately rather than leave the caller waiting on
    // a listener callback NimBLE will never deliver.
    int rc = ble_gattc_write_no_rsp_flat(this->conn_handle_, handle, data, len);
    if (rc == 0 && this->listener_ != nullptr)
      this->listener_->on_write_result(handle, 0);
    return rc;
  }
  if (this->pending_op_ != PendingOp::NONE)
    return BLE_HS_EBUSY;
  this->pending_op_ = PendingOp::WRITE_CHR;
  int rc =
      ble_gattc_write_flat(this->conn_handle_, handle, data, len, &NimbleGattEngine::attr_cb_, this->event_owner());
  if (rc != 0)
    this->pending_op_ = PendingOp::NONE;
  return rc;
}

int NimbleGattEngine::write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len) {
  if (this->conn_handle_ == BLE_HS_CONN_HANDLE_NONE)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  if (this->pending_op_ != PendingOp::NONE)
    return BLE_HS_EBUSY;
  this->pending_op_ = PendingOp::WRITE_DSC;
  int rc =
      ble_gattc_write_flat(this->conn_handle_, handle, data, len, &NimbleGattEngine::attr_cb_, this->event_owner());
  if (rc != 0)
    this->pending_op_ = PendingOp::NONE;
  return rc;
}

int NimbleGattEngine::notify_characteristic(uint16_t handle, bool enable) {
  if (this->conn_handle_ == BLE_HS_CONN_HANDLE_NONE)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  // Local bookkeeping only -- the CCCD write is the caller's responsibility
  // via a plain write_descriptor() (ble_gatt_client.h's documented
  // contract). NimBLE delivers BLE_GAP_EVENT_NOTIFY_RX for any inbound
  // notify/indicate on this connection regardless of local registration
  // state, so there is nothing to ask the stack for beyond this.
  if (this->listener_ != nullptr)
    this->listener_->on_notify_state(handle, enable, 0);
  return 0;
}

int NimbleGattEngine::pair() {
  if (this->conn_handle_ == BLE_HS_CONN_HANDLE_NONE)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  return ble_gap_security_initiate(this->conn_handle_);
}

int NimbleGattEngine::update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency,
                                               uint16_t timeout) {
  if (this->conn_handle_ == BLE_HS_CONN_HANDLE_NONE)
    return ble_device_base::GATT_ERR_NOT_CONNECTED;
  struct ble_gap_upd_params params;
  memset(&params, 0, sizeof(params));
  params.itvl_min = min_interval;
  params.itvl_max = max_interval;
  params.latency = latency;
  params.supervision_timeout = timeout;
  return ble_gap_update_params(this->conn_handle_, &params);
}

ble_device_base::GattServiceTable NimbleGattEngine::get_service_table() {
  ble_device_base::GattServiceTable table;
  table.services = this->services_.empty() ? nullptr : this->services_.data();
  table.characteristics = this->characteristics_.empty() ? nullptr : this->characteristics_.data();
  table.descriptors = this->descriptors_.empty() ? nullptr : this->descriptors_.data();
  table.service_count = static_cast<uint16_t>(this->services_.size());
  table.characteristic_count = static_cast<uint16_t>(this->characteristics_.size());
  table.descriptor_count = static_cast<uint16_t>(this->descriptors_.size());
  return table;
}

void NimbleGattEngine::release_services() {
  this->services_.clear();
  this->services_.shrink_to_fit();
  this->characteristics_.clear();
  this->characteristics_.shrink_to_fit();
  this->descriptors_.clear();
  this->descriptors_.shrink_to_fit();
}

void NimbleGattEngine::loop() {
  drain_gatt_events();
  // Captured before poll_timeout() (which transitions to Backoff on a hit)
  // so the two deadline states can be told apart below -- they resolve
  // through different listener methods.
  BleConnState prior_state = this->fsm_.state();
  if (this->fsm_.poll_timeout(millis())) {
    // The FSM just self-timed-out (Connecting or Discovering exceeded its
    // deadline) and is now in Backoff -- this is the bounded-exit guarantee
    // that replaces Bluedroid's unbounded esp_ble_gattc_search_service()
    // hang. Tear down whatever NimBLE-side state still exists so it cannot
    // deliver a stale completion into the next connection attempt.
    if (this->conn_handle_ != BLE_HS_CONN_HANDLE_NONE) {
      ble_gap_terminate(this->conn_handle_, HCI_ERR_REM_USER_CONN_TERM);
    } else {
      ble_gap_conn_cancel();
    }
    this->pending_op_ = PendingOp::NONE;
    if (prior_state == BleConnState::CONNECTING) {
      // No established link to tear down asynchronously -- the cancelled
      // attempt's own CONNECT event may still arrive later for the same
      // attempt (empirically confirmed on hardware), so guard against
      // reporting this attempt's outcome twice.
      if (!this->connect_result_reported_) {
        this->connect_result_reported_ = true;
        if (this->listener_ != nullptr)
          this->listener_->on_connection_state(false, 0, BLE_HS_ETIMEOUT);
      }
    } else {
      // Discovering: the connection itself is fine, only discovery hung.
      // ble_gap_terminate above starts a real teardown, whose DISCONNECT
      // event reports on_connection_state through the normal
      // finish_disconnect_ path -- report the more specific discovery
      // failure here instead of a second, redundant connection-state call.
      if (this->listener_ != nullptr)
        this->listener_->on_service_discovery_done(BLE_HS_ETIMEOUT);
    }
  }
}

void NimbleGattEngine::finish_connect_(int error) {
  if (this->connect_result_reported_) {
    // A late completion for an attempt our own bounded timeout already
    // resolved (see loop()) -- e.g. the cancelled connect's own async
    // CONNECT event arriving after the fact. Nothing left to report; the
    // FSM is already in Backoff and a GAP_CONNECT_* event applied there is
    // a documented no-op anyway, but skip it explicitly for clarity.
    return;
  }
  this->connect_result_reported_ = true;
  if (error == 0) {
    this->fsm_.handle_event(BleConnEvent::GAP_CONNECT_OK, millis());
    // Fire-and-forget: negotiating a larger MTU only helps later reads/
    // writes carry more per PDU. Not gating on_connection_state on it keeps
    // the critical connect path off a second async round trip -- mtu_
    // updates silently in handle_gatt_event_ when GAP_EVENT_MTU arrives.
    ble_gattc_exchange_mtu(this->conn_handle_, nullptr, nullptr);
    if (this->listener_ != nullptr)
      this->listener_->on_connection_state(true, this->mtu_, 0);
  } else {
    this->conn_handle_ = BLE_HS_CONN_HANDLE_NONE;
    this->fsm_.handle_event(BleConnEvent::GAP_CONNECT_FAIL, millis());
    if (this->listener_ != nullptr)
      this->listener_->on_connection_state(false, 0, error);
  }
}

void NimbleGattEngine::finish_disconnect_(int reason) {
  this->conn_handle_ = BLE_HS_CONN_HANDLE_NONE;
  this->pending_op_ = PendingOp::NONE;
  // A mismatched state (e.g. this fires while still Connecting, which
  // cannot happen per NimBLE's own event model) is a documented FSM no-op,
  // not an error -- see BleConnectionFsm::handle_event.
  this->fsm_.handle_event(BleConnEvent::GAP_DISCONNECT_EVT, millis());
  if (this->listener_ != nullptr)
    this->listener_->on_connection_state(false, 0, reason);
}

void NimbleGattEngine::start_next_service_chr_discovery_() {
  if (this->discover_svc_index_ >= this->services_.size()) {
    this->discover_chr_index_ = 0;
    this->start_next_chr_dsc_discovery_();
    return;
  }
  auto &svc = this->services_[this->discover_svc_index_];
  svc.first_characteristic = static_cast<uint16_t>(this->characteristics_.size());
  int rc = ble_gattc_disc_all_chrs(this->conn_handle_, svc.start_handle, svc.end_handle,
                                   &NimbleGattEngine::disc_chr_cb_, this->event_owner());
  if (rc != 0) {
    ESP_LOGW(TAG, "ble_gattc_disc_all_chrs failed: %d", rc);
    this->finish_discovery_(rc);
  }
}

void NimbleGattEngine::start_next_chr_dsc_discovery_() {
  while (this->discover_chr_index_ < this->characteristics_.size()) {
    auto &chr = this->characteristics_[this->discover_chr_index_];
    if (chr.end_handle > chr.value_handle) {
      chr.first_descriptor = static_cast<uint16_t>(this->descriptors_.size());
      int rc = ble_gattc_disc_all_dscs(this->conn_handle_, chr.value_handle + 1, chr.end_handle,
                                       &NimbleGattEngine::disc_dsc_cb_, this->event_owner());
      if (rc != 0) {
        ESP_LOGW(TAG, "ble_gattc_disc_all_dscs failed: %d", rc);
        this->finish_discovery_(rc);
      }
      return;  // wait for this characteristic's DISC_DSC_DONE
    }
    this->discover_chr_index_++;
  }
  this->finish_discovery_(0);
}

void NimbleGattEngine::finish_discovery_(int error) {
  // A synchronous discovery-call failure (rare: e.g. host resource
  // exhaustion) has no dedicated FSM event of its own (spec/transitions.json
  // only models discover_done/discover_timeout/gap_disconnect_evt out of
  // Discovering) -- routing it through DISCOVER_TIMEOUT lands it in the same
  // Backoff state a real elapsed deadline would, which is the correct
  // outcome even though the trigger was not actually a clock. Documented
  // simplification for v1 (docs/ARCHITECTURE.md).
  this->fsm_.handle_event(error == 0 ? BleConnEvent::DISCOVER_DONE : BleConnEvent::DISCOVER_TIMEOUT, millis());
  if (this->listener_ != nullptr)
    this->listener_->on_service_discovery_done(error);
}

void NimbleGattEngine::handle_gatt_event_(const GattEvent &event) {
  switch (event.type) {
    case GattEventType::CONNECT:
      this->conn_handle_ = event.status == 0 ? event.conn_handle : BLE_HS_CONN_HANDLE_NONE;
      this->finish_connect_(event.status);
      break;
    case GattEventType::DISCONNECT:
      this->finish_disconnect_(event.status);
      break;
    case GattEventType::MTU:
      if (event.conn_handle == this->conn_handle_)
        this->mtu_ = event.handle2;
      break;
    case GattEventType::DISC_SVC: {
      ble_device_base::GattService svc{};
      svc.uuid = nimble_raw_uuid_to_espbtuuid(event.uuid_type, event.uuid128);
      svc.start_handle = event.handle;
      svc.end_handle = event.handle2;
      this->services_.push_back(svc);
      break;
    }
    case GattEventType::DISC_SVC_DONE:
      this->discover_svc_index_ = 0;
      this->start_next_service_chr_discovery_();
      break;
    case GattEventType::DISC_CHR: {
      // Backfill the previous characteristic's end_handle now that we know
      // where it stops: one before this one's definition handle. The very
      // first characteristic in a service has no previous entry to backfill.
      auto &svc = this->services_[this->discover_svc_index_];
      if (svc.characteristic_count > 0)
        this->characteristics_.back().end_handle = event.handle - 1;
      ble_device_base::GattCharacteristic chr{};
      chr.uuid = nimble_raw_uuid_to_espbtuuid(event.uuid_type, event.uuid128);
      chr.value_handle = event.handle2;
      chr.end_handle = 0;  // filled in above (next char) or below (DISC_CHR_DONE, last char)
      chr.properties = event.properties;
      this->characteristics_.push_back(chr);
      svc.characteristic_count++;
      break;
    }
    case GattEventType::DISC_CHR_DONE: {
      auto &svc = this->services_[this->discover_svc_index_];
      if (svc.characteristic_count > 0)
        this->characteristics_.back().end_handle = svc.end_handle;
      this->discover_svc_index_++;
      this->start_next_service_chr_discovery_();
      break;
    }
    case GattEventType::DISC_DSC: {
      ble_device_base::GattDescriptor dsc{};
      dsc.uuid = nimble_raw_uuid_to_espbtuuid(event.uuid_type, event.uuid128);
      dsc.handle = event.handle;
      this->descriptors_.push_back(dsc);
      this->characteristics_[this->discover_chr_index_].descriptor_count++;
      break;
    }
    case GattEventType::DISC_DSC_DONE:
      this->discover_chr_index_++;
      this->start_next_chr_dsc_discovery_();
      break;
    case GattEventType::ATTR_OP: {
      PendingOp op = this->pending_op_;
      this->pending_op_ = PendingOp::NONE;
      if (this->listener_ == nullptr)
        break;
      switch (op) {
        case PendingOp::READ_CHR:
        case PendingOp::READ_DSC:
          this->listener_->on_read_result(event.handle, event.data, event.data_len, event.status);
          break;
        case PendingOp::WRITE_CHR:
        case PendingOp::WRITE_DSC:
          this->listener_->on_write_result(event.handle, event.status);
          break;
        case PendingOp::NONE:
        default:
          // NimBLE allows one outstanding attr op per connection, so this
          // should not happen; log and drop rather than guess which
          // listener method to call.
          ESP_LOGW(TAG, "Attr completion with no pending op tracked (handle=%u)", event.handle);
          break;
      }
      break;
    }
    case GattEventType::NOTIFY_RX:
      if (this->listener_ != nullptr)
        this->listener_->on_notify_data(event.handle, event.data, event.data_len);
      break;
    case GattEventType::ENC_CHANGE:
      if (this->listener_ != nullptr)
        this->listener_->on_pairing_result(event.status);
      break;
    case GattEventType::CONN_UPDATE:
      // No dedicated completion in the contract for update_connection_params
      // -- nothing to deliver.
      break;
  }
}

}  // namespace esphome::nimble_ble

#endif  // USE_BLE_GATT_CLIENT
#endif  // USE_ESP32
