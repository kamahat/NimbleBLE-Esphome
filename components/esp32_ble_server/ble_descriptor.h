// ble_descriptor.h -- SURCHARGE NimbleBLE-Esphome de esp32_ble_server/ble_descriptor.h.
//
// Reconstruction clean-room sur les API GATT serveur NimBLE brutes
// (host/ble_gatt.h : ble_gatt_dsc_def, ble_gatts_add_svcs) plutôt que sur
// esp_gatts_api.h Bluedroid. Contrairement au serveur Bluedroid (création
// asynchrone service par service), NimBLE enregistre une table statique de
// services/caractéristiques/descripteurs en un seul appel -- do_create() ici
// ne fait donc que construire la structure ble_gatt_dsc_def native ; c'est
// BLECharacteristic/BLEService/BLEServer qui assemblent la table complète et
// l'enregistrent (voir ble_server.cpp).
//
// La CCCD (0x2902) est gérée automatiquement par NimBLE dès qu'une
// caractéristique porte le flag notify/indicate ("Do not include CCCD; it
// gets added automatically" -- host/ble_gatt.h) : BLE2902 reste une classe
// de compatibilité API pour ble_server_automations.h, mais si un utilisateur
// déclare explicitement un descripteur 0x2902 dans sa config, il est détecté
// et exclu de la table NimBLE réelle (voir BLECharacteristic::do_create) --
// documenté dans docs/OVERRIDE_CAVEATS.md.
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/ble_device_base/ble_device.h"

#include "host/ble_gatt.h"
#include "host/ble_uuid.h"

#include <freertos/FreeRTOS.h>

#include <functional>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace esphome::esp32_ble_server {

class BLECharacteristic;

class BLEDescriptor {
 public:
  BLEDescriptor(ble_device_base::ESPBTUUID uuid, uint16_t max_len = 100, bool read = true, bool write = true);
  virtual ~BLEDescriptor() = default;

  ble_device_base::ESPBTUUID get_uuid() const { return this->uuid_; }

  void set_value(std::vector<uint8_t> &&buffer);
  void set_value(std::initializer_list<uint8_t> data) { this->set_value(std::vector<uint8_t>(data)); }
  void set_value(const std::string &buffer) { this->set_value(std::vector<uint8_t>(buffer.begin(), buffer.end())); }

  void on_write(std::function<void(std::span<const uint8_t>, uint16_t)> &&callback) {
    this->on_write_callback_ =
        std::make_unique<std::function<void(std::span<const uint8_t>, uint16_t)>>(std::move(callback));
  }

  bool is_created() const { return this->created_; }
  // NimBLE registration failures surface once, at ble_gatts_add_svcs()/
  // ble_gatts_start() time (BLEServer), not per-descriptor -- kept for API
  // parity with the reference (ble_server_automations.h does not call this,
  // but future consumers matching the core's shape may).
  bool is_failed() const { return false; }

  /// Builds the native ble_gatt_dsc_def entry. Called by BLECharacteristic
  /// while assembling its own characteristic's descriptor array -- not
  /// itself a NimBLE API call (that happens once, for the whole table, in
  /// BLEServer::setup_gatt_server_()).
  void do_create(BLECharacteristic *characteristic);
  const ble_gatt_dsc_def &native_def() const { return this->def_; }
  uint16_t get_uuid16_if_short() const;  // 0 if this is a 128-bit UUID

  /// Called by BLEServer::loop() draining a DSC_WRITE event targeting this
  /// descriptor.
  void handle_write_from_queue_(const uint8_t *data, uint16_t len, uint16_t conn_handle) {
    if (this->on_write_callback_)
      (*this->on_write_callback_)(std::span<const uint8_t>(data, len), conn_handle);
  }

 protected:
  friend class BLECharacteristic;
  static int access_cb_(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
  int handle_access_(uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt);

  BLECharacteristic *characteristic_{nullptr};
  ble_device_base::ESPBTUUID uuid_;
  ble_uuid_any_t nimble_uuid_{};
  ble_gatt_dsc_def def_{};
  uint16_t max_len_;
  uint8_t att_flags_;
  bool created_{false};

  std::vector<uint8_t> value_;
  portMUX_TYPE value_mux_ = portMUX_INITIALIZER_UNLOCKED;

  std::unique_ptr<std::function<void(std::span<const uint8_t>, uint16_t)>> on_write_callback_;
};

}  // namespace esphome::esp32_ble_server

#endif  // USE_ESP32
