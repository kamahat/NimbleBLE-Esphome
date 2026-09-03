// GENERATED FILE -- do not edit by hand.
// Source of truth: spec/transitions.json (see tools/gen_state_machine/gen_cpp.py).
// Re-run gen_cpp.py after editing the spec; spec/ble_state_machine.tla (gen_tla.py)
// must be regenerated from the same spec so the verified model and this engine
// cannot silently diverge (docs/ARCHITECTURE.md, state machine section).
//
// One instance tracks one BLE connection slot's Connecting/Discovering deadline
// discipline -- the direct fix for the Bluedroid hang this project exists to
// replace (no deadline today: esp_ble_gattc_search_service() never times out
// on its own, see docs/HARDWARE_VALIDATION.md).
#pragma once

#include <cstdint>

namespace esphome::nimble_ble {

enum class BleConnState : uint8_t {
  IDLE,
  SCANNING,
  CONNECTING,
  DISCOVERING,
  READY,
  DISCONNECTING,
  BACKOFF,
};

enum class BleConnEvent : uint8_t {
  SCAN_START,
  ADV_MATCHED,
  CONNECT_REQUEST,
  GAP_CONNECT_OK,
  GAP_CONNECT_FAIL,
  CONNECT_TIMEOUT,
  DISCOVER_START,
  DISCOVER_DONE,
  DISCOVER_TIMEOUT,
  DISCONNECT_REQUEST,
  GAP_DISCONNECT_EVT,
  BACKOFF_ELAPSED,
  DROP_UNSOLICITED_PAIRING,
};

const char *ble_conn_state_name(BleConnState state);
const char *ble_conn_event_name(BleConnEvent event);

/// One connection slot's state machine, generated from spec/transitions.json.
/// Not thread-safe: drive it only from the ESPHome main loop (nimble_gattc
/// marshals NimBLE host-task callbacks onto the loop via nimble_event first).
class BleConnectionFsm {
 public:
  BleConnState state() const { return this->state_; }
  /// True in Connecting/Discovering only -- the only states with a deadline.
  bool has_deadline() const;
  uint32_t deadline_ms() const { return this->deadline_ms_; }
  /// Consecutive failed connection attempts for this slot's address; reset on
  /// a successful Ready entry, incremented on every entry into Backoff.
  uint16_t backoff_count() const { return this->backoff_count_; }

  /// Applies `event` at logical time `now_ms` per the transition table.
  /// Returns true if it matched a transition for the current state (a
  /// mismatched event is a documented no-op, not an error: NimBLE delivers
  /// some events unconditionally regardless of which state issued the
  /// request that is completing).
  bool handle_event(BleConnEvent event, uint32_t now_ms);

  /// Call periodically (e.g. from loop()). If the current state carries a
  /// deadline and now_ms has reached it, applies the matching *_TIMEOUT event
  /// itself and returns true. This is what makes BoundedWait a runtime
  /// guarantee and not just a spec property: nothing external has to remember
  /// to call handle_event(*_TIMEOUT) -- the FSM enforces its own deadline.
  bool poll_timeout(uint32_t now_ms);

 protected:
  void enter_(BleConnState state, uint32_t now_ms, uint32_t deadline_ms);

  BleConnState state_{BleConnState::IDLE};
  uint32_t deadline_ms_{0};
  uint16_t backoff_count_{0};
};

}  // namespace esphome::nimble_ble
