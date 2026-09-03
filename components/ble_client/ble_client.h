// ble_client.h -- SURCHARGE NimbleBLE-Esphome de ble_client (surface YAML
// `ble_client:`). BLEClientNode simplifié par rapport au core : pas de
// gattc_event_handler/gap_event_handler Bluedroid -- un noeud reçoit
// on_ble_client_connected()/on_ble_client_disconnected(), traduits par
// BLEClient::set_state() depuis la ClientState neutre (ble_device_base),
// elle-même pilotée par esp32_ble_client::BLEClientBase sur le moteur
// nimble_ble::NimbleGattEngine. Voir docs/OVERRIDE_CAVEATS.md pour ce qui
// n'est délibérément pas repris (pairing/passkey, ble_client.ble_write).
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/components/esp32_ble_client/ble_client_base.h"

#include <vector>

namespace esphome::ble_client {

class BLEClient;

class BLEClientNode {
 public:
  virtual void on_ble_client_connected() {}
  virtual void on_ble_client_disconnected() {}
  virtual void loop() {}
  void set_ble_client_parent(BLEClient *parent) { this->parent_ = parent; }
  BLEClient *parent() { return this->parent_; }

 protected:
  BLEClient *parent_{nullptr};
};

class BLEClient final : public esp32_ble_client::BLEClientBase {
 public:
  void loop() override;
  void dump_config() override;

  void register_ble_node(BLEClientNode *node) {
    node->set_ble_client_parent(this);
    this->nodes_.push_back(node);
  }

  void set_state(ble_device_base::ClientState st) override;

 protected:
  std::vector<BLEClientNode *> nodes_;
};

}  // namespace esphome::ble_client

#endif
