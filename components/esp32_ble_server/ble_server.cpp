#include "ble_server.h"

#include "esphome/core/log.h"

#ifdef USE_ESP32

namespace esphome::esp32_ble_server {

static const char *const TAG = "esp32_ble_server";

BLEServer *global_ble_server = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

float BLEServer::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void BLEServer::setup() {
  global_ble_server = this;
  this->setup_gatt_server_();
  if (this->started_)
    this->restart_advertising_();
}

BLEService *BLEServer::create_service(ble_device_base::ESPBTUUID uuid, bool advertise, uint16_t num_handles) {
  auto *service = new BLEService(uuid, num_handles, static_cast<uint8_t>(this->services_.size()),  // NOLINT
                                 advertise);
  this->services_.push_back(service);
  return service;
}

BLEService *BLEServer::get_service(ble_device_base::ESPBTUUID uuid, uint8_t inst_id) {
  for (auto *service : this->services_) {
    if (service->get_uuid() == uuid && service->get_inst_id() == inst_id)
      return service;
  }
  return nullptr;
}

void BLEServer::setup_gatt_server_() {
  if (this->services_.empty())
    return;

  this->svc_defs_ = std::make_unique<ble_gatt_svc_def[]>(this->services_.size() + 1);
  for (size_t i = 0; i < this->services_.size(); i++) {
    this->services_[i]->do_create(this);
    this->svc_defs_[i] = this->services_[i]->native_def();
  }
  this->svc_defs_[this->services_.size()] = {};  // terminator (type == 0)

  int rc = ble_gatts_count_cfg(this->svc_defs_.get());
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gatts_count_cfg failed: %d", rc);
    return;
  }
  rc = ble_gatts_add_svcs(this->svc_defs_.get());
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gatts_add_svcs failed: %d", rc);
    return;
  }
  rc = ble_gatts_start();
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gatts_start failed: %d", rc);
    return;
  }

  // val_handle_ is only meaningful once ble_gatts_add_svcs() has filled it
  // in -- the routing map is built here, after registration, not while the
  // def table was being assembled above.
  for (auto *service : this->services_) {
    for (auto *chr : service->characteristics_) {
      this->handle_to_characteristic_[chr->get_val_handle()] = chr;
    }
  }
  this->started_ = true;
  ESP_LOGI(TAG, "GATT server registered: %u service(s)", this->services_.size());
}

void BLEServer::restart_advertising_() {
  this->parent_->start_advertising_with_callback(&BLEServer::gap_event_cb_, this);
}

int BLEServer::gap_event_cb_(struct ble_gap_event *event, void *arg) {
  return static_cast<BLEServer *>(arg)->handle_gap_event_(event);
}

int BLEServer::handle_gap_event_(struct ble_gap_event *event) {
  switch (event->type) {
    case BLE_GAP_EVENT_CONNECT: {
      if (event->connect.status == 0) {
        ServerEvent qevent;
        qevent.type = ServerEventType::CONNECT;
        qevent.conn_handle = event->connect.conn_handle;
        server_event_queue().push_from_host_task(qevent);
      } else {
        // The connect attempt itself failed (rare for a peripheral) -- no
        // DISCONNECT will ever follow it, so re-arm advertising directly.
        this->restart_advertising_();
      }
      return 0;
    }
    case BLE_GAP_EVENT_DISCONNECT: {
      ServerEvent qevent;
      qevent.type = ServerEventType::DISCONNECT;
      qevent.conn_handle = event->disconnect.conn.conn_handle;
      server_event_queue().push_from_host_task(qevent);
      // Undirected-connectable advertising stops the instant a link forms;
      // the peer disconnecting is exactly when we must become discoverable
      // again for the next one.
      this->restart_advertising_();
      return 0;
    }
    case BLE_GAP_EVENT_SUBSCRIBE: {
      auto it = this->handle_to_characteristic_.find(event->subscribe.attr_handle);
      if (it == this->handle_to_characteristic_.end())
        return 0;
      ServerEvent qevent;
      qevent.type = ServerEventType::SUBSCRIBE;
      qevent.target = it->second;
      qevent.conn_handle = event->subscribe.conn_handle;
      qevent.cur_notify = event->subscribe.cur_notify;
      qevent.cur_indicate = event->subscribe.cur_indicate;
      server_event_queue().push_from_host_task(qevent);
      return 0;
    }
    default:
      return 0;
  }
}

int8_t BLEServer::find_client_index_(uint16_t conn_handle) const {
  for (uint8_t i = 0; i < this->client_count_; i++) {
    if (this->clients_[i] == conn_handle)
      return static_cast<int8_t>(i);
  }
  return -1;
}

void BLEServer::dispatch_callbacks_(CallbackType type, uint16_t conn_handle) {
  for (auto &entry : this->callbacks_) {
    if (entry.type == type)
      entry.callback(conn_handle);
  }
}

void BLEServer::loop() {
  ServerEvent event;
  while (server_event_queue().pop(&event)) {
    switch (event.type) {
      case ServerEventType::CONNECT:
        if (this->find_client_index_(event.conn_handle) < 0 && this->client_count_ < MAX_CLIENTS) {
          this->clients_[this->client_count_++] = event.conn_handle;
        }
        this->dispatch_callbacks_(CallbackType::ON_CONNECT, event.conn_handle);
        break;
      case ServerEventType::DISCONNECT: {
        int8_t idx = this->find_client_index_(event.conn_handle);
        if (idx >= 0) {
          this->clients_[idx] = this->clients_[this->client_count_ - 1];
          this->client_count_--;
        }
        this->dispatch_callbacks_(CallbackType::ON_DISCONNECT, event.conn_handle);
        break;
      }
      case ServerEventType::SUBSCRIBE:
        static_cast<BLECharacteristic *>(event.target)
            ->handle_subscribe_(event.conn_handle, event.cur_notify, event.cur_indicate);
        break;
      case ServerEventType::CHR_WRITE:
        static_cast<BLECharacteristic *>(event.target)
            ->handle_write_from_queue_(event.data, event.data_len, event.conn_handle);
        delete[] event.data;
        break;
      case ServerEventType::DSC_WRITE:
        static_cast<BLEDescriptor *>(event.target)
            ->handle_write_from_queue_(event.data, event.data_len, event.conn_handle);
        delete[] event.data;
        break;
    }
  }
}

void BLEServer::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BLE Server (NimBLE):\n"
                "  Services: %u\n"
                "  Max clients: %u\n"
                "  Connected clients: %u",
                this->services_.size(), this->max_clients_, this->client_count_);
}

}  // namespace esphome::esp32_ble_server

#endif  // USE_ESP32
