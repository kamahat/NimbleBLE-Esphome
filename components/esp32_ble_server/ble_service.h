// ble_service.h -- SURCHARGE NimbleBLE-Esphome de esp32_ble_server/ble_service.h.
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "ble_characteristic.h"
#include "esphome/components/ble_device_base/ble_device.h"

#include "host/ble_gatt.h"

#include <memory>
#include <vector>

namespace esphome::esp32_ble_server {

class BLEServer;

class BLEService final {
 public:
  BLEService(ble_device_base::ESPBTUUID uuid, uint16_t num_handles, uint8_t inst_id, bool advertise);
  ~BLEService();

  BLECharacteristic *create_characteristic(ble_device_base::ESPBTUUID uuid, uint32_t properties);

  ble_device_base::ESPBTUUID get_uuid() const { return this->uuid_; }
  uint8_t get_inst_id() const { return this->inst_id_; }
  bool get_advertise() const { return this->advertise_; }
  BLECharacteristic *get_last_created_characteristic() { return this->last_created_characteristic_; }

  BLEServer *get_server() { return this->server_; }

  /// Builds the native ble_gatt_svc_def entry (and recursively, every
  /// characteristic's/descriptor's). Called once by BLEServer while
  /// assembling the whole build's static service table -- see
  /// ble_server.cpp's setup_gatt_server_().
  void do_create(BLEServer *server);
  const ble_gatt_svc_def &native_def() const { return this->def_; }

 protected:
  friend class BLEServer;
  std::vector<BLECharacteristic *> characteristics_;
  BLECharacteristic *last_created_characteristic_{nullptr};
  BLEServer *server_{nullptr};
  ble_device_base::ESPBTUUID uuid_;
  ble_uuid_any_t nimble_uuid_{};
  uint16_t num_handles_;  // Bluedroid pre-allocation hint, unused by NimBLE; kept for API parity
  uint8_t inst_id_;
  bool advertise_{false};

  std::unique_ptr<ble_gatt_chr_def[]> chr_defs_;
  ble_gatt_svc_def def_{};
};

}  // namespace esphome::esp32_ble_server

#endif  // USE_ESP32
