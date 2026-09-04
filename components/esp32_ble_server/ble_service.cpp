#include "ble_service.h"

#include "esphome/components/nimble_ble/nimble_uuid.h"

#ifdef USE_ESP32

namespace esphome::esp32_ble_server {

BLEService::BLEService(ble_device_base::ESPBTUUID uuid, uint16_t num_handles, uint8_t inst_id, bool advertise)
    : uuid_(uuid), num_handles_(num_handles), inst_id_(inst_id), advertise_(advertise) {}

BLEService::~BLEService() {
  for (auto *chr : this->characteristics_)
    delete chr;  // NOLINT(cppcoreguidelines-owning-memory)
}

BLECharacteristic *BLEService::create_characteristic(ble_device_base::ESPBTUUID uuid, uint32_t properties) {
  auto *chr = new BLECharacteristic(uuid, properties);  // NOLINT(cppcoreguidelines-owning-memory)
  this->characteristics_.push_back(chr);
  this->last_created_characteristic_ = chr;
  return chr;
}

void BLEService::do_create(BLEServer *server) {
  this->server_ = server;
  nimble_ble::espbtuuid_to_nimble_uuid(this->uuid_, &this->nimble_uuid_);

  if (!this->characteristics_.empty()) {
    this->chr_defs_ = std::make_unique<ble_gatt_chr_def[]>(this->characteristics_.size() + 1);
    for (size_t i = 0; i < this->characteristics_.size(); i++) {
      this->characteristics_[i]->do_create(this);
      this->chr_defs_[i] = this->characteristics_[i]->native_def();
    }
    this->chr_defs_[this->characteristics_.size()] = {};  // terminator
  }

  this->def_.type = BLE_GATT_SVC_TYPE_PRIMARY;
  this->def_.uuid = &this->nimble_uuid_.u;
  this->def_.includes = nullptr;
  this->def_.characteristics = this->characteristics_.empty() ? nullptr : this->chr_defs_.get();
}

}  // namespace esphome::esp32_ble_server

#endif  // USE_ESP32
