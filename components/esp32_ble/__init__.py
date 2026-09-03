"""SURCHARGE de esphome/components/esp32_ble -- backend NimBLE au lieu de Bluedroid.

M1 : périmètre volontairement réduit par rapport au esp32_ble core (pas de
mode esp32_hosted, pas de tuning PSRAM Bluedroid-spécifique, pas encore de
IO capability / auth req étendue, pas de connection_timeout/max_notifications
sdkconfig -- ces options Bluedroid-spécifiques n'ont pas d'équivalent direct
NimBLE 1:1 et seront réévaluées en M2/M3 quand le GATT client/tracker arrive.
Documenté dans docs/OVERRIDE_CAVEATS.md.
"""
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.const import CONF_ID, CONF_ENABLE_ON_BOOT, CONF_NAME
from esphome.core import CORE, TimePeriod

AUTO_LOAD = ["ble_device_base", "nimble_ble"]
DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@kamahat"]

CONF_BLE_ID = "ble_id"
CONF_ADVERTISING = "advertising"
CONF_ADVERTISING_CYCLE_TIME = "advertising_cycle_time"

esp32_ble_ns = cg.esphome_ns.namespace("esp32_ble")
ESP32BLE = esp32_ble_ns.class_("ESP32BLE", cg.Component)

BLEEnabledCondition = esp32_ble_ns.class_("BLEEnabledCondition")
BLEEnableAction = esp32_ble_ns.class_("BLEEnableAction")
BLEDisableAction = esp32_ble_ns.class_("BLEDisableAction")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32BLE),
        cv.Optional(CONF_NAME): cv.All(cv.string, cv.Length(max=20)),
        cv.Optional(CONF_ENABLE_ON_BOOT, default=True): cv.boolean,
        cv.Optional(CONF_ADVERTISING, default=False): cv.boolean,
        cv.Optional(
            CONF_ADVERTISING_CYCLE_TIME, default="10s"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_enable_on_boot(config[CONF_ENABLE_ON_BOOT]))
    if (name := config.get(CONF_NAME)) is not None:
        cg.add(var.set_name(name))
    if config[CONF_ADVERTISING]:
        cg.add(var.set_advertising(True))
    await cg.register_component(var, config)

    # Force NimBLE, refuse Bluedroid -- the two are mutually exclusive at the
    # sdkconfig level (docs/ARCHITECTURE.md, "Fait déterminant").
    add_idf_sdkconfig_option("CONFIG_BT_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_NIMBLE_ENABLED", True)
    add_idf_sdkconfig_option("CONFIG_BT_BLUEDROID_ENABLED", False)
    add_idf_sdkconfig_option("CONFIG_BT_CONTROLLER_ENABLED", True)

    cg.add_define("USE_ESP32_BLE")
    # Discriminates our NimBLE backend from a Bluedroid one in
    # bluetooth_connection_gatt_backend.h -- USE_ESP32_BLE alone is not
    # enough, since core emits it for either stack.
    cg.add_define("USE_ESP32_BLE_NIMBLE")