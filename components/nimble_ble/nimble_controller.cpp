#include "nimble_controller.h"

#ifdef USE_ESP32

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

#include <algorithm>

namespace esphome::nimble_ble {

static const char *const TAG = "nimble_ble";

volatile bool NimbleController::synced_ = false;

void NimbleController::on_reset_(int reason) {
  ESP_LOGW(TAG, "NimBLE host reset, reason=%d", reason);
  synced_ = false;
}

void NimbleController::on_sync_() {
  // Ensures we have a usable (public or random static) address before
  // anything tries to advertise/connect -- mirrors the nimble blecent/bleprph
  // sample bring-up sequence.
  int rc = ble_hs_util_ensure_addr(0);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_hs_util_ensure_addr failed: %d", rc);
    return;
  }
  synced_ = true;
  ESP_LOGD(TAG, "NimBLE host synced");
}

void NimbleController::host_task_(void *param) {
  // nimble_port_run() blocks forever processing host events; this task's
  // only job is to run it and clean up if it ever returns (reset/shutdown).
  nimble_port_run();
  nimble_port_freertos_deinit();
}

bool NimbleController::setup(const char *device_name) {
  this->device_name_ = device_name;

  esp_err_t err = nimble_port_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "nimble_port_init failed: %d", err);
    return false;
  }

  ble_hs_cfg.reset_cb = &NimbleController::on_reset_;
  ble_hs_cfg.sync_cb = &NimbleController::on_sync_;
  ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

  // M7 hardening (docs/SECURITY.md): fl4p hardcoded a pairing passkey
  // (123456) that got auto-accepted for any peer requesting KEYBOARD_ONLY
  // pairing, with no allowlist or consent. This project has no legitimate
  // use for passkey/OOB pairing at all (documented deferred scope, see
  // docs/OVERRIDE_CAVEATS.md) -- declaring NO_INPUT_OUTPUT capability makes
  // a passkey-based exchange structurally impossible (there is no code path
  // where this device could generate, display, or accept one), rather than
  // relying on a policy check to reject it after the fact. sm_bonding/
  // sm_mitm stay off: nothing here should persist bond state for or demand
  // authenticated pairing with a peer this project does not yet have a
  // pairing UX for.
  ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT;
  ble_hs_cfg.sm_bonding = 0;
  ble_hs_cfg.sm_mitm = 0;

  int rc = ble_svc_gap_device_name_set(device_name);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_svc_gap_device_name_set failed: %d", rc);
    return false;
  }

  nimble_port_freertos_init(&NimbleController::host_task_);

  // Bring-up is asynchronous (host task + sync_cb); block briefly for sync
  // the same way ESP32BLE::ble_setup_() waits 200ms after Bluedroid bring-up.
  // 2s is generous headroom for the host task to start and sync once --
  // M1 exit criterion is "compiles and advertises", not final tuning.
  for (int waited_ms = 0; waited_ms < 2000 && !synced_; waited_ms += 20) {
    vTaskDelay(pdMS_TO_TICKS(20));
  }
  if (!synced_) {
    ESP_LOGE(TAG, "NimBLE host did not sync within timeout");
    return false;
  }
  return true;
}

bool NimbleController::start_advertising(ble_gap_event_fn *cb, void *cb_arg) {
  if (!synced_) {
    ESP_LOGE(TAG, "Cannot advertise: host not synced");
    return false;
  }

  struct ble_hs_adv_fields fields;
  memset(&fields, 0, sizeof(fields));
  fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
  fields.tx_pwr_lvl_is_present = 1;
  fields.tx_pwr_lvl = BLE_HS_ADV_TX_PWR_LVL_AUTO;
  fields.name = reinterpret_cast<const uint8_t *>(this->device_name_);
  fields.name_len = this->device_name_ != nullptr ? strlen(this->device_name_) : 0;
  fields.name_is_complete = 1;

  int rc = ble_gap_adv_set_fields(&fields);
  if (rc == BLE_HS_EMSGSIZE) {
    // Legacy advertising payload is capped at BLE_HS_ADV_MAX_SZ (31) bytes
    // total; flags (3B) + tx power (3B) + name AD header (2B) leaves only
    // ~23 bytes for the device name itself -- a descriptive `esphome:
    // name:` easily exceeds that. Found via real hardware testing in M5
    // (the first role in this project that actually advertises -- the
    // client/scanner roles never hit this path). Degrade gracefully
    // instead of never advertising at all: drop tx power first, then
    // truncate the name.
    ESP_LOGW(TAG, "Advertising fields too large for name '%s'; dropping tx power level",
             this->device_name_ != nullptr ? this->device_name_ : "");
    fields.tx_pwr_lvl_is_present = 0;
    rc = ble_gap_adv_set_fields(&fields);
  }
  if (rc == BLE_HS_EMSGSIZE) {
    constexpr size_t kMaxNameLen = BLE_HS_ADV_MAX_SZ - 3 /* flags AD */ - 2 /* name AD header */;
    ESP_LOGW(TAG, "Still too large; truncating advertised name to %u bytes", (unsigned) kMaxNameLen);
    fields.name_len = static_cast<uint8_t>(std::min<size_t>(fields.name_len, kMaxNameLen));
    fields.name_is_complete = 0;
    rc = ble_gap_adv_set_fields(&fields);
  }
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gap_adv_set_fields failed: %d", rc);
    return false;
  }

  struct ble_gap_adv_params adv_params;
  memset(&adv_params, 0, sizeof(adv_params));
  adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
  adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

  rc = ble_gap_adv_start(BLE_OWN_ADDR_PUBLIC, nullptr, BLE_HS_FOREVER, &adv_params, cb, cb_arg);
  if (rc != 0) {
    ESP_LOGE(TAG, "ble_gap_adv_start failed: %d", rc);
    return false;
  }
  this->advertising_ = true;
  return true;
}

bool NimbleController::stop_advertising() {
  if (!this->advertising_)
    return true;
  int rc = ble_gap_adv_stop();
  if (rc != 0 && rc != BLE_HS_EALREADY) {
    ESP_LOGE(TAG, "ble_gap_adv_stop failed: %d", rc);
    return false;
  }
  this->advertising_ = false;
  return true;
}

void NimbleController::get_mac_msb_first(uint8_t out[6]) const {
  uint8_t addr[6];
  int rc = ble_hs_id_copy_addr(BLE_OWN_ADDR_PUBLIC, addr, nullptr);
  if (rc != 0) {
    memset(out, 0, 6);
    return;
  }
  // NimBLE returns the address LSB-first; ESPHome's MAC helpers expect
  // MSB-first (matches ESP32BLE::get_mac_msb_first's documented contract).
  for (int i = 0; i < 6; i++) {
    out[i] = addr[5 - i];
  }
}

}  // namespace esphome::nimble_ble

#endif
