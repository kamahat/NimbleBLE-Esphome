// connect_backoff.h -- M7 hardening: exponential backoff + jitter for BLE
// reconnect attempts.
//
// Fixes the fl4p audit finding (docs/SECURITY.md): no rate-limiting on
// reconnect attempts lets a failing/out-of-range peripheral hammer the
// shared radio/scan resource indefinitely at a fixed cadence.
//
// Deliberately placed here (components/nimble_ble/), not under
// components/bluetooth_proxy/ as the original project plan sketched before
// the client-vs-proxy split was implemented: bluetooth_proxy has no
// autonomous reconnect loop of its own -- every BluetoothGATTConnect request
// is client-initiated (Home Assistant decides when to retry). The only
// self-driven "keep trying forever" loop in this codebase is
// esp32_ble_client::BLEClientBase's auto_connect, which is what this backs.
//
// Keyed off BleConnectionFsm's own backoff_count() (see ble_connection_fsm.h)
// rather than a separate per-address map: one BLEClientBase/NimbleGattEngine
// instance already corresponds to exactly one configured address, so the
// FSM's own counter already *is* "per address".
#pragma once

#include <cstdint>

namespace esphome::nimble_ble {

constexpr uint32_t CONNECT_BACKOFF_BASE_MS = 5000;
constexpr uint32_t CONNECT_BACKOFF_MAX_MS = 60000;
constexpr uint8_t CONNECT_BACKOFF_JITTER_PERCENT = 20;

/// Pure function (no internal RNG call) so it stays host-testable
/// deterministically -- the caller supplies the random draw, e.g.
/// `esphome::random_uint32() % 100`.
///
/// backoff_count is BleConnectionFsm::backoff_count() at the moment Backoff
/// was entered: 1 on the first failure (never 0 while actually in Backoff,
/// since the FSM increments on entry), so the first retry keeps the
/// already-tuned 5000ms base delay from before this hardening pass.
/// Returned delay is base * 2^(backoff_count-1), capped at
/// CONNECT_BACKOFF_MAX_MS, then jittered by +/-CONNECT_BACKOFF_JITTER_PERCENT%
/// (so many devices backing off at the same time don't all retry in lockstep).
uint32_t compute_connect_backoff_delay_ms(uint16_t backoff_count, uint32_t jitter_random_0_99);

}  // namespace esphome::nimble_ble
