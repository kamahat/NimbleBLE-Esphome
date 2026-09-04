// SURCHARGE de esphome/components/esp32_ble/ble.h.
//
// M1 : surface publique réduite au strict nécessaire pour "compile et
// advertise" -- pas encore les callbacks GAP/GATT (M2/M3), pas l'API
// d'advertising avancée (service_data/manufacturer_data/service_uuid,
// M2+). Voir components/esp32_ble/README.md et docs/OVERRIDE_CAVEATS.md
// pour l'écart exact avec le esp32_ble core actuel.
#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/automation.h"

#ifdef USE_ESP32

#include "esphome/components/nimble_ble/nimble_controller.h"

namespace esphome::esp32_ble {

class ESP32BLE final : public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;

  void enable();
  void disable();
  bool is_active() { return this->active_; }

  void set_name(const char *name) { this->name_ = name; }
  void set_enable_on_boot(bool enable_on_boot) { this->enable_on_boot_ = enable_on_boot; }
  void set_advertising(bool advertising) { this->advertising_wanted_ = advertising; }

  void get_mac_msb_first(uint8_t out[6]) const { this->controller_.get_mac_msb_first(out); }

  // Lets a peripheral-role consumer (esp32_ble_server) own advertising with
  // its own GAP callback (CONNECT/DISCONNECT/SUBSCRIBE), instead of M1's
  // plain advertise-only path. Documented scope limit
  // (docs/OVERRIDE_CAVEATS.md): combining esp32_ble_server with esp32_ble's
  // own advertising: true in the same build is not coordinated -- the GATT
  // server's advertising call simply takes over the one NimBLE advertising
  // set that exists.
  bool start_advertising_with_callback(ble_gap_event_fn *cb, void *cb_arg) {
    return this->controller_.start_advertising(cb, cb_arg);
  }

 protected:
  nimble_ble::NimbleController controller_;
  const char *name_{nullptr};
  bool enable_on_boot_{true};
  bool advertising_wanted_{false};
  bool active_{false};
};

extern ESP32BLE *global_ble;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

template<typename... Ts> class BLEEnabledCondition final : public Condition<Ts...> {
 public:
  bool check(const Ts &...x) override { return global_ble != nullptr && global_ble->is_active(); }
};

template<typename... Ts> class BLEEnableAction final : public Action<Ts...> {
 public:
  void play(const Ts &...x) override {
    if (global_ble != nullptr)
      global_ble->enable();
  }
};

template<typename... Ts> class BLEDisableAction final : public Action<Ts...> {
 public:
  void play(const Ts &...x) override {
    if (global_ble != nullptr)
      global_ble->disable();
  }
};

}  // namespace esphome::esp32_ble

#endif
