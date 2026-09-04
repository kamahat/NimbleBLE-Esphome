// ble_characteristic.h -- SURCHARGE NimbleBLE-Esphome de
// esp32_ble_server/ble_characteristic.h. Voir ble_descriptor.h pour le
// contexte général (table statique NimBLE vs création asynchrone Bluedroid).
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32

#include "ble_descriptor.h"
#include "esphome/components/ble_device_base/ble_device.h"
#include "esphome/components/bytebuffer/bytebuffer.h"

#include "host/ble_gatt.h"

#include <freertos/FreeRTOS.h>

#include <functional>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace esphome::esp32_ble_server {

class BLEService;

class BLECharacteristic final {
 public:
  BLECharacteristic(ble_device_base::ESPBTUUID uuid, uint32_t properties);
  ~BLECharacteristic();

  void set_value(bytebuffer::ByteBuffer buffer) { this->set_value(buffer.get_data()); }
  void set_value(std::vector<uint8_t> &&buffer);
  void set_value(std::initializer_list<uint8_t> data) { this->set_value(std::vector<uint8_t>(data)); }
  void set_value(const std::string &buffer) { this->set_value(std::vector<uint8_t>(buffer.begin(), buffer.end())); }

  static constexpr uint32_t PROPERTY_READ = 1 << 0;
  static constexpr uint32_t PROPERTY_WRITE = 1 << 1;
  static constexpr uint32_t PROPERTY_NOTIFY = 1 << 2;
  static constexpr uint32_t PROPERTY_BROADCAST = 1 << 3;
  static constexpr uint32_t PROPERTY_INDICATE = 1 << 4;
  static constexpr uint32_t PROPERTY_WRITE_NR = 1 << 5;

  void notify();

  void do_create(BLEService *service);

  void add_descriptor(BLEDescriptor *descriptor) { this->descriptors_.push_back(descriptor); }

  BLEService *get_service() { return this->service_; }
  ble_device_base::ESPBTUUID get_uuid() const { return this->uuid_; }
  std::vector<uint8_t> &get_value() { return this->value_; }
  uint16_t get_val_handle() const { return this->val_handle_; }

  bool is_created() const { return this->created_; }
  bool is_failed() const { return false; }

  void on_write(std::function<void(std::span<const uint8_t>, uint16_t)> &&callback) {
    this->on_write_callback_ =
        std::make_unique<std::function<void(std::span<const uint8_t>, uint16_t)>>(std::move(callback));
  }
  void on_read(std::function<void(uint16_t)> &&callback) {
    this->on_read_callback_ = std::make_unique<std::function<void(uint16_t)>>(std::move(callback));
  }

  /// Called by BLEServer::loop() draining a SUBSCRIBE event routed to this
  /// characteristic (looked up by val_handle on the host task -- a plain map
  /// lookup, not user code, so that part happens synchronously there).
  void handle_subscribe_(uint16_t conn_handle, bool cur_notify, bool cur_indicate);
  /// Called by BLEServer::loop() draining a CHR_WRITE event targeting this
  /// characteristic.
  void handle_write_from_queue_(const uint8_t *data, uint16_t len, uint16_t conn_handle);

  const ble_gatt_chr_def &native_def() const { return this->def_; }

 protected:
  friend class BLEService;
  static int access_cb_(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg);
  int handle_access_(uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt);

  BLEService *service_{};
  ble_device_base::ESPBTUUID uuid_;
  ble_uuid_any_t nimble_uuid_{};
  uint32_t flags_;
  uint16_t val_handle_{0xFFFF};
  bool created_{false};

  std::vector<uint8_t> value_;
  portMUX_TYPE value_mux_ = portMUX_INITIALIZER_UNLOCKED;

  std::vector<BLEDescriptor *> descriptors_;
  // Owned copy of the descriptor def array NimBLE keeps a pointer to;
  // allocated once in do_create(), lives for the device's lifetime (GATT
  // servers are never torn down at runtime in this project -- see
  // docs/OVERRIDE_CAVEATS.md).
  std::unique_ptr<ble_gatt_dsc_def[]> dsc_defs_;
  ble_gatt_chr_def def_{};

  struct ClientNotificationEntry {
    uint16_t conn_handle;
    bool indicate;  // true = indicate, false = notify
  };
  std::vector<ClientNotificationEntry> clients_to_notify_;

  std::unique_ptr<std::function<void(std::span<const uint8_t>, uint16_t)>> on_write_callback_;
  std::unique_ptr<std::function<void(uint16_t)>> on_read_callback_;
};

}  // namespace esphome::esp32_ble_server

#endif  // USE_ESP32
