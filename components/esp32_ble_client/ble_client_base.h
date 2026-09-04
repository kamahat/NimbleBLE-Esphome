// ble_client_base.h -- SURCHARGE NimbleBLE-Esphome de esp32_ble_client.
//
// Reconstruction clean-room sur le moteur partagé nimble_ble::NimbleGattEngine
// (components/nimble_ble/nimble_gattc.h) plutôt que sur les callbacks Bluedroid
// esp_gattc_cb_event_t/esp_gap_ble_cb_event_t du core -- ce composant n'a donc
// ni gattc_event_handler ni gap_event_handler, et ne s'enregistre pas comme
// ESPBTClient auprès d'esp32_ble_tracker (NimBLE peut composer un
// ble_gap_connect() direct par adresse sans scan préalable -- voir
// docs/ARCHITECTURE.md, écart architectural déjà assumé en M2/M3). Portée
// réduite documentée dans docs/OVERRIDE_CAVEATS.md : pas de découverte du
// type d'adresse via scan (PUBLIC toujours supposé), pas de pairing/passkey.
//
// C'est ici que vit le timeout borné de découverte (via
// nimble_ble::BleConnectionFsm, embarquée dans NimbleGattEngine) -- le
// correctif direct du hang Bluedroid documenté dans docs/HARDWARE_VALIDATION.md.
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/ble_device_base/ble_client_state.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/nimble_ble/connect_backoff.h"
#include "esphome/components/nimble_ble/nimble_gattc.h"

#include <functional>

namespace esphome::esp32_ble_client {

using ble_device_base::ClientState;
using ble_device_base::ConnectionType;

/// A characteristic resolved by (service UUID, characteristic UUID) against
/// the connected peer's discovered service table. found=false means either
/// UUID was not present at the time of the lookup (services_discovered()
/// must be true first).
struct FoundCharacteristic {
  uint16_t value_handle{0};
  uint8_t properties{0};
  bool found{false};
};

class BLEClientBase : public Component, public ble_device_base::GattClientListener {
 public:
  void setup() override;
  void loop() override;
  float get_setup_priority() const override;
  void dump_config() override;

  void run_later(std::function<void()> &&f);  // NOLINT

  virtual void connect();
  virtual void disconnect();
  void unconditional_disconnect() { this->disconnect(); }

  bool connected() const { return this->state_ == ClientState::ESTABLISHED; }
  ClientState state() const { return this->state_; }
  virtual void set_state(ClientState st) { this->state_ = st; }

  void set_auto_connect(bool auto_connect) { this->auto_connect_ = auto_connect; }
  void set_connection_type(ConnectionType ct) { this->engine_.set_connection_type(ct); }

  virtual void set_address(uint64_t address);
  uint64_t get_address() const { return this->address_; }
  const char *address_str() const { return this->address_str_; }

  /// Direct access to the underlying engine for operations this base class
  /// does not itself wrap (raw reads/writes by handle, pairing, connection
  /// parameter updates) -- see nimble_gattc.h for the full surface.
  nimble_ble::NimbleGattEngine &engine() { return this->engine_; }

  bool services_discovered() const { return this->state_ == ClientState::ESTABLISHED; }
  FoundCharacteristic get_characteristic(const ble_device_base::ESPBTUUID &service_uuid,
                                         const ble_device_base::ESPBTUUID &char_uuid);

  // ble_device_base::GattClientListener overrides -- translate NimbleGattEngine
  // completions into the ClientState machine.
  void on_connection_state(bool connected, uint16_t mtu, int error) override;
  void on_service_discovery_done(int error) override;

 protected:
  nimble_ble::NimbleGattEngine engine_;
  uint64_t address_{0};
  char address_str_[18]{};
  ClientState state_{ClientState::IDLE};
  bool auto_connect_{false};
  // M7: exponential backoff + jitter (nimble_ble::compute_connect_backoff_delay_ms),
  // keyed off the engine's own BleConnectionFsm::backoff_count() -- see
  // connect_backoff.h. current_backoff_delay_ms_ is latched once per Backoff
  // entry (computed lazily in loop(), the first time backoff_started_at_ is 0)
  // so it does not get recomputed (and drift) every loop() tick while waiting.
  uint32_t backoff_started_at_{0};
  uint32_t current_backoff_delay_ms_{0};
};

}  // namespace esphome::esp32_ble_client

#endif
