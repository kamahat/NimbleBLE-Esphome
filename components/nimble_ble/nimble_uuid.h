// nimble_uuid.h -- NimBLE UUID bytes <-> ble_device_base::ESPBTUUID.
//
// Byte order: NimBLE's ble_uuid128_t.value is 16 bytes in Bluetooth
// over-the-air (little-endian) order, the same convention
// ble_device_base::ESPBTUUID::from_raw(const uint8_t*) already assumes (a
// plain memcpy, no reversal) -- confirmed empirically in M2, where adv
// manufacturer-data 128-bit UUIDs parsed via from_raw print correctly
// against a real iBeacon frame. So the 128-bit case below is a straight
// memcpy, not from_raw_reversed().
//
// Takes raw type+bytes rather than a NimBLE ble_uuid_t* because the producer
// side (nimble_gattc.cpp's disc_*_cb_ callbacks, on the NimBLE host task)
// copies a discovered UUID's type and bytes directly into a GattEvent to
// cross the thread-marshaling queue -- by the time this runs (on the
// ESPHome main loop), the original ble_uuid_t no longer exists.
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32
#ifdef USE_BLE_GATT_CLIENT

#include "esphome/components/ble_device_base/ble_device.h"

#include "host/ble_uuid.h"

#include <cstring>

namespace esphome::nimble_ble {

inline ble_device_base::ESPBTUUID nimble_raw_uuid_to_espbtuuid(uint8_t type, const uint8_t raw[16]) {
  switch (type) {
    case BLE_UUID_TYPE_16: {
      uint16_t value;
      memcpy(&value, raw, sizeof(value));
      return ble_device_base::ESPBTUUID::from_uint16(value);
    }
    case BLE_UUID_TYPE_32: {
      uint32_t value;
      memcpy(&value, raw, sizeof(value));
      return ble_device_base::ESPBTUUID::from_uint32(value);
    }
    case BLE_UUID_TYPE_128:
    default:
      return ble_device_base::ESPBTUUID::from_raw(raw);
  }
}

}  // namespace esphome::nimble_ble

#endif  // USE_BLE_GATT_CLIENT
#endif  // USE_ESP32
