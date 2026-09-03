// Test-only GattClientListener for tests/components/bluetooth_connection.
// Included via esphome:.includes: so the on_boot lambda can reach a real
// listener instance (a lambda alone cannot implement a virtual interface).
#pragma once

#include "esphome/components/ble_device_base/ble_gatt_client.h"
#include "esphome/core/log.h"

class TestGattListener : public esphome::ble_device_base::GattClientListener {
 public:
  void on_connection_state(bool connected, uint16_t mtu, int error) override {
    ESP_LOGI("m3_test", "on_connection_state: connected=%d mtu=%u error=%d", connected, mtu, error);
  }
  void on_service_discovery_done(int error) override {
    ESP_LOGI("m3_test", "on_service_discovery_done: error=%d", error);
  }
};

TestGattListener g_test_listener;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
