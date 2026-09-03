// bluetooth_connection_nimble.h -- the build's ble_device_base::BLEGattConnection
// when USE_ESP32_BLE_NIMBLE is set (bound in bluetooth_connection_gatt_backend.h).
//
// Thin Component wrapper: all the real connect/discover/read/write logic
// lives in the shared nimble_ble::NimbleGattEngine (also reusable by
// esp32_ble_client's legacy path, not yet ported -- see docs/ARCHITECTURE.md
// M3 status). This class only adds what the contract needs beyond the
// engine itself: being a real Component so new_gatt_backend() can register
// it and get periodic loop() calls.
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_BLE_GATT_CLIENT
#ifdef USE_ESP32_BLE_NIMBLE

#include "esphome/core/component.h"
#include "esphome/components/nimble_ble/nimble_gattc.h"

namespace esphome::bluetooth_connection {

class NimbleGattClient : public Component, public nimble_ble::NimbleGattEngine {
 public:
  void setup() override {}
  void loop() override { nimble_ble::NimbleGattEngine::loop(); }
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }
};

}  // namespace esphome::bluetooth_connection

#endif  // USE_ESP32_BLE_NIMBLE
#endif  // USE_BLE_GATT_CLIENT
