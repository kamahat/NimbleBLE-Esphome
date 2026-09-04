#include "ble_characteristic.h"
#include "ble_service.h"
#include "nimble_server_event.h"

#include "esphome/components/nimble_ble/nimble_uuid.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32

#include "host/ble_hs_mbuf.h"
#include "os/os_mbuf.h"

#include <algorithm>

namespace esphome::esp32_ble_server {

static const char *const TAG = "esp32_ble_server.chr";

BLECharacteristic::BLECharacteristic(ble_device_base::ESPBTUUID uuid, uint32_t properties)
    : uuid_(uuid), flags_(properties) {}

BLECharacteristic::~BLECharacteristic() = default;

void BLECharacteristic::set_value(std::vector<uint8_t> &&buffer) {
  portENTER_CRITICAL(&this->value_mux_);
  this->value_ = std::move(buffer);
  portEXIT_CRITICAL(&this->value_mux_);
}

namespace {
uint32_t property_to_nimble_flags(uint32_t properties) {
  uint32_t flags = 0;
  if (properties & BLECharacteristic::PROPERTY_READ)
    flags |= BLE_GATT_CHR_F_READ;
  if (properties & BLECharacteristic::PROPERTY_WRITE)
    flags |= BLE_GATT_CHR_F_WRITE;
  if (properties & BLECharacteristic::PROPERTY_NOTIFY)
    flags |= BLE_GATT_CHR_F_NOTIFY;
  if (properties & BLECharacteristic::PROPERTY_INDICATE)
    flags |= BLE_GATT_CHR_F_INDICATE;
  if (properties & BLECharacteristic::PROPERTY_BROADCAST)
    flags |= BLE_GATT_CHR_F_BROADCAST;
  if (properties & BLECharacteristic::PROPERTY_WRITE_NR)
    flags |= BLE_GATT_CHR_F_WRITE_NO_RSP;
  return flags;
}
}  // namespace

void BLECharacteristic::do_create(BLEService *service) {
  this->service_ = service;
  nimble_ble::espbtuuid_to_nimble_uuid(this->uuid_, &this->nimble_uuid_);

  // NimBLE manages the CCCD (0x2902) automatically for a notify/indicate
  // characteristic ("Do not include CCCD; it gets added automatically" --
  // host/ble_gatt.h) -- a user-declared 0x2902 descriptor is real for API
  // purposes (set_value/on_write still resolve against the C++ object) but
  // excluded from the actual registered table, or ble_gatts_add_svcs()
  // would see a duplicate/conflicting descriptor. Documented in
  // docs/OVERRIDE_CAVEATS.md.
  size_t real_count = 0;
  for (auto *dsc : this->descriptors_) {
    if (dsc->get_uuid16_if_short() != 0x2902)
      real_count++;
  }
  if (real_count > 0) {
    this->dsc_defs_ = std::make_unique<ble_gatt_dsc_def[]>(real_count + 1);
    size_t i = 0;
    for (auto *dsc : this->descriptors_) {
      if (dsc->get_uuid16_if_short() == 0x2902)
        continue;
      dsc->do_create(this);
      this->dsc_defs_[i++] = dsc->native_def();
    }
    this->dsc_defs_[real_count] = {};  // terminator (uuid == nullptr)
  } else {
    // Still call do_create on any (skipped) 2902 so its API surface
    // (set_value/on_write) is usable even though it is not really
    // registered -- matches the documented simplification above.
    for (auto *dsc : this->descriptors_) {
      dsc->do_create(this);
    }
  }

  this->def_.uuid = &this->nimble_uuid_.u;
  this->def_.access_cb = &BLECharacteristic::access_cb_;
  this->def_.arg = this;
  this->def_.descriptors = real_count > 0 ? this->dsc_defs_.get() : nullptr;
  this->def_.flags = property_to_nimble_flags(this->flags_);
  this->def_.min_key_size = 0;
  this->def_.val_handle = &this->val_handle_;
  this->def_.cpfd = nullptr;
  this->created_ = true;
}

int BLECharacteristic::access_cb_(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                                  void *arg) {
  return static_cast<BLECharacteristic *>(arg)->handle_access_(conn_handle, ctxt);
}

int BLECharacteristic::handle_access_(uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
    // A real read (as opposed to NimBLE re-invoking this to fetch the
    // payload for notify()/indicate() -- see below) may want to refresh the
    // value first. Matches the reference's own accepted behavior
    // (BLECharacteristicSetValueAction's pattern): on_read runs
    // synchronously here, on the host task, not marshaled -- the ATT read
    // response must be filled within this same call, so there is no main
    // loop round trip to wait for. Documented: on_read callbacks must stay
    // fast and simple (in practice: just set_value(), itself thread-safe).
    if (this->on_read_callback_)
      (*this->on_read_callback_)(conn_handle);
    portENTER_CRITICAL(&this->value_mux_);
    int rc = os_mbuf_append(ctxt->om, this->value_.data(), this->value_.size());
    portEXIT_CRITICAL(&this->value_mux_);
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    uint8_t *buf = new uint8_t[len];
    ble_hs_mbuf_to_flat(ctxt->om, buf, len, nullptr);
    portENTER_CRITICAL(&this->value_mux_);
    this->value_.assign(buf, buf + len);
    portEXIT_CRITICAL(&this->value_mux_);
    if (this->on_write_callback_) {
      ServerEvent event;
      event.type = ServerEventType::CHR_WRITE;
      event.target = this;
      event.conn_handle = conn_handle;
      event.data = buf;
      event.data_len = len;
      server_event_queue().push_from_host_task(event);
    } else {
      delete[] buf;
    }
    return 0;
  }
  return BLE_ATT_ERR_UNLIKELY;
}

void BLECharacteristic::notify() {
  if (this->val_handle_ == 0xFFFF)
    return;
  for (auto &entry : this->clients_to_notify_) {
    int rc = entry.indicate ? ble_gatts_indicate(entry.conn_handle, this->val_handle_)
                            : ble_gatts_notify(entry.conn_handle, this->val_handle_);
    if (rc != 0)
      ESP_LOGW(TAG, "notify/indicate failed for conn %u: %d", entry.conn_handle, rc);
  }
}

void BLECharacteristic::handle_subscribe_(uint16_t conn_handle, bool cur_notify, bool cur_indicate) {
  auto it = std::find_if(this->clients_to_notify_.begin(), this->clients_to_notify_.end(),
                        [conn_handle](const ClientNotificationEntry &e) { return e.conn_handle == conn_handle; });
  if (!cur_notify && !cur_indicate) {
    if (it != this->clients_to_notify_.end())
      this->clients_to_notify_.erase(it);
    return;
  }
  bool indicate = cur_indicate;  // indicate takes priority if somehow both are set
  if (it != this->clients_to_notify_.end()) {
    it->indicate = indicate;
  } else {
    this->clients_to_notify_.push_back({conn_handle, indicate});
  }
}

void BLECharacteristic::handle_write_from_queue_(const uint8_t *data, uint16_t len, uint16_t conn_handle) {
  if (this->on_write_callback_)
    (*this->on_write_callback_)(std::span<const uint8_t>(data, len), conn_handle);
}

}  // namespace esphome::esp32_ble_server

#endif  // USE_ESP32
