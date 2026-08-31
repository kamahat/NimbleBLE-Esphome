// SURCHARGE de esphome/components/esp32_ble_tracker -- backend NimBLE.
//
// M2 : périmètre volontairement réduit -- pas de gestion de clients GATT
// (ESPHOME_ESP32_BLE_TRACKER_CLIENT_COUNT, arrive en M3), pas de
// coexistence tuning esp32-spécifique, pas d'intégration OTA. Voir
// docs/OVERRIDE_CAVEATS.md.
//
// Différence architecturale assumée par rapport au core Bluedroid :
// NimBLE n'a pas de callback GAP global unique -- chaque opération
// (ble_gap_disc, ble_gap_connect, ble_gap_adv_start) prend son propre
// callback. Ce tracker possède donc directement sa session de découverte
// NimBLE (host/ble_gap.h) au lieu de s'enregistrer sur un dispatch central
// esp32_ble comme le fait le core Bluedroid -- esp32_ble::register_gap_*
// n'existe donc pas dans notre surcharge de esp32_ble (voir son README).
//
// Puisque NimBLE délivre advertisement et scan response comme deux
// événements SEPARES (contrairement au contrôleur Bluedroid qui les
// fusionne déjà), on réutilise ble_device_base::ScanResponseMerger/
// AdvDispatcher -- exactement l'abstraction prévue pour ce cas (voir son
// commentaire d'en-tête : "trackers whose controller delivers advertisement
// and scan response as SEPARATE reports").
#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#ifdef USE_ESP32

#include "esphome/components/esp32_ble/ble.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/ble_device_base/ble_hub.h"
#include "esphome/components/ble_device_base/scan_response_merger.h"

#include "host/ble_gap.h"

namespace esphome::esp32_ble_tracker {

using namespace esp32_ble;
using adv_data_t = ble_device_base::adv_data_t;
using ESPBTDevice = ble_device_base::ESPBTDevice;
using ESPBTDeviceListener = ble_device_base::ESPBTDeviceListener;
using ScannerState = ble_device_base::ScannerState;

class ESP32BLETracker final : public Component, public Parented<ESP32BLE> {
 public:
  void set_scan_duration(uint32_t scan_duration) { this->scan_duration_ = scan_duration; }
  void set_scan_interval(uint32_t scan_interval) { this->scan_interval_ = scan_interval; }
  void set_scan_window(uint32_t scan_window) { this->scan_window_ = scan_window; }
  void set_scan_active(bool scan_active) { this->scan_active_ = scan_active; }
  bool get_scan_active() const { return this->scan_active_; }
  void set_scan_continuous(bool scan_continuous) { this->scan_continuous_ = scan_continuous; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void loop() override;

  // ---- ble_device_base::BLEHub contract (BLEHubContract concept) ----
  void register_listener(ble_device_base::ESPBTDeviceListener *listener) {
    this->dispatcher_.register_listener(listener);
  }
  void set_raw_advertisement_callback(ble_device_base::RawAdvertisementCallback callback) {
    this->dispatcher_.set_raw_advertisement_callback(callback);
  }
  static constexpr ble_device_base::HubCapabilities get_capabilities() {
    // merges_scan_response = true: ScanResponseMerger does the merging for
    // us, so consumers see the same merged-frame contract Bluedroid gives.
    return {/* active_scan = */ true, /* merges_scan_response = */ true, /* gatt = */ false,
            /* scan_mode_switch = */ false};
  }
  void get_adapter_mac(uint8_t out[MAC_ADDRESS_SIZE]) { this->parent_->get_mac_msb_first(out); }
  bool scan_running() { return this->scanner_state_ == ScannerState::RUNNING; }
  bool scan_active() { return this->scan_active_; }
  bool request_scan_mode(bool active) { return false; }

  void start_scan();
  void stop_scan();
  ScannerState get_scanner_state() const { return this->scanner_state_; }

 protected:
  static int gap_event_handler_(struct ble_gap_event *event, void *arg);
  int handle_gap_event_(struct ble_gap_event *event);
  void start_scan_();
  void stop_scan_();

  ble_device_base::AdvDispatcher dispatcher_;
  ble_device_base::ScanResponseMerger merger_;

  uint32_t scan_duration_{300};
  // Units: 0.625ms controller ticks (matches ble_device_base::to_ble_units).
  uint32_t scan_interval_{512};  // 320ms
  uint32_t scan_window_{48};     // 30ms
  bool scan_active_{true};
  bool scan_continuous_{true};
  ScannerState scanner_state_{ScannerState::IDLE};
};

extern ESP32BLETracker *global_esp32_ble_tracker;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::esp32_ble_tracker

#endif
