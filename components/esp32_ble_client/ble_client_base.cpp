#include "ble_client_base.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

#include <cstdio>

namespace esphome::esp32_ble_client {

static const char *const TAG = "esp32_ble_client";

float BLEClientBase::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void BLEClientBase::setup() {
  this->engine_.set_listener(this);
  if (this->auto_connect_)
    this->connect();
}

void BLEClientBase::loop() {
  this->engine_.loop();
  if (this->engine_.state() != nimble_ble::BleConnState::BACKOFF) {
    this->backoff_started_at_ = 0;
    return;
  }
  // M7: exponential backoff + jitter (see connect_backoff.h) -- the engine
  // itself has no timer for how long to sit in Backoff, by design -- only
  // Connecting/Discovering carry a deadline (spec/transitions.json). The
  // delay is computed once per Backoff entry (backoff_started_at_ == 0),
  // from backoff_count() at that exact moment, so it does not keep growing
  // while merely waiting out a single already-latched delay.
  if (this->backoff_started_at_ == 0) {
    this->backoff_started_at_ = millis();
    this->current_backoff_delay_ms_ =
        nimble_ble::compute_connect_backoff_delay_ms(this->engine_.backoff_count(), random_uint32() % 100);
    ESP_LOGD(TAG, "[%s] backing off %ums (attempt %u)", this->address_str_,
             static_cast<unsigned>(this->current_backoff_delay_ms_), static_cast<unsigned>(this->engine_.backoff_count()));
  } else if (this->auto_connect_ && millis() - this->backoff_started_at_ >= this->current_backoff_delay_ms_) {
    this->backoff_started_at_ = 0;
    this->connect();
  }
}

void BLEClientBase::dump_config() {
  ESP_LOGCONFIG(TAG, "BLE Client:\n  Address: %s\n  Auto-connect: %s", this->address_str_, YESNO(this->auto_connect_));
}

void BLEClientBase::run_later(std::function<void()> &&f) { this->set_timeout(0, std::move(f)); }

void BLEClientBase::set_address(uint64_t address) {
  this->address_ = address;
  if (address == 0) {
    this->address_str_[0] = '\0';
    return;
  }
  uint8_t mac[6];
  ble_device_base::uint64_to_mac_msb_first(address, mac);
  snprintf(this->address_str_, sizeof(this->address_str_), "%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2],
           mac[3], mac[4], mac[5]);
}

void BLEClientBase::connect() {
  this->backoff_started_at_ = 0;
  this->set_state(ClientState::CONNECTING);
  int rc = this->engine_.state() == nimble_ble::BleConnState::BACKOFF
               ? this->engine_.retry_connect(this->address_, ble_device_base::BLE_ADDR_TYPE_PUBLIC)
               : this->engine_.connect(this->address_, ble_device_base::BLE_ADDR_TYPE_PUBLIC);
  if (rc != 0) {
    ESP_LOGW(TAG, "[%s] connect() failed to start: %d", this->address_str_, rc);
    this->set_state(ClientState::IDLE);
  }
}

void BLEClientBase::disconnect() {
  this->set_state(ClientState::DISCONNECTING);
  this->engine_.gatt_disconnect();
}

void BLEClientBase::on_connection_state(bool connected, uint16_t mtu, int error) {
  if (connected) {
    this->set_state(ClientState::CONNECTED);
    int rc = this->engine_.discover_services();
    if (rc != 0) {
      ESP_LOGW(TAG, "[%s] discover_services() failed to start: %d", this->address_str_, rc);
      this->engine_.gatt_disconnect();
    }
  } else {
    ESP_LOGD(TAG, "[%s] disconnected/failed to connect (error=%d)", this->address_str_, error);
    this->set_state(ClientState::IDLE);
  }
}

void BLEClientBase::on_service_discovery_done(int error) {
  if (error == 0) {
    this->set_state(ClientState::ESTABLISHED);
    return;
  }
  // A nonzero error here means discovery timed out or failed to start; the
  // engine's own loop() already tears the link down in the timeout case
  // (ble_gap_terminate), and the resulting DISCONNECT event brings
  // on_connection_state(false, ...) -> set_state(IDLE) through the normal
  // path above -- nothing further to do here.
  ESP_LOGW(TAG, "[%s] service discovery failed: %d", this->address_str_, error);
}

FoundCharacteristic BLEClientBase::get_characteristic(const ble_device_base::ESPBTUUID &service_uuid,
                                                       const ble_device_base::ESPBTUUID &char_uuid) {
  auto table = this->engine_.get_service_table();
  for (uint16_t s = 0; s < table.service_count; s++) {
    const auto &svc = table.services[s];
    if (!(svc.uuid == service_uuid))
      continue;
    for (uint16_t c = svc.first_characteristic; c < svc.first_characteristic + svc.characteristic_count; c++) {
      const auto &chr = table.characteristics[c];
      if (chr.uuid == char_uuid)
        return {chr.value_handle, chr.properties, true};
    }
  }
  return {};
}

}  // namespace esphome::esp32_ble_client

#endif
