// nimble_server_event.h -- thread-safe marshaling of NimBLE GATT server
// access-callback events (delivered on the NimBLE host task, same reason as
// components/nimble_ble/nimble_event.h on the client side: touching arbitrary
// user automation code -- on_write callbacks -- from the host task would be a
// data race) onto the ESPHome main loop.
//
// Reads never go through this queue: answering a read is just copying the
// characteristic/descriptor's own value_ (guarded by a short critical
// section) into the response mbuf, synchronously, on the host task -- no
// user code involved, nothing to marshal. Only writes (which may invoke a
// user on_write callback) and connect/disconnect/subscribe notifications go
// through here.
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include <cstdint>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

namespace esphome::esp32_ble_server {

enum class ServerEventType : uint8_t {
  CHR_WRITE,
  DSC_WRITE,
  CONNECT,
  DISCONNECT,
  SUBSCRIBE,
};

/// One marshaled event. `target` is the BLECharacteristic* or BLEDescriptor*
/// the write applies to (opaque here to avoid a circular include); CONNECT/
/// DISCONNECT/SUBSCRIBE carry target == nullptr and are dispatched to the
/// one BLEServer instance draining the queue.
struct ServerEvent {
  ServerEventType type;
  void *target{nullptr};
  uint16_t conn_handle{0};
  /// SUBSCRIBE only: the peer's current subscription state, captured
  /// synchronously in the GAP callback (the ble_gap_event is only valid for
  /// the duration of that call). Both false means the peer unsubscribed.
  bool cur_notify{false};
  bool cur_indicate{false};
  /// Heap-owned write payload; nullptr for CONNECT/DISCONNECT/SUBSCRIBE.
  /// The consumer (drain_server_events()) must delete[] this after use.
  uint8_t *data{nullptr};
  uint16_t data_len{0};
};

class ServerEventQueue {
 public:
  void init(int capacity);
  /// Called from the NimBLE host task (an access callback or GAP event).
  /// Never blocks; drops with a one-time logged counter bump on overflow.
  void push_from_host_task(const ServerEvent &event);
  /// Called from the ESPHome main loop. Returns false when empty.
  bool pop(ServerEvent *out);

 protected:
  QueueHandle_t queue_{nullptr};
  volatile uint32_t dropped_count_{0};
};

/// The one queue for this build's GATT server (a build has exactly one
/// BLEServer). Lazily initialized on first use.
ServerEventQueue &server_event_queue();

}  // namespace esphome::esp32_ble_server

#endif  // USE_ESP32
