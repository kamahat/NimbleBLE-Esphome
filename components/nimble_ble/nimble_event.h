// nimble_event.h -- thread-safe marshaling of NimBLE GAP/GATT client
// callbacks (delivered on the NimBLE host task) onto the ESPHome main loop.
//
// Real ESPHome's own Bluedroid backend (esp32_ble/ble_event.h) does exactly
// this for the same reason: ESP-IDF/NimBLE callbacks fire from a stack task,
// not from the Arduino/ESPHome loop task, so touching a component's own
// state directly from inside one (as our M2 esp32_ble_tracker override does
// for scan results -- acceptable there because ESPBTDeviceListener::
// parse_device is a short, self-contained read) would be a data race for
// anything stateful. A GATT client's connect/discover/read/write flow,
// mutating a BleConnectionFsm and completing operations one at a time, is
// exactly that "anything stateful" case, so it goes through this queue.
//
// Variable-length payloads (characteristic reads, inbound notifications) are
// heap-copied at push() time and owned by the popped event until the
// consumer frees them -- never truncated to a fixed inline size. Silent
// truncation of large GATT responses is one of the fl4p weaknesses this
// project's audit flagged to avoid (docs/SECURITY.md).
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32
#ifdef USE_BLE_GATT_CLIENT

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome::nimble_ble {

enum class GattEventType : uint8_t {
  CONNECT,
  DISCONNECT,
  MTU,
  DISC_SVC,
  DISC_SVC_DONE,
  DISC_CHR,
  DISC_CHR_DONE,
  DISC_DSC,
  DISC_DSC_DONE,
  /// A read OR write attribute operation completed (NimBLE's ble_gattc_read*/
  /// write* all share the ble_gatt_attr_fn signature). Which listener method
  /// this resolves to is decided by the consumer's own pending_op_ bookkeeping
  /// (NimbleGattClient allows only one outstanding attr op at a time), not by
  /// anything in this event.
  ATTR_OP,
  NOTIFY_RX,
  ENC_CHANGE,
  CONN_UPDATE,
};

/// One marshaled event. Field meaning depends on `type`; see nimble_gattc.cpp
/// for the producer side (push) and consumer side (handling in loop()).
struct GattEvent {
  /// The NimbleGattClient instance this event belongs to -- every NimBLE
  /// callback that can produce a GattEvent is invoked with cb_arg (GAP) or
  /// cb_arg (GATT ops) set to the owning instance's `this` at the point the
  /// operation was issued (ble_gap_connect's callback is inherited by the
  /// connection for its whole lifetime, so this stays valid for every
  /// later event on that connection too). Letting the event carry its own
  /// owner means any one instance's loop() can safely drain the whole
  /// shared queue -- see drain_gatt_events() in nimble_gattc.cpp -- instead
  /// of needing a conn_handle registry.
  void *owner{nullptr};
  GattEventType type;
  uint16_t conn_handle{0};
  /// NimBLE host error code (0 = success) or ATT status, meaning depends on
  /// type: connect/disconnect reason, discovery ble_gatt_error.status, etc.
  int status{0};
  /// Primary handle: attribute handle for READ/WRITE/NOTIFY_RX, service
  /// start_handle for DISC_SVC, characteristic def_handle for DISC_CHR,
  /// descriptor handle for DISC_DSC.
  uint16_t handle{0};
  /// Secondary handle, meaning depends on type: service end_handle
  /// (DISC_SVC), characteristic value handle (DISC_CHR), negotiated MTU
  /// (MTU), 0/1 indication flag (NOTIFY_RX).
  uint16_t handle2{0};
  /// Characteristic properties bitfield (DISC_CHR only).
  uint8_t properties{0};
  /// Raw NimBLE UUID bytes, BLE_UUID_TYPE_16/32/128 -- see nimble_uuid.h for
  /// the conversion to ble_device_base::ESPBTUUID. Zeroed/unused where the
  /// event carries no UUID (READ/WRITE/NOTIFY_RX/CONNECT/...).
  uint8_t uuid_type{0};
  uint8_t uuid128[16]{0};
  /// Heap-owned variable-length payload (READ result, NOTIFY_RX value);
  /// nullptr for every other event type. The consumer (nimble_gattc's
  /// loop()-driven drain) must delete[] this after use.
  uint8_t *data{nullptr};
  uint16_t data_len{0};
};

/// A single shared queue for every open GATT connection in this build --
/// events already carry conn_handle, so one consumer loop can fan them out.
/// Sized by ESPHOME_BLE_GATT_CLIENT_COUNT (see nimble_gattc.cpp): a build
/// with more concurrent connections gets a proportionally larger queue
/// instead of a fixed guess.
class GattEventQueue {
 public:
  void init(int capacity);
  /// Called from the NimBLE host task (GAP/GATT callback context). Copies
  /// `event` into the queue; never blocks (drops with a one-time counter
  /// bump on overflow rather than stalling the host task -- see
  /// dropped_count()).
  void push_from_host_task(const GattEvent &event);
  /// Called from the ESPHome main loop. Returns false when the queue is
  /// empty.
  bool pop(GattEvent *out);
  /// Events dropped because the queue was full when push_from_host_task ran.
  /// A nonzero count means ESPHOME_BLE_GATT_CLIENT_COUNT's queue sizing (or
  /// how often loop() drains it) needs revisiting -- logged, not silent.
  uint32_t dropped_count() const { return this->dropped_count_; }

 protected:
  QueueHandle_t queue_{nullptr};
  volatile uint32_t dropped_count_{0};
};

/// The one queue shared by every NimbleGattClient instance in this build
/// (each event carries its own owner, so a single funnel is safe -- see
/// GattEvent::owner). Lazily sized to capacity on the first call; every
/// caller in a given build passes the same ESPHOME_BLE_GATT_CLIENT_COUNT-
/// derived capacity, so which instance happens to run first is irrelevant.
GattEventQueue &gatt_event_queue(int capacity_hint);

}  // namespace esphome::nimble_ble

#endif  // USE_BLE_GATT_CLIENT
#endif  // USE_ESP32
