#include "esp32_ble_tracker.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include "esphome/core/hal.h"

namespace esphome::esp32_ble_tracker {

static const char *const TAG = "esp32_ble_tracker.nimble";

ESP32BLETracker *global_esp32_ble_tracker = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

float ESP32BLETracker::get_setup_priority() const { return setup_priority::AFTER_BLUETOOTH; }

void ESP32BLETracker::setup() {
  if (this->parent_->is_failed()) {
    this->mark_failed();
    ESP_LOGE(TAG, "BLE Tracker was marked failed by ESP32BLE");
    return;
  }
  global_esp32_ble_tracker = this;

  this->merger_.bind(&this->dispatcher_, &this->scan_continuous_, TAG);

  if (this->scan_continuous_) {
    this->start_scan_();
  }
}

void ESP32BLETracker::loop() {
  if (!this->parent_->is_active())
    return;

  if (!this->merger_.empty()) {
    this->merger_.sweep(millis());
  }

  if (this->scanner_state_ == ScannerState::IDLE && this->scan_continuous_) {
    this->start_scan_();
  }
}

void ESP32BLETracker::start_scan() {
  this->scan_continuous_ = true;
  if (this->scanner_state_ == ScannerState::IDLE)
    this->start_scan_();
}

void ESP32BLETracker::stop_scan() {
  this->scan_continuous_ = false;
  this->stop_scan_();
}

void ESP32BLETracker::start_scan_() {
  if (!this->parent_->is_active()) {
    ESP_LOGW(TAG, "Cannot start scan while ESP32BLE is disabled.");
    return;
  }
  if (this->scanner_state_ != ScannerState::IDLE)
    return;

  struct ble_gap_disc_params params;
  memset(&params, 0, sizeof(params));
  params.itvl = this->scan_interval_;
  params.window = this->scan_window_;
  params.filter_policy = BLE_HCI_SCAN_FILT_NO_WL;
  params.limited = 0;
  params.passive = this->scan_active_ ? 0 : 1;
  params.filter_duplicates = 0;  // we dedup ourselves (DiscoveredDeviceLog); NimBLE dedup would
                                 // also drop the scan-response half of a pair we need to merge.

  int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, this->scan_duration_ * 1000 /* scan_duration_ is in whole seconds (CONF_DURATION), ble_gap_disc wants ms */,
                        &params, &ESP32BLETracker::gap_event_handler_, this);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gap_disc failed: %d", rc);
    return;
  }
  this->set_scanner_state_(ScannerState::RUNNING);
  ESP_LOGV(TAG, "Scan started.");
}

void ESP32BLETracker::stop_scan_() {
  if (this->scanner_state_ != ScannerState::RUNNING)
    return;
  int rc = ble_gap_disc_cancel();
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    ESP_LOGE(TAG, "ble_gap_disc_cancel failed: %d", rc);
  }
  this->merger_.flush();
  this->dispatcher_.on_scan_end();
  this->set_scanner_state_(ScannerState::IDLE);
}

int ESP32BLETracker::gap_event_handler_(struct ble_gap_event *event, void *arg) {
  return static_cast<ESP32BLETracker *>(arg)->handle_gap_event_(event);
}

int ESP32BLETracker::handle_gap_event_(struct ble_gap_event *event) {
  switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
      const auto &disc = event->disc;
      const uint8_t *mac = disc.addr.val;  // LSB-first (controller order) -- matches
                                           // ble_device_base's ingest convention directly.
      // Legacy PDU report type (Bluetooth Core spec, HCI LE Advertising Report):
      // 0=ADV_IND 1=ADV_DIRECT_IND 2=ADV_SCAN_IND 3=ADV_NONCONN_IND 4=SCAN_RSP
      if (disc.event_type == 4) {
        this->merger_.submit_scan_rsp(mac, disc.rssi, disc.addr.type, disc.data, disc.length_data);
      } else if (disc.event_type == 0 || disc.event_type == 2) {
        // Scannable (ADV_IND / ADV_SCAN_IND): a scan response may follow.
        this->merger_.stash_adv(mac, disc.rssi, disc.addr.type, disc.data, disc.length_data, millis());
      } else {
        // Not scannable (ADV_DIRECT_IND / ADV_NONCONN_IND): deliver directly, no response coming.
        this->dispatcher_.dispatch(mac, disc.rssi, disc.addr.type, disc.data, disc.length_data,
                                   /*raw_only=*/false, this->scan_continuous_ ? nullptr : TAG);
      }
      return 0;
    }
    case BLE_GAP_EVENT_DISC_COMPLETE: {
      ESP_LOGV(TAG, "Scan complete (reason=%d)", event->disc_complete.reason);
      this->merger_.flush();
      this->dispatcher_.on_scan_end();
      this->set_scanner_state_(ScannerState::IDLE);
      return 0;
    }
    default:
      return 0;
  }
}

void ESP32BLETracker::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BLE Tracker (NimBLE):\n"
                "  Scan Duration: %" PRIu32 " s\n"
                "  Scan Interval: %.1f ms\n"
                "  Scan Window: %.1f ms\n"
                "  Scan Type: %s\n"
                "  Continuous Scanning: %s",
                this->scan_duration_, this->scan_interval_ * 0.625f, this->scan_window_ * 0.625f,
                this->scan_active_ ? "ACTIVE" : "PASSIVE", YESNO(this->scan_continuous_));
}

}  // namespace esphome::esp32_ble_tracker

#endif
