// Host-buildable unit test (no ESP-IDF): g++ -std=c++20 -I components/nimble_ble
// tests/unit/test_ble_connection_fsm.cpp components/nimble_ble/ble_connection_fsm.cpp
#include "ble_connection_fsm.h"

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

// -- M6: trace replay against TLC-explored counterexamples ------------------
//
// TLC's own model (spec/ble_state_machine.tla, generated from the same
// spec/transitions.json as this engine -- see tools/gen_state_machine/) uses
// small abstract tick deadlines (2/3), not the real 5000ms/8000ms, to keep
// the explored state space tractable. These two tests replay the *shape* of
// two real TLC counterexample traces -- extracted by asserting a
// deliberately false invariant and reading off the resulting behavior, the
// standard TLA+ idiom for pulling a concrete positive trace out of a model
// checker -- against the compiled engine with its real deadlines, checking
// that the same event sequence produces the same state sequence.

// Trace 1: forced by asserting NeverReady == state # "Ready" (a Ready state
// is of course reachable; TLC's shortest counterexample, depth 4):
//   State 1: state=Idle
//   State 2: <ConnectRequestFromIdle>    state=Connecting
//   State 3: <GapConnectOkFromConnecting> state=Discovering
//   State 4: <DiscoverDoneFromDiscovering> state=Ready
static void test_tlc_trace_happy_path() {
  BleConnectionFsm fsm;
  CHECK(fsm.state() == BleConnState::IDLE);
  fsm.handle_event(BleConnEvent::CONNECT_REQUEST, 0);
  CHECK(fsm.state() == BleConnState::CONNECTING);
  fsm.handle_event(BleConnEvent::GAP_CONNECT_OK, 100);
  CHECK(fsm.state() == BleConnState::DISCOVERING);
  fsm.handle_event(BleConnEvent::DISCOVER_DONE, 200);
  CHECK(fsm.state() == BleConnState::READY);
}

// Trace 2: forced by asserting NeverConnectTimeout == last_event #
// "connect_timeout" (depth 5; the two Tick steps are TLC advancing its
// abstract clock up to the deadline -- poll_timeout(now) below is the real
// engine's equivalent of "let time pass, then check"):
//   State 1: state=Idle
//   State 2: <ConnectRequestFromIdle>   state=Connecting, deadline=clock+2
//   State 3: <Tick>                     clock=1
//   State 4: <Tick>                     clock=2 (== deadline)
//   State 5: <ConnectTimeoutFromConnecting> state=Backoff, backoff_count=1
static void test_tlc_trace_connect_timeout() {
  BleConnectionFsm fsm;
  fsm.handle_event(BleConnEvent::CONNECT_REQUEST, 0);
  CHECK(fsm.state() == BleConnState::CONNECTING);
  CHECK(fsm.deadline_ms() == 5000);
  CHECK(!fsm.poll_timeout(4999));
  CHECK(fsm.state() == BleConnState::CONNECTING);
  CHECK(fsm.poll_timeout(5000));
  CHECK(fsm.state() == BleConnState::BACKOFF);
  CHECK(fsm.backoff_count() == 1);
}

int main() {
  test_bounded_discovery_timeout();
  test_connect_timeout_before_gap_ok();
  test_successful_path_resets_backoff();
  test_unsolicited_pairing_dropped_in_connecting();
  test_mismatched_event_is_noop();
  test_tlc_trace_happy_path();
  test_tlc_trace_connect_timeout();
  if (failures == 0) {
    std::printf("All ble_connection_fsm tests passed.\n");
    return 0;
  }
  std::fprintf(stderr, "%d failure(s).\n", failures);
  return 1;
}
