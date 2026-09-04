// nimble_gattc.h -- one GATT client connection over raw ESP-IDF NimBLE APIs
// (host/ble_gap.h, host/ble_gatt.h), satisfying
// ble_device_base::BLEGattConnectionContract (ble_gatt_client.h).
//
// One instance == one connection slot (matches bluetooth_connection's model:
// new_gatt_backend() instantiates one backend per configured proxy
// connection, not a shared multiplexer). Every NimBLE GAP/GATT callback for
// this connection is invoked with the instance's own `this` as cb_arg --
// ble_gap_connect()'s callback is inherited by the connection for its whole
// lifetime (NimBLE docs), and every ble_gattc_* call below passes `this`
// explicitly -- so a callback never needs to look its owner up.
//
// Discovery is depth-first and strictly sequential (services, then each
// service's characteristics in turn, then each characteristic's descriptors
// in turn): NimBLE permits exactly one outstanding GATT procedure per
// connection, so overlapping discovery calls would simply fail, not race.
// BleConnectionFsm owns only the outer bounded-timeout state (Connecting/
// Discovering); the discover_step_ cursor below is the inner walk within
// "Discovering" and has no bearing on the FSM's deadline.
#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP32
#ifdef USE_BLE_GATT_CLIENT

#include "esphome/components/ble_device_base/ble_gatt_client.h"
#include "ble_connection_fsm.h"
#include "nimble_event.h"

#include "host/ble_gap.h"
#include "host/ble_gatt.h"

// NimBLE's porting-layer log_common.h #defines bare, unprefixed macros --
// LOG_LEVEL_ERROR/WARN/INFO/DEBUG/NONE -- that collide directly with
// esphome::api::LogLevel's enum values of the identical names (api_pb2.h).
// Any translation unit that includes both this header and api_pb2.h (M4:
// bluetooth_connection_hub.cpp is the first to need both) fails with
// "expected class-name before '{'" deep inside the corrupted enum, because
// the preprocessor substitutes the numeric macro value into the enum
// declaration's identifier position. Undefined here, at the point we pull
// NimBLE's headers in, so no downstream include order can hit this by
// surprise (confirmed on real hardware build, not a hypothetical).
#undef LOG_VERSION_V3
#undef LOG_TYPE_STREAM
#undef LOG_TYPE_MEMORY
#undef LOG_TYPE_STORAGE
#undef LOG_LEVEL_DEBUG
#undef LOG_LEVEL_INFO
#undef LOG_LEVEL_WARN
#undef LOG_LEVEL_ERROR
#undef LOG_LEVEL_CRITICAL
#undef LOG_LEVEL_NONE
#undef LOG_LEVEL_MAX
#undef LOG_LEVEL_STR

#include <vector>

namespace esphome::nimble_ble {

/// Drains every pending event from the shared queue (see gatt_event_queue())
/// and routes each to its owning NimbleGattEngine. Call once per ESPHome
/// main loop tick from any single component that is guaranteed to run every
/// tick -- NimbleGattEngine::loop() does this, so any configured connection
/// slot's Component::loop() keeps the whole build's queue drained even
/// while other slots are idle.
void drain_gatt_events();

class NimbleGattEngine {
 public:
  void set_listener(ble_device_base::GattClientListener *listener) { this->listener_ = listener; }

  int connect(uint64_t address, uint8_t addr_type);
  /// Resumes from Backoff (fires BACKOFF_ELAPSED, the one transition
  /// connect() itself cannot take -- Backoff only accepts that event per
  /// spec/transitions.json) and immediately issues a fresh connect(). The
  /// engine has no timer of its own for how long to wait in Backoff (by
  /// design: only Connecting/Discovering carry a deadline) -- callers (e.g.
  /// esp32_ble_client's retry loop) must only call this once they know the
  /// backoff delay has elapsed.
  int retry_connect(uint64_t address, uint8_t addr_type);
  int gatt_disconnect();
  bool cancel_gatt_disconnect();
  int discover_services();
  int read_characteristic(uint16_t handle);
  int write_characteristic(uint16_t handle, const uint8_t *data, uint16_t len, bool response);
  int read_descriptor(uint16_t handle);
  int write_descriptor(uint16_t handle, const uint8_t *data, uint16_t len);
  int notify_characteristic(uint16_t handle, bool enable);
  int pair();
  int update_connection_params(uint16_t min_interval, uint16_t max_interval, uint16_t latency, uint16_t timeout);
  ble_device_base::GattServiceTable get_service_table();
  void release_services();
  void set_connection_type(ble_device_base::ConnectionType connection_type) {
    this->connection_type_ = connection_type;
  }

