#include "nimble_event.h"

#ifdef USE_ESP32
#ifdef USE_BLE_GATT_CLIENT

#include "esphome/core/log.h"

namespace esphome::nimble_ble {

static const char *const TAG = "nimble_ble.event";

void GattEventQueue::init(int capacity) {
  this->queue_ = xQueueCreate(capacity, sizeof(GattEvent));
}

void GattEventQueue::push_from_host_task(const GattEvent &event) {
  // xQueueSend with a 0 tick timeout: the NimBLE host task must never block
  // here (blocking it would stall every other in-flight BLE operation, not
  // just this connection's). A full queue means the main loop is draining
  // slower than events arrive; that is a real problem to surface (see
  // dropped_count()), not one to solve by blocking the stack.
  if (xQueueSend(this->queue_, &event, 0) != pdTRUE) {
    // Free what push() would otherwise have handed to the consumer -- a
    // dropped event must not leak its heap payload.
    delete[] event.data;
    this->dropped_count_++;
    if (this->dropped_count_ == 1) {
      // Logged once, not per-drop: a sustained overflow would otherwise spam
      // the log from the host task on every subsequent event. dropped_count()
      // is still live for a caller that wants to notice more were lost.
      ESP_LOGE(TAG, "GATT event queue full, dropping events (queue too small or loop() draining too slowly)");
    }
  }
}

bool GattEventQueue::pop(GattEvent *out) { return xQueueReceive(this->queue_, out, 0) == pdTRUE; }

GattEventQueue &gatt_event_queue(int capacity_hint) {
  static GattEventQueue queue;
  static bool initialized = false;
  if (!initialized) {
    queue.init(capacity_hint);
    initialized = true;
  }
  return queue;
}

}  // namespace esphome::nimble_ble

#endif  // USE_BLE_GATT_CLIENT
#endif  // USE_ESP32
