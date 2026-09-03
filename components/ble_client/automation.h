// automation.h -- SURCHARGE NimbleBLE-Esphome de ble_client/automation.h.
// Triggers/actions simplifiés : pas de pairing/passkey, pas de ble_write
// déclaratif (voir docs/OVERRIDE_CAVEATS.md et engine()->write_characteristic()
// pour l'équivalent via lambda).
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/core/automation.h"
#include "ble_client.h"

namespace esphome::ble_client {

class BLEClientConnectTrigger final : public Trigger<>, public BLEClientNode {
 public:
  explicit BLEClientConnectTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void on_ble_client_connected() override { this->trigger(); }
};

class BLEClientDisconnectTrigger final : public Trigger<>, public BLEClientNode {
 public:
  explicit BLEClientDisconnectTrigger(BLEClient *parent) { parent->register_ble_node(this); }
  void on_ble_client_disconnected() override { this->trigger(); }
};

// Fire-and-forget for v1: unlike the core's play_complex()-based actions,
// these do not wait for the connect/disconnect to actually finish before
// continuing the calling automation (documented simplification, see
// docs/OVERRIDE_CAVEATS.md). on_connect/on_disconnect above are the way to
// react to the actual outcome.
template<typename... Ts> class BLEClientConnectAction final : public Action<Ts...> {
 public:
  explicit BLEClientConnectAction(BLEClient *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->connect(); }

 protected:
  BLEClient *parent_;
};

template<typename... Ts> class BLEClientDisconnectAction final : public Action<Ts...> {
 public:
  explicit BLEClientDisconnectAction(BLEClient *parent) : parent_(parent) {}
  void play(const Ts &...x) override { this->parent_->disconnect(); }

 protected:
  BLEClient *parent_;
};

}  // namespace esphome::ble_client

#endif