  /// Drains this build's shared event queue and polls the FSM's own
  /// deadline. Must be called every ESPHome main loop tick by the owning
  /// Component (bluetooth_connection_nimble.cpp's BluetoothConnection wraps
  /// this, matching the Bluedroid backend's own loop()-driven model).
  void loop();

  /// The event owner tag this instance registers on every NimBLE callback it
  /// issues -- exposed so the static callback trampolines (in the .cpp) can
  /// be written once as free functions instead of per-instance thunks.
  void *event_owner() { return this; }

  /// The FSM's current outer state (Idle/Connecting/Discovering/Ready/
  /// Disconnecting/Backoff) -- exposed so a consumer (esp32_ble_client's
  /// retry loop, future bluetooth_proxy diagnostics) can react to it without
  /// duplicating the FSM's own bookkeeping.
  BleConnState state() const { return this->fsm_.state(); }

  /// The FSM's own per-slot failure counter (see BleConnectionFsm::backoff_count())
  /// -- M7's connect_backoff.h keys its exponential-delay computation off this
  /// directly instead of tracking a separate per-address counter.
  uint16_t backoff_count() const { return this->fsm_.backoff_count(); }

 protected:
  friend void drain_gatt_events();
  void handle_gatt_event_(const GattEvent &event);
  void finish_connect_(int error);
  void finish_disconnect_(int reason);
  void start_next_service_chr_discovery_();
  void start_next_chr_dsc_discovery_();
  void finish_discovery_(int error);
  void fail_pending_op_(int error);

  static int gap_event_cb_(struct ble_gap_event *event, void *arg);
  static int disc_svc_cb_(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_svc *service,
                          void *arg);
  static int disc_chr_cb_(uint16_t conn_handle, const struct ble_gatt_error *error, const struct ble_gatt_chr *chr,
                          void *arg);
  static int disc_dsc_cb_(uint16_t conn_handle, const struct ble_gatt_error *error, uint16_t chr_val_handle,
                          const struct ble_gatt_dsc *dsc, void *arg);
  static int attr_cb_(uint16_t conn_handle, const struct ble_gatt_error *error, struct ble_gatt_attr *attr,
                      void *arg);

  ble_device_base::GattClientListener *listener_{nullptr};
  BleConnectionFsm fsm_;
  uint16_t conn_handle_{BLE_HS_CONN_HANDLE_NONE};
  uint16_t mtu_{ble_device_base::DEFAULT_ATT_MTU};
  // Guards against reporting a connect attempt's outcome twice: our own
  // bounded timeout (loop()) can resolve it before the cancelled attempt's
  // own async CONNECT event later arrives through the queue for the same
  // attempt. Reset at the start of every connect(), set the first time
  // either path reports a result.
  bool connect_result_reported_{false};
  ble_device_base::ConnectionType connection_type_{ble_device_base::ConnectionType::V1};

  // The single outstanding read/write's completion type -- attr_cb_ is
  // shared by every ble_gattc_read*/write* call, so this says which
  // listener method (and, for reads, whether it targets a characteristic or
  // a descriptor) the next attr_cb_ callback resolves to.
  enum class PendingOp : uint8_t { NONE, READ_CHR, READ_DSC, WRITE_CHR, WRITE_DSC } pending_op_{PendingOp::NONE};

  // Discovery-only cursor state; meaningless outside BleConnState::DISCOVERING.
  size_t discover_svc_index_{0};
  size_t discover_chr_index_{0};

  std::vector<ble_device_base::GattService> services_;
  std::vector<ble_device_base::GattCharacteristic> characteristics_;
  std::vector<ble_device_base::GattDescriptor> descriptors_;
};

}  // namespace esphome::nimble_ble

#endif  // USE_BLE_GATT_CLIENT
#endif  // USE_ESP32
