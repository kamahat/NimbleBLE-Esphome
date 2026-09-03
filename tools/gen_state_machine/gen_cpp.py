"""Generates components/nimble_ble/nimble_fsm/ble_connection_fsm.h/.cpp from
spec/transitions.json. Do not hand-edit the generated files -- re-run this
script after editing the spec.
"""
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent.parent
SPEC = json.loads((ROOT / "spec" / "transitions.json").read_text(encoding="utf-8"))
OUT_DIR = ROOT / "components" / "nimble_ble" / "nimble_fsm"


def screaming(name: str) -> str:
    return name.upper()


def pascal(name: str) -> str:
    return "".join(part.capitalize() for part in name.split("_"))


states = SPEC["states"]
events = SPEC["events"]
transitions = SPEC["transitions"]

header = f"""// GENERATED FILE -- do not edit by hand.
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

namespace esphome::nimble_ble {{

enum class BleConnState : uint8_t {{
"""
for s in states:
    header += f"  {screaming(s)},\n"
header += "};\n\n"

header += "enum class BleConnEvent : uint8_t {\n"
for e in events:
    header += f"  {screaming(e)},\n"
header += "};\n\n"

header += """const char *ble_conn_state_name(BleConnState state);
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
"""

cpp = """// GENERATED FILE -- do not edit by hand. See ble_connection_fsm.h.
#include "ble_connection_fsm.h"

namespace esphome::nimble_ble {

const char *ble_conn_state_name(BleConnState state) {
  switch (state) {
"""
for s in states:
    cpp += f'    case BleConnState::{screaming(s)}:\n      return "{s}";\n'
cpp += """    default:
      return "?";
  }
}

const char *ble_conn_event_name(BleConnEvent event) {
  switch (event) {
"""
for e in events:
    cpp += f'    case BleConnEvent::{screaming(e)}:\n      return "{e}";\n'
cpp += """    default:
      return "?";
  }
}

bool BleConnectionFsm::has_deadline() const {
  return this->state_ == BleConnState::CONNECTING || this->state_ == BleConnState::DISCOVERING;
}

void BleConnectionFsm::enter_(BleConnState state, uint32_t now_ms, uint32_t deadline_ms) {
  if (state == BleConnState::BACKOFF) {
    this->backoff_count_++;
  } else if (state == BleConnState::READY) {
    this->backoff_count_ = 0;
  }
  this->state_ = state;
  // Deadlines are absolute (now_ms + the spec's relative set_deadline_ms), so
  // poll_timeout() is a single unconditional comparison -- no per-state "when
  // did I enter" bookkeeping needed beyond this one field.
  this->deadline_ms_ = deadline_ms == 0 ? 0 : now_ms + deadline_ms;
}

bool BleConnectionFsm::handle_event(BleConnEvent event, uint32_t now_ms) {
  switch (this->state_) {
"""

by_state = {}
for t in transitions:
    by_state.setdefault(t["from"], []).append(t)

for s in states:
    cpp += f"    case BleConnState::{screaming(s)}:\n"
    rows = by_state.get(s, [])
    if not rows:
        cpp += "      return false;\n"
        continue
    cpp += "      switch (event) {\n"
    for t in rows:
        deadline = t.get("set_deadline_ms", 0)
        cpp += f"        case BleConnEvent::{screaming(t['event'])}:\n"
        cpp += f"          this->enter_(BleConnState::{screaming(t['to'])}, now_ms, {deadline});\n"
        cpp += "          return true;\n"
    cpp += "        default:\n          return false;\n      }\n"

cpp += """    default:
      return false;
  }
}

bool BleConnectionFsm::poll_timeout(uint32_t now_ms) {
  if (!this->has_deadline() || this->deadline_ms_ == 0 || now_ms < this->deadline_ms_)
    return false;
  BleConnEvent timeout_event =
      this->state_ == BleConnState::CONNECTING ? BleConnEvent::CONNECT_TIMEOUT : BleConnEvent::DISCOVER_TIMEOUT;
  return this->handle_event(timeout_event, now_ms);
}

}  // namespace esphome::nimble_ble
"""

OUT_DIR.mkdir(parents=True, exist_ok=True)
(OUT_DIR / "ble_connection_fsm.h").write_text(header, encoding="utf-8")
(OUT_DIR / "ble_connection_fsm.cpp").write_text(cpp, encoding="utf-8")
print("Generated ble_connection_fsm.h/.cpp OK")
