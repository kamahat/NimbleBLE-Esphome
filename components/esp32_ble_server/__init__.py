"""SURCHARGE de esphome/components/esp32_ble_server -- backend NimBLE.

Portée M5 réduite par rapport au core (voir docs/OVERRIDE_CAVEATS.md) : pas de
service Device Information auto-généré (manufacturer/firmware_version/etc. --
un utilisateur peut le déclarer lui-même comme un service normal), pas de
génération automatique de CUD (Characteristic User Description) ni de CCCD
explicite (NimBLE gère la CCCD nativement pour notify/indicate -- voir
ble_descriptor.h ; un 0x2902 explicitement déclaré est accepté mais exclu de
la table réellement enregistrée), pas de valeurs templatées (lambda) pour
`value:` (valeur statique uniquement -- liste d'octets ou chaîne), pas
d'options d'encodage avancées (string_encoding/endianness) ni d'appearance
BLE. `ble_server.characteristic.set_value`/`ble_server.characteristic.notify`/
`ble_server.descriptor.set_value` restent disponibles pour les mises à jour
dynamiques via automation.
"""

from esphome import automation
import esphome.codegen as cg
from esphome.components import esp32_ble
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_ON_CONNECT,
    CONF_ON_DISCONNECT,
    CONF_UUID,
    CONF_VALUE,
)

AUTO_LOAD = ["esp32_ble", "ble_device_base", "bytebuffer"]
DEPENDENCIES = ["esp32"]
CODEOWNERS = ["@kamahat"]

CONF_SERVICES = "services"
CONF_ADVERTISE = "advertise"
CONF_CHARACTERISTICS = "characteristics"
CONF_DESCRIPTORS = "descriptors"
CONF_READ = "read"
CONF_WRITE = "write"
CONF_WRITE_NO_RESPONSE = "write_no_response"
CONF_NOTIFY = "notify"
CONF_INDICATE = "indicate"
CONF_BROADCAST = "broadcast"
CONF_MAX_LENGTH = "max_length"
CONF_MAX_CLIENTS = "max_clients"
CONF_MANUFACTURER_DATA = "manufacturer_data"
CONF_ON_WRITE = "on_write"

esp32_ble_server_ns = cg.esphome_ns.namespace("esp32_ble_server")
esp32_ble_server_automations_ns = esp32_ble_server_ns.namespace("esp32_ble_server_automations")

BLEServer = esp32_ble_server_ns.class_("BLEServer", cg.Component, cg.Parented.template(esp32_ble.ESP32BLE))
BLEService = esp32_ble_server_ns.class_("BLEService")
BLECharacteristic = esp32_ble_server_ns.class_("BLECharacteristic")
BLEDescriptor = esp32_ble_server_ns.class_("BLEDescriptor")
# Separate namespace handles (not class_()) for static member/method access
# (PROPERTY_* constants, BLETriggers::create_*) -- calling `.foo` on a
# class_() MockObjClass emits an instance-member `.` access, not `::`; only a
# namespace() object's attribute access renders the `::` a static reference
# needs. BLECharacteristic/BLETriggers above stay class_() for use_id/typing.
BLECharacteristic_ns = esp32_ble_server_ns.namespace("BLECharacteristic")
BLETriggers_ns = esp32_ble_server_automations_ns.namespace("BLETriggers")
BLECharacteristicSetValueAction = esp32_ble_server_automations_ns.class_(
    "BLECharacteristicSetValueAction", automation.Action
)
BLECharacteristicNotifyAction = esp32_ble_server_automations_ns.class_(
    "BLECharacteristicNotifyAction", automation.Action
)
BLEDescriptorSetValueAction = esp32_ble_server_automations_ns.class_(
    "BLEDescriptorSetValueAction", automation.Action
)

from esphome.components import ble_device_base  # noqa: E402

bt_uuid = ble_device_base.bt_uuid
ble_device_base_ns = cg.esphome_ns.namespace("ble_device_base")
ESPBTUUID_ns = ble_device_base_ns.namespace("ESPBTUUID")


def parse_uuid(uuid_str):
    # ESPBTUUID::from_raw(const std::string &) parses the 4/8/36-char text
    # form (short hex or dashed 128-bit) directly -- see ble_device.cpp.
    return ESPBTUUID_ns.from_raw(uuid_str)


