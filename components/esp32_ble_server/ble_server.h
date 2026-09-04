// ble_server.h -- SURCHARGE NimbleBLE-Esphome de esp32_ble_server/ble_server.h.
//
// Écart architectural assumé face au serveur Bluedroid : NimBLE enregistre
// une table STATIQUE de services/caractéristiques/descripteurs en un seul
// appel (ble_gatts_add_svcs() puis ble_gatts_start()), alors que Bluedroid
// crée chaque service de façon asynchrone (create -> add_char* -> start).
// Comme la configuration `esp32_ble_server:` est entièrement connue à la
// compilation (YAML), setup() construit la table complète une seule fois
// au lieu de reproduire la machine à états asynchrone du core -- voir
// docs/ARCHITECTURE.md.
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "ble_characteristic.h"
#include "ble_service.h"
#include "nimble_server_event.h"

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "host/ble_gap.h"

#include <functional>
#include <unordered_map>
#include <vector>

namespace esphome::esp32_ble_server {

using namespace esp32_ble;

class BLEServer final : public Component, public Parented<ESP32BLE> {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  bool is_running() const { return this->parent_->is_active() && this->started_; }

  void set_manufacturer_data(const std::vector<uint8_t> &data) { this->manufacturer_data_ = data; }
  void set_max_clients(uint8_t max_clients) { this->max_clients_ = max_clients; }
  uint8_t get_max_clients() const { return this->max_clients_; }

  BLEService *create_service(ble_device_base::ESPBTUUID uuid, bool advertise = false, uint16_t num_handles = 15);
  BLEService *get_service(ble_device_base::ESPBTUUID uuid, uint8_t inst_id = 0);

  uint8_t get_client_count() const { return this->client_count_; }

  void on_connect(std::function<void(uint16_t)> &&callback) {
    this->callbacks_.push_back({CallbackType::ON_CONNECT, std::move(callback)});
  }
  void on_disconnect(std::function<void(uint16_t)> &&callback) {
    this->callbacks_.push_back({CallbackType::ON_DISCONNECT, std::move(callback)});
  }

 protected:
  enum class CallbackType : uint8_t { ON_CONNECT, ON_DISCONNECT };
  struct CallbackEntry {
    CallbackType type;
    std::function<void(uint16_t)> callback;
  };

  void setup_gatt_server_();
  void restart_advertising_();
  void dispatch_callbacks_(CallbackType type, uint16_t conn_handle);
  int8_t find_client_index_(uint16_t conn_handle) const;

  static int gap_event_cb_(struct ble_gap_event *event, void *arg);
  int handle_gap_event_(struct ble_gap_event *event);

  std::vector<CallbackEntry> callbacks_;
  std::vector<uint8_t> manufacturer_data_;
  std::vector<BLEService *> services_;
  // Terminator included (services_.size() + 1); assembled once in
  // setup_gatt_server_() and never touched again -- NimBLE keeps a pointer
  // to this array for the device's lifetime.
  std::unique_ptr<ble_gatt_svc_def[]> svc_defs_;
  // Routes a BLE_GAP_EVENT_SUBSCRIBE (which only carries the value handle)
  // back to its owning characteristic. Populated once, read-only from then
  // on, so a plain map lookup from the GAP callback (host task) is safe --
  // no user code runs during the lookup itself.
  std::unordered_map<uint16_t, BLECharacteristic *> handle_to_characteristic_;

  static constexpr uint8_t MAX_CLIENTS = 9;  // mirrors esp32_ble's CONFIG_BT_NIMBLE_MAX_CONNECTIONS ceiling
  uint16_t clients_[MAX_CLIENTS]{};
  uint8_t client_count_{0};
  uint8_t max_clients_{1};
  bool started_{false};
};

extern BLEServer *global_ble_server;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::esp32_ble_server

#endif  // USE_ESP32
