// GENERATED FILE -- do not edit by hand. See ble_connection_fsm.h.
#include "ble_connection_fsm.h"

namespace esphome::nimble_ble {

const char *ble_conn_state_name(BleConnState state) {
  switch (state) {
    case BleConnState::IDLE:
      return "Idle";
    case BleConnState::SCANNING:
      return "Scanning";
    case BleConnState::CONNECTING:
      return "Connecting";
    case BleConnState::DISCOVERING:
      return "Discovering";
    case BleConnState::READY:
      return "Ready";
    case BleConnState::DISCONNECTING:
      return "Disconnecting";
    case BleConnState::BACKOFF:
      return "Backoff";
    default:
      return "?";
  }
}

const char *ble_conn_event_name(BleConnEvent event) {
  switch (event) {
    case BleConnEvent::SCAN_START:
      return "scan_start";
    case BleConnEvent::ADV_MATCHED:
      return "adv_matched";
    case BleConnEvent::CONNECT_REQUEST:
      return "connect_request";
    case BleConnEvent::GAP_CONNECT_OK:
      return "gap_connect_ok";
    case BleConnEvent::GAP_CONNECT_FAIL:
      return "gap_connect_fail";
    case BleConnEvent::CONNECT_TIMEOUT:
      return "connect_timeout";
    case BleConnEvent::DISCOVER_START:
      return "discover_start";
    case BleConnEvent::DISCOVER_DONE:
      return "discover_done";
    case BleConnEvent::DISCOVER_TIMEOUT:
      return "discover_timeout";
    case BleConnEvent::DISCONNECT_REQUEST:
      return "disconnect_request";
    case BleConnEvent::GAP_DISCONNECT_EVT:
      return "gap_disconnect_evt";
    case BleConnEvent::BACKOFF_ELAPSED:
      return "backoff_elapsed";
    case BleConnEvent::DROP_UNSOLICITED_PAIRING:
      return "drop_unsolicited_pairing";
    default:
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
    case BleConnState::IDLE:
      switch (event) {
        case BleConnEvent::SCAN_START:
          this->enter_(BleConnState::SCANNING, now_ms, 0);
          return true;
        case BleConnEvent::CONNECT_REQUEST:
          this->enter_(BleConnState::CONNECTING, now_ms, 5000);
          return true;
        default:
          return false;
      }
    case BleConnState::SCANNING:
      switch (event) {
        case BleConnEvent::ADV_MATCHED:
          this->enter_(BleConnState::IDLE, now_ms, 0);
          return true;
        default:
          return false;
      }
    case BleConnState::CONNECTING:
      switch (event) {
        case BleConnEvent::GAP_CONNECT_OK:
          this->enter_(BleConnState::DISCOVERING, now_ms, 8000);
          return true;
        case BleConnEvent::GAP_CONNECT_FAIL:
          this->enter_(BleConnState::BACKOFF, now_ms, 0);
          return true;
        case BleConnEvent::CONNECT_TIMEOUT:
          this->enter_(BleConnState::BACKOFF, now_ms, 0);
          return true;
        case BleConnEvent::DROP_UNSOLICITED_PAIRING:
          this->enter_(BleConnState::BACKOFF, now_ms, 0);
          return true;
        default:
          return false;
      }
    case BleConnState::DISCOVERING:
      switch (event) {
        case BleConnEvent::DISCOVER_DONE:
          this->enter_(BleConnState::READY, now_ms, 0);
          return true;
        case BleConnEvent::DISCOVER_TIMEOUT:
          this->enter_(BleConnState::BACKOFF, now_ms, 0);
          return true;
        case BleConnEvent::GAP_DISCONNECT_EVT:
          this->enter_(BleConnState::BACKOFF, now_ms, 0);
          return true;
        default:
          return false;
      }
    case BleConnState::READY:
      switch (event) {
        case BleConnEvent::DISCONNECT_REQUEST:
          this->enter_(BleConnState::DISCONNECTING, now_ms, 0);
          return true;
        case BleConnEvent::GAP_DISCONNECT_EVT:
          this->enter_(BleConnState::BACKOFF, now_ms, 0);
          return true;
        default:
          return false;
      }
    case BleConnState::DISCONNECTING:
      switch (event) {
        case BleConnEvent::GAP_DISCONNECT_EVT:
          this->enter_(BleConnState::IDLE, now_ms, 0);
          return true;
        default:
          return false;
      }
    case BleConnState::BACKOFF:
      switch (event) {
        case BleConnEvent::BACKOFF_ELAPSED:
          this->enter_(BleConnState::IDLE, now_ms, 0);
          return true;
        default:
          return false;
      }
    default:
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