def parse_properties(char_conf):
    # `|=` on a MockObj invokes MockObj.__ior__, which renders a literal C++
    # assignment operator -- invalid as a nested sub-expression here. `props
    # = props | x` instead uses __or__/__ror__, building a plain "a | b | c"
    # expression, which is what a constructor argument needs.
    props = 0
    if char_conf[CONF_READ]:
        props = props | BLECharacteristic_ns.PROPERTY_READ
    if char_conf[CONF_WRITE]:
        props = props | BLECharacteristic_ns.PROPERTY_WRITE
    if char_conf[CONF_WRITE_NO_RESPONSE]:
        props = props | BLECharacteristic_ns.PROPERTY_WRITE_NR
    if char_conf[CONF_NOTIFY]:
        props = props | BLECharacteristic_ns.PROPERTY_NOTIFY
    if char_conf[CONF_INDICATE]:
        props = props | BLECharacteristic_ns.PROPERTY_INDICATE
    if char_conf[CONF_BROADCAST]:
        props = props | BLECharacteristic_ns.PROPERTY_BROADCAST
    return props


def parse_value(value_config):
    """Static value only for v1: a byte list or a plain string.

    Plain Python str/list values pass through cg.add()'s own safe_exp() call
    unmodified -- a Python str becomes a C++ string literal, which implicitly
    converts to std::string at the set_value(const std::string &) overload.
    """
    if isinstance(value_config, str):
        return value_config
    return cg.ArrayInitializer(*value_config)


DESCRIPTOR_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEDescriptor),
        cv.Required(CONF_UUID): bt_uuid,
        cv.Optional(CONF_MAX_LENGTH, default=100): cv.int_range(min=1, max=512),
        cv.Optional(CONF_READ, default=True): cv.boolean,
        cv.Optional(CONF_WRITE, default=True): cv.boolean,
        cv.Optional(CONF_VALUE): cv.Any(cv.string, cv.ensure_list(cv.hex_uint8_t)),
        cv.Optional(CONF_ON_WRITE): automation.validate_automation(single=True),
    }
)

CHARACTERISTIC_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLECharacteristic),
        cv.Required(CONF_UUID): bt_uuid,
        cv.Optional(CONF_READ, default=False): cv.boolean,
        cv.Optional(CONF_WRITE, default=False): cv.boolean,
        cv.Optional(CONF_WRITE_NO_RESPONSE, default=False): cv.boolean,
        cv.Optional(CONF_NOTIFY, default=False): cv.boolean,
        cv.Optional(CONF_INDICATE, default=False): cv.boolean,
        cv.Optional(CONF_BROADCAST, default=False): cv.boolean,
        cv.Optional(CONF_VALUE): cv.Any(cv.string, cv.ensure_list(cv.hex_uint8_t)),
        cv.Optional(CONF_ON_WRITE): automation.validate_automation(single=True),
        cv.Optional(CONF_DESCRIPTORS, default=[]): cv.ensure_list(DESCRIPTOR_SCHEMA),
    }
)

SERVICE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEService),
        cv.Required(CONF_UUID): bt_uuid,
        cv.Optional(CONF_ADVERTISE, default=False): cv.boolean,
        cv.Optional(CONF_CHARACTERISTICS, default=[]): cv.ensure_list(CHARACTERISTIC_SCHEMA),
    }
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(BLEServer),
        cv.GenerateID(esp32_ble.CONF_BLE_ID): cv.use_id(esp32_ble.ESP32BLE),
        cv.Optional(CONF_MAX_CLIENTS, default=1): cv.int_range(min=1, max=9),
        cv.Optional(CONF_MANUFACTURER_DATA): cv.ensure_list(cv.hex_uint8_t),
        cv.Required(CONF_SERVICES): cv.ensure_list(SERVICE_SCHEMA),
        cv.Optional(CONF_ON_CONNECT): automation.validate_automation(single=True),
        cv.Optional(CONF_ON_DISCONNECT): automation.validate_automation(single=True),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code_descriptor(descriptor_conf, char_var):
    desc_var = cg.new_Pvariable(
        descriptor_conf[CONF_ID],
        parse_uuid(descriptor_conf[CONF_UUID]),
        descriptor_conf[CONF_MAX_LENGTH],
        descriptor_conf[CONF_READ],
        descriptor_conf[CONF_WRITE],
    )
    cg.add(char_var.add_descriptor(desc_var))
    if CONF_VALUE in descriptor_conf:
        cg.add(desc_var.set_value(parse_value(descriptor_conf[CONF_VALUE])))
    if CONF_ON_WRITE in descriptor_conf:
        cg.add_define("USE_ESP32_BLE_SERVER_DESCRIPTOR_ON_WRITE")
        await automation.build_automation(
            BLETriggers_ns.create_descriptor_on_write_trigger(desc_var),
            [(cg.std_vector.template(cg.uint8), "x"), (cg.uint16, "id")],
            descriptor_conf[CONF_ON_WRITE],
        )


