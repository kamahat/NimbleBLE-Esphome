// M1 scope: controller/host bring-up + advertising only. GAP/GATT event
// plumbing into ESP32BLE's existing BLEEvent queue is M2+ (scan) and M3
// (GATT), not implemented here yet -- see docs/ARCHITECTURE.md milestones.
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include <cstdint>

namespace esphome::nimble_ble {

class NimbleController {
 public:
  // Brings up the NimBLE controller + host (nimble_port_init +
  // nimble_port_freertos_init) and blocks until ble_hs_cfg.sync_cb fires
  // (host synced with controller) or a timeout elapses. device_name is
  // applied via ble_svc_gap_device_name_set once synced.
  bool setup(const char *device_name);

  bool start_advertising();
  bool stop_advertising();
  bool is_advertising() const { return this->advertising_; }

  void get_mac_msb_first(uint8_t out[6]) const;

 protected:
  static void on_sync_();
  static void on_reset_(int reason);
  static void host_task_(void *param);

  static volatile bool synced_;
  bool advertising_{false};
  const char *device_name_{nullptr};
};

}  // namespace esphome::nimble_ble

#endif
