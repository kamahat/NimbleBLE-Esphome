#include "bluetooth_connection.h"

#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS

#include "esphome/components/api/api_pb2.h"
#include "esphome/core/log.h"

namespace esphome::bluetooth_connection {

static const char *const TAG = "bluetooth_connection";

BatchClose close_service_batch(api::BluetoothGATTGetServicesResponse &resp, size_t &current_size, int16_t &send_service,
                               uint8_t connection_index, const char *address_str) {
  // Calculate the actual size of just this service (+1 for the field tag)
  size_t service_size = resp.services.back().calculate_size() + 1;

  if (current_size + service_size > MAX_PACKET_SIZE) {
    if (resp.services.size() > 1) {
      // We would go over -- pop the last service and retry it in the next batch
      resp.services.pop_back();
      ESP_LOGD(TAG, "[%d] [%s] Service %d would exceed limit (current: %u + service: %u > %u), sending current batch",
               connection_index, address_str, send_service, (unsigned) current_size, (unsigned) service_size,
               (unsigned) MAX_PACKET_SIZE);
      // Don't advance send_service -- the popped service goes into the next batch
    } else {
      // This single service is too large, but we have to send it anyway;
      // advance so we don't get stuck
      ESP_LOGW(TAG, "[%d] [%s] Service %d is too large (%u bytes) but sending anyway", connection_index, address_str,
               send_service, (unsigned) service_size);
      send_service++;
    }
    return BatchClose::SEND;
  }

  current_size += service_size;
  send_service++;
  return BatchClose::CONTINUE;
}

}  // namespace esphome::bluetooth_connection

#endif  // USE_BLUETOOTH_PROXY_CONNECTIONS

#if defined(USE_ESP32) && defined(USE_BLE_GATT_CLIENT)

#include "host/ble_gap.h"

namespace esphome::bluetooth_connection {

// Address-scoped NimBLE maintenance. Gated with the connection surface: the
// advertisement-only arm no longer dispatches these requests at all.
//
// PUBLIC always assumed, same simplification as the rest of this project's
// GATT client paths (see docs/OVERRIDE_CAVEATS.md): NimBLE composes by
// address directly, with no scan-learned address type.

conn_err_t unpair_device(uint64_t address) {
  ble_addr_t addr;
  addr.type = 0;  // BLE_ADDR_PUBLIC
  uint8_t mac_msb[6];
  ble_device_base::uint64_to_mac_msb_first(address, mac_msb);
  for (int i = 0; i < 6; i++)
    addr.val[i] = mac_msb[5 - i];
  return ble_gap_unpair(&addr);
}

}  // namespace esphome::bluetooth_connection
#endif  // USE_ESP32 && USE_BLE_GATT_CLIENT
