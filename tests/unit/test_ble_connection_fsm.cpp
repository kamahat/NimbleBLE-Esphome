// Host-buildable unit test (no ESP-IDF): g++ -std=c++20 -I components/nimble_ble
// tests/unit/test_ble_connection_fsm.cpp components/nimble_ble/nimble_fsm/ble_connection_fsm.cpp
#include "nimble_fsm/ble_connection_fsm.h"

#include <cassert>
#include <cstdio>

using esphome::nimble_ble::BleConnectionFsm;
using esphome::nimble_ble::BleConnEvent;
using esphome::nimble_ble::BleConnState;

static int failures = 0;
#define CHECK(cond)                                                       \
  do {                                                                    \
    if (!(cond)) {                                                        \
      std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
      failures++;                                                         \
    }                                                                     \
  } while (0)

// The direct fix for the Bluedroid hang: Discovering must exit on its own
// deadline even if nothing external ever calls handle_event(DISCOVER_TIMEOUT).
static void test_bounded_discovery_timeout() {
  BleConnectionFsm fsm;
  CHECK(fsm.state() == BleConnState::IDLE);

  fsm.handle_event(BleConnEvent::CONNECT_REQUEST, 0);
  CHECK(fsm.state() == BleConnState::CONNECTING);
  CHECK(fsm.has_deadline());
  CHECK(fsm.deadline_ms() == 5000);

  fsm.handle_event(BleConnEvent::GAP_CONNECT_OK, 1000);
  CHECK(fsm.state() == BleConnState::DISCOVERING);
  CHECK(fsm.deadline_ms() == 1000 + 8000);

  // Well before the deadline: no self-timeout.
  CHECK(!fsm.poll_timeout(5000));
  CHECK(fsm.state() == BleConnState::DISCOVERING);

  // At/after the deadline: the FSM fires its own DISCOVER_TIMEOUT -> Backoff,
  // with zero external prompting beyond calling poll_timeout(now).
  CHECK(fsm.poll_timeout(9000));
  CHECK(fsm.state() == BleConnState::BACKOFF);
  CHECK(fsm.backoff_count() == 1);
}

static void test_connect_timeout_before_gap_ok() {
  BleConnectionFsm fsm;
  fsm.handle_event(BleConnEvent::CONNECT_REQUEST, 0);
  CHECK(fsm.poll_timeout(5000));
  CHECK(fsm.state() == BleConnState::BACKOFF);
  CHECK(fsm.backoff_count() == 1);
}

static void test_successful_path_resets_backoff() {
  BleConnectionFsm fsm;
  fsm.handle_event(BleConnEvent::CONNECT_REQUEST, 0);
  fsm.poll_timeout(5000);  // -> Backoff, backoff_count 1
  fsm.handle_event(BleConnEvent::BACKOFF_ELAPSED, 5000);
  CHECK(fsm.state() == BleConnState::IDLE);

  fsm.handle_event(BleConnEvent::CONNECT_REQUEST, 5000);
  fsm.handle_event(BleConnEvent::GAP_CONNECT_OK, 5100);
  fsm.handle_event(BleConnEvent::DISCOVER_DONE, 5200);
  CHECK(fsm.state() == BleConnState::READY);
  CHECK(!fsm.has_deadline());
  CHECK(fsm.backoff_count() == 0);

  fsm.handle_event(BleConnEvent::DISCONNECT_REQUEST, 6000);
  CHECK(fsm.state() == BleConnState::DISCONNECTING);
  fsm.handle_event(BleConnEvent::GAP_DISCONNECT_EVT, 6100);
  CHECK(fsm.state() == BleConnState::IDLE);
}

static void test_unsolicited_pairing_dropped_in_connecting() {
  BleConnectionFsm fsm;
  fsm.handle_event(BleConnEvent::CONNECT_REQUEST, 0);
  CHECK(fsm.handle_event(BleConnEvent::DROP_UNSOLICITED_PAIRING, 100));
  CHECK(fsm.state() == BleConnState::BACKOFF);
}

static void test_mismatched_event_is_noop() {
  BleConnectionFsm fsm;
  // DISCOVER_DONE while Idle: not in the transition table for Idle.
  CHECK(!fsm.handle_event(BleConnEvent::DISCOVER_DONE, 0));
  CHECK(fsm.state() == BleConnState::IDLE);
}

int main() {
  test_bounded_discovery_timeout();
  test_connect_timeout_before_gap_ok();
  test_successful_path_resets_backoff();
  test_unsolicited_pairing_dropped_in_connecting();
  test_mismatched_event_is_noop();
  if (failures == 0) {
    std::printf("All ble_connection_fsm tests passed.\n");
    return 0;
  }
  std::fprintf(stderr, "%d failure(s).\n", failures);
  return 1;
}
