"""SURCHARGE de esphome/components/esp32_ble_tracker -- backend NimBLE.

M2 : périmètre volontairement réduit par rapport au core -- pas de gestion
de clients GATT (arrive en M3, donc pas de max_connections/coex tuning ici),
pas d'intégration OTA (le NimBLE scan n'a pas besoin d'être coupé pendant un
OTA de la même façon que Bluedroid). Documenté dans docs/OVERRIDE_CAVEATS.md.

Différence assumée : cette surcharge n'appelle PAS
esp32_ble.register_gap_event_handler/register_gap_scan_event_handler/
register_gattc_event_handler/register_ble_status_event_handler -- ces
helpers n'existent pas dans notre esp32_ble (NimBLE n'a pas de dispatch GAP
central façon Bluedroid). Le tracker possède directement sa session
ble_gap_disc() NimBLE. Voir esp32_ble_tracker.h pour le détail.
"""

from esphome import automation
import esphome.codegen as cg
from esphome.components import ble_device_base, esp32_ble
from esphome.components.const import CONF_ON_SCAN_END, CONF_SCAN_PARAMETERS, CONF_WINDOW
import esphome.config_validation as cv
from esphome.const import (
    CONF_ACTIVE,
    CONF_CONTINUOUS,
    CONF_DURATION,
    CONF_ID,
    CONF_INTERVAL,
    CONF_MAC_ADDRESS,
    CONF_MANUFACTURER_ID,
    CONF_ON_BLE_ADVERTISE,
    CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE,
    CONF_ON_BLE_SERVICE_DATA_ADVERTISE,
    CONF_SERVICE_UUID,
    CONF_TRIGGER_ID,
)
from esphome.core import CORE

AUTO_LOAD = ["ble_device_base", "esp32_ble"]
DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@kamahat"]

ble_device_base.register_hub_provider("esp32_ble_tracker")

esp32_ble_tracker_ns = cg.esphome_ns.namespace("esp32_ble_tracker")
# ble_device_base.BLEHub listed as a base purely so cv.use_id(BLEHub)
# resolves an ESP32BLETracker instance (Python-side ID-matching only --
# BLEHubContract is a compile-time C++ concept, no virtual base needed
# there; discovered missing when bluetooth_proxy's BLE_DEVICE_SCHEMA first
# tried to cv.use_id(BLEHub) against our tracker, M4).
ESP32BLETracker = esp32_ble_tracker_ns.class_(
    "ESP32BLETracker", ble_device_base.BLEHub, cg.Component, cg.Parented.template(esp32_ble.ESP32BLE)
)
ESPBTDevice = ble_device_base.ble_device_base_ns.class_("ESPBTDevice")
ESPBTDeviceConstRef = ESPBTDevice.operator("ref").operator("const")
adv_data_t = cg.std_vector.template(cg.uint8)
adv_data_t_const_ref = adv_data_t.operator("ref").operator("const")

ESPBTAdvertiseTrigger = esp32_ble_tracker_ns.class_(
    "ESPBTAdvertiseTrigger", automation.Trigger.template(ESPBTDeviceConstRef)
)
BLEServiceDataAdvertiseTrigger = esp32_ble_tracker_ns.class_(
    "BLEServiceDataAdvertiseTrigger", automation.Trigger.template(adv_data_t_const_ref)
)
BLEManufacturerDataAdvertiseTrigger = esp32_ble_tracker_ns.class_(
    "BLEManufacturerDataAdvertiseTrigger",
    automation.Trigger.template(adv_data_t_const_ref),
)
BLEEndOfScanTrigger = esp32_ble_tracker_ns.class_(
    "BLEEndOfScanTrigger", automation.Trigger.template()
)
ESP32BLEStartScanAction = esp32_ble_tracker_ns.class_(
    "ESP32BLEStartScanAction", automation.Action
)
ESP32BLEStopScanAction = esp32_ble_tracker_ns.class_(
    "ESP32BLEStopScanAction", automation.Action
)

as_hex = ble_device_base.as_hex
as_reversed_hex_array = ble_device_base.as_reversed_hex_array
bt_uuid = ble_device_base.bt_uuid
bt_uuid16_format = ble_device_base.BT_UUID16_FORMAT
bt_uuid32_format = ble_device_base.BT_UUID32_FORMAT
bt_uuid128_format = ble_device_base.BT_UUID128_FORMAT

