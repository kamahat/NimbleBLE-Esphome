#include "adv_queue.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"

namespace esphome::esp32_ble_tracker {

static const char *const TAG = "esp32_ble_tracker.adv_queue";

void AdvQueue::init(int capacity) { this->queue_ = xQueueCreate(capacity, sizeof(AdvQueueEvent)); }

void AdvQueue::push_from_host_task(const AdvQueueEvent &event) {
  if (xQueueSend(this->queue_, &event, 0) != pdTRUE) {
    delete[] event.data;
    this->dropped_count_++;
    if (this->dropped_count_ == 1) {
      ESP_LOGW(TAG, "Advertisement queue full, dropping reports (see dropped_count() for a running total)");
    }
    return;
  }
}

bool AdvQueue::pop(AdvQueueEvent *out) { return xQueueReceive(this->queue_, out, 0) == pdTRUE; }

}  // namespace esphome::esp32_ble_tracker

#endif  // USE_ESP32
