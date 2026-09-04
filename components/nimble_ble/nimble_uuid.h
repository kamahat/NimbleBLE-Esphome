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
#if defined(USE_BLE_GATT_CLIENT) || defined(USE_ESP32_BLE_SERVER)

#include "esphome/components/ble_device_base/ble_device.h"

#include "host/ble_uuid.h"

#include <cstring>

namespace esphome::nimble_ble {

#ifdef USE_BLE_GATT_CLIENT
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
#endif  // USE_BLE_GATT_CLIENT

#ifdef USE_ESP32_BLE_SERVER
/// Reverse direction, for the GATT server (esp32_ble_server): a user-
/// configured ESPBTUUID becomes a NimBLE ble_uuid_any_t for
/// ble_gatt_svc_def/ble_gatt_chr_def/ble_gatt_dsc_def. `out` must outlive
/// the registered service table (NimBLE keeps a pointer, not a copy).
inline void espbtuuid_to_nimble_uuid(const ble_device_base::ESPBTUUID &uuid, ble_uuid_any_t *out) {
  using ble_device_base::ESPBTUUID;
  switch (uuid.type()) {
    case ESPBTUUID::Type::UUID16:
      out->u16.u.type = BLE_UUID_TYPE_16;
      out->u16.value = uuid.uuid16();
      break;
    case ESPBTUUID::Type::UUID32:
      out->u32.u.type = BLE_UUID_TYPE_32;
      out->u32.value = uuid.uuid32();
      break;
    case ESPBTUUID::Type::UUID128:
    default:
      out->u128.u.type = BLE_UUID_TYPE_128;
      memcpy(out->u128.value, uuid.uuid128(), sizeof(out->u128.value));
      break;
  }
}
#endif  // USE_ESP32_BLE_SERVER

}  // namespace esphome::nimble_ble

#endif  // USE_BLE_GATT_CLIENT || USE_ESP32_BLE_SERVER
#endif  // USE_ESP32