SCAN_PARAMETERS_SCHEMA = ble_device_base.scan_parameters_schema("320ms")

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32BLETracker),
        cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
        cv.Optional(CONF_SCAN_PARAMETERS, default={}): SCAN_PARAMETERS_SCHEMA,
        cv.Optional(CONF_ON_BLE_ADVERTISE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESPBTAdvertiseTrigger),
                cv.Optional(CONF_MAC_ADDRESS): cv.ensure_list(cv.mac_address),
            }
        ),
        cv.Optional(CONF_ON_BLE_SERVICE_DATA_ADVERTISE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    BLEServiceDataAdvertiseTrigger
                ),
                cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
                cv.Required(CONF_SERVICE_UUID): bt_uuid,
            }
        ),
        cv.Optional(
            CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE
        ): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    BLEManufacturerDataAdvertiseTrigger
                ),
                cv.Optional(CONF_MAC_ADDRESS): cv.mac_address,
                cv.Required(CONF_MANUFACTURER_ID): bt_uuid,
            }
        ),
        cv.Optional(CONF_ON_SCAN_END): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEEndOfScanTrigger)}
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    cg.add_define("USE_ESP32_BLE_TRACKER")
    cg.add_define("USE_BLE_SCAN_RESPONSE_MERGER")
    cg.add_define("USE_ESP32_BLE_DEVICE")
    cg.add_define("USE_ESP32_BLE_UUID")
    ble_device_base.request_irk_support()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    cg.add(var.set_parent(parent))

    params = config[CONF_SCAN_PARAMETERS]
    cg.add(var.set_scan_duration(params[CONF_DURATION]))
    cg.add(var.set_scan_interval(ble_device_base.to_ble_units(params[CONF_INTERVAL])))
    cg.add(var.set_scan_window(ble_device_base.to_ble_units(params[CONF_WINDOW])))
    cg.add(var.set_scan_active(params[CONF_ACTIVE]))
    cg.add(var.set_scan_continuous(params[CONF_CONTINUOUS]))

    for conf in config.get(CONF_ON_BLE_ADVERTISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if CONF_MAC_ADDRESS in conf:
            addr_list = [it.as_hex for it in conf[CONF_MAC_ADDRESS]]
            cg.add(trigger.set_addresses(addr_list))
        await automation.build_automation(trigger, [(ESPBTDeviceConstRef, "x")], conf)
        ble_device_base.request_listener_slot()
    for conf in config.get(CONF_ON_BLE_SERVICE_DATA_ADVERTISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if len(conf[CONF_SERVICE_UUID]) == len(bt_uuid16_format):
            cg.add(trigger.set_service_uuid16(as_hex(conf[CONF_SERVICE_UUID])))
        elif len(conf[CONF_SERVICE_UUID]) == len(bt_uuid32_format):
            cg.add(trigger.set_service_uuid32(as_hex(conf[CONF_SERVICE_UUID])))
        elif len(conf[CONF_SERVICE_UUID]) == len(bt_uuid128_format):
            cg.add(
                trigger.set_service_uuid128(
                    as_reversed_hex_array(conf[CONF_SERVICE_UUID])
                )
            )
        if CONF_MAC_ADDRESS in conf:
            cg.add(trigger.set_address(conf[CONF_MAC_ADDRESS].as_hex))
        await automation.build_automation(trigger, [(adv_data_t_const_ref, "x")], conf)
        ble_device_base.request_listener_slot()
    for conf in config.get(CONF_ON_BLE_MANUFACTURER_DATA_ADVERTISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        if len(conf[CONF_MANUFACTURER_ID]) == len(bt_uuid16_format):
            cg.add(trigger.set_manufacturer_uuid16(as_hex(conf[CONF_MANUFACTURER_ID])))
        elif len(conf[CONF_MANUFACTURER_ID]) == len(bt_uuid32_format):
            cg.add(trigger.set_manufacturer_uuid32(as_hex(conf[CONF_MANUFACTURER_ID])))
        elif len(conf[CONF_MANUFACTURER_ID]) == len(bt_uuid128_format):
            cg.add(
                trigger.set_manufacturer_uuid128(
                    as_reversed_hex_array(conf[CONF_MANUFACTURER_ID])
                )
            )
        if CONF_MAC_ADDRESS in conf:
            cg.add(trigger.set_address(conf[CONF_MAC_ADDRESS].as_hex))
        await automation.build_automation(trigger, [(adv_data_t_const_ref, "x")], conf)
        ble_device_base.request_listener_slot()
    for conf in config.get(CONF_ON_SCAN_END, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
        ble_device_base.request_listener_slot()
