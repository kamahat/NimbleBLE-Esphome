// SURCHARGE de esphome/components/esp32_ble/ble_scan_result.h -- version sans
// dépendance Bluedroid (pas d'include esp_gap_ble_api.h). Champs identiques
// (mêmes noms/tailles) à ce que ble_device_base/ble_device.cpp::parse_scan_rst
// lit réellement (confirmé par lecture directe du code core) : bda, rssi,
// ble_addr_type, ble_adv, adv_data_len, scan_rsp_len. search_evt conservé
// pour compat de forme mais pas encore utilisé (pas de tracker en M1).
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include <cstdint>

namespace esphome::esp32_ble {

// Legacy BLE advertising cap: 31-byte adv + 31-byte scan response.
static constexpr uint8_t MAX_ADV_DATA_LEN = 31;

struct __attribute__((packed)) BLEScanResult {
  uint8_t bda[6];
  uint8_t ble_addr_type;
  int8_t rssi;
  uint8_t ble_adv[MAX_ADV_DATA_LEN * 2];
  uint8_t adv_data_len;
  uint8_t scan_rsp_len;
  uint8_t search_evt;
};

}  // namespace esphome::esp32_ble

#endif
