"""SURCHARGE de esphome/components/ble_client -- backend NimBLE.

Portée M3 réduite par rapport au core (voir docs/OVERRIDE_CAVEATS.md) : pas de
pairing/passkey (on_passkey_request, on_passkey_notification,
on_numeric_comparison_request, ble_client.passkey_reply,
ble_client.numeric_comparison_reply, ble_client.remove_bond), pas de
ble_client.ble_write déclaratif (utiliser id(mon_client).engine().
write_characteristic(...) dans un lambda), pas de type d'adresse appris par
scan (PUBLIC toujours supposé -- esp32_ble_client/ble_client_base.h). Pas non
plus de plateformes sensor/switch/text_sensor consommant BLEClientNode --
hors périmètre de ce projet (voir docs/ARCHITECTURE.md, M3).
"""

from esphome import automation
from esphome.automation import maybe_simple_id
import esphome.codegen as cg
from esphome.components import ble_device_base, esp32_ble_client
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_MAC_ADDRESS,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_TRIGGER_ID,
)

AUTO_LOAD = ["esp32_ble_client"]
CODEOWNERS = ["@kamahat"]
DEPENDENCIES = ["esp32"]
MULTI_CONF = True

CONF_AUTO_CONNECT = "auto_connect"

ble_client_ns = cg.esphome_ns.namespace("ble_client")
BLEClient = ble_client_ns.class_("BLEClient", esp32_ble_client.BLEClientBase)
BLEClientNode = ble_client_ns.class_("BLEClientNode")

BLEClientConnectTrigger = ble_client_ns.class_(
    "BLEClientConnectTrigger", automation.Trigger.template()
)
BLEClientDisconnectTrigger = ble_client_ns.class_(
    "BLEClientDisconnectTrigger", automation.Trigger.template()
)
BLEClientConnectAction = ble_client_ns.class_("BLEClientConnectAction", automation.Action)
BLEClientDisconnectAction = ble_client_ns.class_(
    "BLEClientDisconnectAction", automation.Action
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEClient),
        cv.Required(CONF_MAC_ADDRESS): cv.mac_address,
        cv.Optional(CONF_AUTO_CONNECT, default=True): cv.boolean,
        cv.Optional(CONF_ON_CONNECT): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEClientConnectTrigger)}
        ),
        cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(
            {cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(BLEClientDisconnectTrigger)}
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

CONF_BLE_CLIENT_ID = "ble_client_id"
BLE_CLIENT_SCHEMA = cv.Schema({cv.GenerateID(CONF_BLE_CLIENT_ID): cv.use_id(BLEClient)})


async def register_ble_node(var, config):
    """For future sensor/switch/text_sensor platforms binding to a BLEClient
    (not yet ported -- see docs/ARCHITECTURE.md M3): call this from their own
    to_code() the same way core's ble_sensor etc. do."""
    parent = await cg.get_variable(config[CONF_BLE_CLIENT_ID])
    cg.add(parent.register_ble_node(var))


BLE_CONNECT_ACTION_SCHEMA = maybe_simple_id({cv.GenerateID(CONF_ID): cv.use_id(BLEClient)})


@automation.register_action(
    "ble_client.connect", BLEClientConnectAction, BLE_CONNECT_ACTION_SCHEMA, synchronous=True
)
async def ble_connect_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


@automation.register_action(
    "ble_client.disconnect", BLEClientDisconnectAction, BLE_CONNECT_ACTION_SCHEMA, synchronous=True
)
async def ble_disconnect_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


async def to_code(config):
    ble_device_base.request_gatt_client()

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_address(config[CONF_MAC_ADDRESS].as_hex))
    cg.add(var.set_auto_connect(config[CONF_AUTO_CONNECT]))

    for conf in config.get(CONF_ON_CONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_DISCONNECT, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
