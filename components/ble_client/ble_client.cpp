#include "ble_client.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

namespace esphome::ble_client {

void BLEClient::loop() {
  esp32_ble_client::BLEClientBase::loop();
  for (auto *node : this->nodes_)
    node->loop();
}

void BLEClient::dump_config() { esp32_ble_client::BLEClientBase::dump_config(); }

void BLEClient::set_state(ble_device_base::ClientState st) {
  ble_device_base::ClientState prev = this->state();
  esp32_ble_client::BLEClientBase::set_state(st);
  if (st == ble_device_base::ClientState::ESTABLISHED && prev != ble_device_base::ClientState::ESTABLISHED) {
    for (auto *node : this->nodes_)
      node->on_ble_client_connected();
  } else if (st == ble_device_base::ClientState::IDLE && prev != ble_device_base::ClientState::IDLE) {
    // Covers both a real disconnect from ESTABLISHED and a connect/discover
    // attempt that never got there (bounded timeout, gap_connect_fail): a
    // node only needs to know the attempt is over, not which of those it was.
    for (auto *node : this->nodes_)
      node->on_ble_client_disconnected();
  }
}

}  // namespace esphome::ble_client

#endif
