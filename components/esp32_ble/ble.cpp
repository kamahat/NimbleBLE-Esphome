#include "ble.h"

#ifdef USE_ESP32

#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include <nvs_flash.h>

namespace esphome::esp32_ble {

static const char *const TAG = "esp32_ble.nimble";

void ESP32BLE::setup() {
  global_ble = this;

  esp_err_t err = nvs_flash_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nvs_flash_init failed: %d", err);
    this->mark_failed();
    return;
  }

  if (this->enable_on_boot_) {
    this->enable();
  }
}

void ESP32BLE::enable() {
  if (this->active_)
    return;

  const char *device_name = this->name_ != nullptr ? this->name_ : App.get_name().c_str();
  if (!this->controller_.setup(device_name)) {
    ESP_LOGE(TAG, "NimBLE controller/host bring-up failed");
    this->mark_failed();
    return;
  }
  this->active_ = true;

  if (this->advertising_wanted_) {
    if (!this->controller_.start_advertising()) {
      ESP_LOGE(TAG, "Failed to start advertising");
    }
  }
}

void ESP32BLE::disable() {
  if (!this->active_)
    return;
  this->controller_.stop_advertising();
  this->active_ = false;
}

void ESP32BLE::loop() {
  // M1: no GAP/GATT event queue yet (advertising only, no scan/GATT
  // consumers exist yet) -- M2 adds the event dispatch consumed by
  // esp32_ble_tracker, matching the queue architecture of the Bluedroid
  // original (see docs/ARCHITECTURE.md).
}

float ESP32BLE::get_setup_priority() const { return setup_priority::BLUETOOTH; }

void ESP32BLE::dump_config() {
  uint8_t mac[6];
  this->get_mac_msb_first(mac);
  ESP_LOGCONFIG(TAG,
                "BLE (NimBLE):\n"
                "  MAC address: %02X:%02X:%02X:%02X:%02X:%02X\n"
                "  Active: %s\n"
                "  Advertising: %s",
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], YESNO(this->active_),
                YESNO(this->controller_.is_advertising()));
}

ESP32BLE *global_ble = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::esp32_ble

#endif
