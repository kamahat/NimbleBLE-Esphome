#include "nimble_server_event.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

namespace esphome::esp32_ble_server {

static const char *const TAG = "esp32_ble_server.event";

void ServerEventQueue::init(int capacity) { this->queue_ = xQueueCreate(capacity, sizeof(ServerEvent)); }

void ServerEventQueue::push_from_host_task(const ServerEvent &event) {
  if (xQueueSend(this->queue_, &event, 0) != pdTRUE) {
    delete[] event.data;
    this->dropped_count_++;
    if (this->dropped_count_ == 1) {
      ESP_LOGE(TAG, "GATT server event queue full, dropping events (queue too small or loop() draining too slowly)");
    }
  }
}

bool ServerEventQueue::pop(ServerEvent *out) { return xQueueReceive(this->queue_, out, 0) == pdTRUE; }

ServerEventQueue &server_event_queue() {
  static ServerEventQueue queue;
  static bool initialized = false;
  if (!initialized) {
    // Fixed capacity: writes/connects/subscribes are low-rate, user-triggered
    // events, not a high-throughput stream like scan advertisements.
    queue.init(16);
    initialized = true;
  }
  return queue;
}

}  // namespace esphome::esp32_ble_server

#endif  // USE_ESP32
