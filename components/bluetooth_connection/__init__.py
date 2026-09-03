"""SURCHARGE de esphome/components/bluetooth_connection -- ESP32/NimBLE only
(this project targets ESP32 exclusively: no RP2, no Bluedroid -- see
docs/ARCHITECTURE.md).

M3 scope: only the GATT client backend itself (NimbleGattClient, satisfying
ble_device_base::BLEGattConnectionContract) is vendored here, exposed via a
standalone `bluetooth_connection:` config key for compile/hardware testing.
bluetooth_connection_hub.h/.cpp (the bluetooth_proxy-facing wrapper) is M4
scope -- not needed to prove the backend contract or the bounded discovery
timeout on real hardware.
"""

import esphome.codegen as cg
from esphome.components import ble_device_base
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@kamahat"]
AUTO_LOAD = ["ble_device_base"]
DEPENDENCIES = ["esp32"]

bluetooth_connection_ns = cg.esphome_ns.namespace("bluetooth_connection")
NimbleGattClient = bluetooth_connection_ns.class_("NimbleGattClient", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NimbleGattClient),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    ble_device_base.request_gatt_client()
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
