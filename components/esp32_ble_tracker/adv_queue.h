// adv_queue.h -- M7 hardening: thread-safe marshaling of raw advertisement
// reports from the NimBLE host task to the ESPHome main loop.
//
// Real bug this fixes (found by reading handle_gap_event_ closely, not by
// following the original project plan's checklist): BLE_GAP_EVENT_DISC fired
// straight into ScanResponseMerger::stash_adv()/submit_scan_rsp() and
// AdvDispatcher::dispatch() *directly from the GAP callback*, which runs on
// the NimBLE host task -- every other host-task callback in this codebase
// (nimble_event.h, nimble_server_event.h) marshals through a queue first
// because touching arbitrary consumer code (a registered ESPBTDeviceListener,
// which for bluetooth_proxy means calling into the ESPHome API/socket layer)
// from a task other than the main loop is a data race, not merely a style
// preference. This queue closes that gap for advertisements the same way.
//
// Also directly addresses the fl4p audit finding in docs/SECURITY.md ("drops
// d'advertisements sous charge sans compteur exposé"): bounded capacity, and
// dropped_count() is public so a template sensor can expose it as an HA
// diagnostic entity (see the component's README for the YAML snippet).
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome::esp32_ble_tracker {

enum class AdvQueueEventType : uint8_t {
  SCAN_RSP,       // -> ScanResponseMerger::submit_scan_rsp()
  STASH_ADV,      // -> ScanResponseMerger::stash_adv()
  DIRECT,         // -> AdvDispatcher::dispatch() (non-scannable, no response coming)
  SCAN_COMPLETE,  // -> ScanResponseMerger::flush() + AdvDispatcher::on_scan_end()
};

/// One marshaled event. `data`/`data_len` are unused (nullptr/0) for
/// SCAN_COMPLETE. `data` is heap-owned; the consumer (ESP32BLETracker::loop())
/// must delete[] it after use.
struct AdvQueueEvent {
  AdvQueueEventType type;
  uint8_t mac[6]{};
  int8_t rssi{0};
  uint8_t addr_type{0};
  uint8_t *data{nullptr};
  uint8_t data_len{0};
};

class AdvQueue {
 public:
  void init(int capacity);
  /// Called from the NimBLE host task (the GAP callback). Never blocks;
  /// drops the event and bumps dropped_count_ on overflow instead -- a scan
  /// flooded with advertisements must degrade by dropping some, not by
  /// blocking the host task (which would stall NimBLE's own event
  /// processing for every connection this build has, not just scanning).
  void push_from_host_task(const AdvQueueEvent &event);
  /// Called from the ESPHome main loop. Returns false when empty.
  bool pop(AdvQueueEvent *out);
  /// Diagnostic counter (docs/SECURITY.md) -- how many advertisement reports
  /// have been dropped since boot because the queue was full. Expose via a
  /// template sensor reading this through the tracker (see README.md).
  uint32_t dropped_count() const { return this->dropped_count_; }

 protected:
  QueueHandle_t queue_{nullptr};
  volatile uint32_t dropped_count_{0};
};

}  // namespace esphome::esp32_ble_tracker

#endif  // USE_ESP32