async def to_code_characteristic(service_var, char_conf):
    char_var = cg.Pvariable(
        char_conf[CONF_ID],
        service_var.create_characteristic(parse_uuid(char_conf[CONF_UUID]), parse_properties(char_conf)),
    )
    if CONF_VALUE in char_conf:
        cg.add(char_var.set_value(parse_value(char_conf[CONF_VALUE])))
    if CONF_ON_WRITE in char_conf:
        cg.add_define("USE_ESP32_BLE_SERVER_CHARACTERISTIC_ON_WRITE")
        await automation.build_automation(
            BLETriggers_ns.create_characteristic_on_write_trigger(char_var),
            [(cg.std_vector.template(cg.uint8), "x"), (cg.uint16, "id")],
            char_conf[CONF_ON_WRITE],
        )
    for descriptor_conf in char_conf[CONF_DESCRIPTORS]:
        await to_code_descriptor(descriptor_conf, char_var)


async def to_code(config):
    cg.add_define("USE_ESP32_BLE_SERVER")

    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    parent = await cg.get_variable(config[esp32_ble.CONF_BLE_ID])
    cg.add(var.set_parent(parent))
    cg.add(var.set_max_clients(config[CONF_MAX_CLIENTS]))
    if CONF_MANUFACTURER_DATA in config:
        cg.add(var.set_manufacturer_data(config[CONF_MANUFACTURER_DATA]))

    for service_config in config[CONF_SERVICES]:
        service_var = cg.Pvariable(
            service_config[CONF_ID],
            var.create_service(parse_uuid(service_config[CONF_UUID]), service_config[CONF_ADVERTISE]),
        )
        for char_conf in service_config[CONF_CHARACTERISTICS]:
            await to_code_characteristic(service_var, char_conf)

    if CONF_ON_CONNECT in config:
        cg.add_define("USE_ESP32_BLE_SERVER_ON_CONNECT")
        await automation.build_automation(
            BLETriggers_ns.create_server_on_connect_trigger(var), [(cg.uint16, "id")], config[CONF_ON_CONNECT]
        )
    if CONF_ON_DISCONNECT in config:
        cg.add_define("USE_ESP32_BLE_SERVER_ON_DISCONNECT")
        await automation.build_automation(
            BLETriggers_ns.create_server_on_disconnect_trigger(var), [(cg.uint16, "id")], config[CONF_ON_DISCONNECT]
        )


BLE_SERVER_CHARACTERISTIC_SET_VALUE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(BLECharacteristic),
        # BLECharacteristicSetValueAction::set_buffer() only overloads
        # std::initializer_list<uint8_t> and ByteBuffer -- no std::string
        # overload (unlike BLECharacteristic::set_value() itself) -- so
        # actions are byte-list-only in this v1 scope, matching set_buffer.
        cv.Required(CONF_VALUE): cv.ensure_list(cv.hex_uint8_t),
    }
)


@automation.register_action(
    "ble_server.characteristic.set_value",
    BLECharacteristicSetValueAction,
    BLE_SERVER_CHARACTERISTIC_SET_VALUE_SCHEMA,
    synchronous=True,
)
async def ble_server_characteristic_set_value(config, action_id, template_arg, args):
    cg.add_define("USE_ESP32_BLE_SERVER_SET_VALUE_ACTION")
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(var.set_buffer(parse_value(config[CONF_VALUE])))
    return var


BLE_SERVER_CHARACTERISTIC_NOTIFY_SCHEMA = cv.Schema({cv.Required(CONF_ID): cv.use_id(BLECharacteristic)})


@automation.register_action(
    "ble_server.characteristic.notify",
    BLECharacteristicNotifyAction,
    BLE_SERVER_CHARACTERISTIC_NOTIFY_SCHEMA,
    synchronous=True,
)
async def ble_server_characteristic_notify(config, action_id, template_arg, args):
    cg.add_define("USE_ESP32_BLE_SERVER_NOTIFY_ACTION")
    parent = await cg.get_variable(config[CONF_ID])
    return cg.new_Pvariable(action_id, template_arg, parent)


BLE_SERVER_DESCRIPTOR_SET_VALUE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_ID): cv.use_id(BLEDescriptor),
        cv.Required(CONF_VALUE): cv.ensure_list(cv.hex_uint8_t),
    }
)


@automation.register_action(
    "ble_server.descriptor.set_value",
    BLEDescriptorSetValueAction,
    BLE_SERVER_DESCRIPTOR_SET_VALUE_SCHEMA,
    synchronous=True,
)
async def ble_server_descriptor_set_value(config, action_id, template_arg, args):
    cg.add_define("USE_ESP32_BLE_SERVER_DESCRIPTOR_SET_VALUE_ACTION")
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    cg.add(var.set_buffer(parse_value(config[CONF_VALUE])))
    return var
