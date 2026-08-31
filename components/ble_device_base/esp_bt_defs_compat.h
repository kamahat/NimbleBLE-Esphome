// Compat shim for the small Bluedroid-typed surface ble_device.h/.cpp still
// use under USE_ESP32 (ESPBTUUID::from_uuid/get_uuid, ESPBTDevice::
// get_address_type -- marked "historical esp32_ble API surface... never
// referenced off-esp32" in the real core header, but still compiled). The
// real esp_bt_defs.h lives on Bluedroid's ESP-IDF include path
// (bt/host/bluedroid/api/include/api/), unreachable once
// CONFIG_BT_BLUEDROID_ENABLED=n -- confirmed by ninja's own diagnostic.
// This file replaces the angle-bracket <esp_bt_defs.h> include with a
// same-directory quoted include so no global -I flag is required.
#pragma once

#include <cstdint>

typedef struct {
  uint8_t len;
  union {
    uint16_t uuid16;
    uint32_t uuid32;
    uint8_t uuid128[16];
  } uuid;
} esp_bt_uuid_t;

#define ESP_UUID_LEN_16 2
#define ESP_UUID_LEN_32 4
#define ESP_UUID_LEN_128 16

typedef enum {
  BLE_ADDR_TYPE_PUBLIC = 0x00,
  BLE_ADDR_TYPE_RANDOM = 0x01,
  BLE_ADDR_TYPE_RPA_PUBLIC = 0x02,
  BLE_ADDR_TYPE_RPA_RANDOM = 0x03,
} esp_ble_addr_type_t;
