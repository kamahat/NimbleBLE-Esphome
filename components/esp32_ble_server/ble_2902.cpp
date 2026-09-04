#include "ble_2902.h"

#ifdef USE_ESP32

namespace esphome::esp32_ble_server {

BLE2902::BLE2902() : BLEDescriptor(ble_device_base::ESPBTUUID::from_uint16(0x2902), 2) {}

}  // namespace esphome::esp32_ble_server

#endif
