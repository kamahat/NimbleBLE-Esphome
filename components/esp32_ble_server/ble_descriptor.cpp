#include "ble_descriptor.h"
#include "ble_characteristic.h"
#include "nimble_server_event.h"

#include "esphome/components/nimble_ble/nimble_uuid.h"

#ifdef USE_ESP32

#include "host/ble_hs_mbuf.h"
#include "os/os_mbuf.h"

namespace esphome::esp32_ble_server {

BLEDescriptor::BLEDescriptor(ble_device_base::ESPBTUUID uuid, uint16_t max_len, bool read, bool write)
    : uuid_(uuid), max_len_(max_len) {
  this->att_flags_ = 0;
  if (read)
    this->att_flags_ |= BLE_ATT_F_READ;
  if (write)
    this->att_flags_ |= BLE_ATT_F_WRITE;
}

void BLEDescriptor::set_value(std::vector<uint8_t> &&buffer) {
  portENTER_CRITICAL(&this->value_mux_);
  this->value_ = std::move(buffer);
  portEXIT_CRITICAL(&this->value_mux_);
}

uint16_t BLEDescriptor::get_uuid16_if_short() const {
  return this->uuid_.type() == ble_device_base::ESPBTUUID::Type::UUID16 ? this->uuid_.uuid16() : 0;
}

void BLEDescriptor::do_create(BLECharacteristic *characteristic) {
  this->characteristic_ = characteristic;
  nimble_ble::espbtuuid_to_nimble_uuid(this->uuid_, &this->nimble_uuid_);
  this->def_.uuid = &this->nimble_uuid_.u;
  this->def_.att_flags = this->att_flags_;
  this->def_.min_key_size = 0;
  this->def_.access_cb = &BLEDescriptor::access_cb_;
  this->def_.arg = this;
  this->created_ = true;
}

int BLEDescriptor::access_cb_(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt,
                              void *arg) {
  return static_cast<BLEDescriptor *>(arg)->handle_access_(conn_handle, ctxt);
}

int BLEDescriptor::handle_access_(uint16_t conn_handle, struct ble_gatt_access_ctxt *ctxt) {
  if (ctxt->op == BLE_GATT_ACCESS_OP_READ_DSC) {
    // Read: just copy the current value into the response mbuf. No user
    // code runs here, so this can be answered synchronously on the host
    // task -- nothing to marshal to the main loop.
    portENTER_CRITICAL(&this->value_mux_);
    int rc = os_mbuf_append(ctxt->om, this->value_.data(), this->value_.size());
    portEXIT_CRITICAL(&this->value_mux_);
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_DSC) {
    uint16_t len = OS_MBUF_PKTLEN(ctxt->om);
    if (len > this->max_len_)
      return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    uint8_t *buf = new uint8_t[len];
    ble_hs_mbuf_to_flat(ctxt->om, buf, len, nullptr);
    portENTER_CRITICAL(&this->value_mux_);
    this->value_.assign(buf, buf + len);
    portEXIT_CRITICAL(&this->value_mux_);
    if (this->on_write_callback_) {
      // A user callback may run: marshal to the main loop rather than
      // invoke it here on the host task.
      ServerEvent event;
      event.type = ServerEventType::DSC_WRITE;
      event.target = this;
      event.conn_handle = conn_handle;
      event.data = buf;  // ownership transferred to the queue/consumer
      event.data_len = len;
      server_event_queue().push_from_host_task(event);
    } else {
      delete[] buf;
    }
    return 0;
  }
  return BLE_ATT_ERR_UNLIKELY;
}

}  // namespace esphome::esp32_ble_server

#endif  // USE_ESP32
