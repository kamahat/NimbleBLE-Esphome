// SURCHARGE de esphome/components/esp32_ble_tracker/automation.h --
// identique au core : aucune de ces triggers ne référence de type
// Bluedroid, seulement ESP32BLETracker/ESPBTDevice/ESPBTUUID/adv_data_t
// (tous alias vers ble_device_base dans notre esp32_ble_tracker.h).
#pragma once

#include "esphome/core/automation.h"
#include "esphome/components/esp32_ble_tracker/esp32_ble_tracker.h"

#include <vector>

#ifdef USE_ESP32

namespace esphome::esp32_ble_tracker {

class ESPBTAdvertiseTrigger final : public Trigger<const ESPBTDevice &>, public ESPBTDeviceListener {
 public:
  explicit ESPBTAdvertiseTrigger(ESP32BLETracker *parent) { parent->register_listener(this); }
  void set_addresses(std::initializer_list<uint64_t> addresses) { this->address_vec_ = addresses; }

  bool parse_device(const ESPBTDevice &device) override {
    uint64_t u64_addr = device.address_uint64();
    if (!address_vec_.empty()) {
      if (std::find(address_vec_.begin(), address_vec_.end(), u64_addr) == address_vec_.end()) {
        return false;
      }
    }
    this->trigger(device);
    return true;
  }

 protected:
  std::vector<uint64_t> address_vec_;
};

class BLEServiceDataAdvertiseTrigger final : public Trigger<const adv_data_t &>, public ESPBTDeviceListener {
 public:
  explicit BLEServiceDataAdvertiseTrigger(ESP32BLETracker *parent) { parent->register_listener(this); }
  void set_address(uint64_t address) { this->address_ = address; }
  void set_service_uuid16(uint16_t uuid) { this->uuid_ = ble_device_base::ESPBTUUID::from_uint16(uuid); }
  void set_service_uuid32(uint32_t uuid) { this->uuid_ = ble_device_base::ESPBTUUID::from_uint32(uuid); }
  void set_service_uuid128(uint8_t *uuid) { this->uuid_ = ble_device_base::ESPBTUUID::from_raw(uuid); }

  bool parse_device(const ESPBTDevice &device) override {
    if (this->address_ && device.address_uint64() != this->address_) {
      return false;
    }
    for (auto &service_data : device.get_service_datas()) {
      if (service_data.uuid == this->uuid_) {
        this->trigger(service_data.data);
        return true;
      }
    }
    return false;
  }

 protected:
  uint64_t address_ = 0;
  ble_device_base::ESPBTUUID uuid_;
};

class BLEManufacturerDataAdvertiseTrigger final : public Trigger<const adv_data_t &>, public ESPBTDeviceListener {
 public:
  explicit BLEManufacturerDataAdvertiseTrigger(ESP32BLETracker *parent) { parent->register_listener(this); }
  void set_address(uint64_t address) { this->address_ = address; }
  void set_manufacturer_uuid16(uint16_t uuid) { this->uuid_ = ble_device_base::ESPBTUUID::from_uint16(uuid); }
  void set_manufacturer_uuid32(uint32_t uuid) { this->uuid_ = ble_device_base::ESPBTUUID::from_uint32(uuid); }
  void set_manufacturer_uuid128(uint8_t *uuid) { this->uuid_ = ble_device_base::ESPBTUUID::from_raw(uuid); }

  bool parse_device(const ESPBTDevice &device) override {
    if (this->address_ && device.address_uint64() != this->address_) {
      return false;
    }
    for (auto &manufacturer_data : device.get_manufacturer_datas()) {
      if (manufacturer_data.uuid == this->uuid_) {
        this->trigger(manufacturer_data.data);
        return true;
      }
    }
    return false;
  }

 protected:
  uint64_t address_ = 0;
  ble_device_base::ESPBTUUID uuid_;
};

class BLEEndOfScanTrigger final : public Trigger<>, public ESPBTDeviceListener {
 public:
  explicit BLEEndOfScanTrigger(ESP32BLETracker *parent) { parent->register_listener(this); }
  bool parse_device(const ESPBTDevice &device) override { return false; }
  void on_scan_end() override { this->trigger(); }
};

template<typename... Ts> class ESP32BLEStartScanAction final : public Action<Ts...> {
 public:
  ESP32BLEStartScanAction(ESP32BLETracker *parent) : parent_(parent) {}
  TEMPLATABLE_VALUE(bool, continuous)
  void play(const Ts &...x) override {
    this->parent_->set_scan_continuous(this->continuous_.value(x...));
    if (this->parent_->get_scanner_state() == ble_device_base::ScannerState::IDLE) {
      this->parent_->start_scan();
    }
  }

 protected:
  ESP32BLETracker *parent_;
};

template<typename... Ts> class ESP32BLEStopScanAction final : public Action<Ts...>, public Parented<ESP32BLETracker> {
 public:
  void play(const Ts &...x) override { this->parent_->stop_scan(); }
};

}  // namespace esphome::esp32_ble_tracker

#endif
