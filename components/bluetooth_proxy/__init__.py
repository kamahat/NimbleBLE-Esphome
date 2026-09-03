"""SURCHARGE de esphome/components/bluetooth_proxy -- ESP32/NimBLE only (ce
projet cible exclusivement ESP32, voir docs/ARCHITECTURE.md). Le
bluetooth_proxy.h/.cpp core est vendoré tel quel (zéro dépendance
Bluedroid/esp_* confirmée par lecture directe) -- c'est bluetooth_connection
qui absorbe toute la différence NimBLE, via le contrat BLEGattConnectionContract
déjà satisfait depuis M3.

Simplifié par rapport au core : pas de dispatch multi-plateforme (rp2/bk72xx/
ln882x -- hors périmètre), pas de esp32_ble.consume_connection_slots() /
BTLoggers (spécifiques Bluedroid, sans équivalent porté -- voir
docs/OVERRIDE_CAVEATS.md), pas de cache_services (CONFIG_BT_GATTC_CACHE_NVS_FLASH
est une fonctionnalité Bluedroid ; SUPPORTS_CACHE_CLEARING=false côté
bluetooth_connection.h).
"""

import esphome.codegen as cg
from esphome.components import ble_device_base, bluetooth_connection
import esphome.config_validation as cv
from esphome.const import CONF_ACTIVE, CONF_ID
from esphome.types import ConfigType

AUTO_LOAD = ["bluetooth_connection", "esp32_ble_tracker"]
DEPENDENCIES = ["api", "esp32"]
CODEOWNERS = ["@kamahat"]

CONF_CONNECTION_SLOTS = "connection_slots"
CONF_CONNECTIONS = "connections"
DEFAULT_CONNECTION_SLOTS = 3
# Mirrors the CONFIG_BT_NIMBLE_MAX_CONNECTIONS ceiling esp32_ble sets --
# esp32_ble_client connections share the same NimBLE connection budget and
# are not accounted for here (no cross-component slot bookkeeping in v1, see
# docs/OVERRIDE_CAVEATS.md): a config combining ble_client: instances with a
# bluetooth_proxy connection_slots count that together exceed this ceiling
# fails at the NimBLE host level at runtime, not at compile time.
MAX_CONNECTION_SLOTS = 9

bluetooth_proxy_ns = cg.esphome_ns.namespace("bluetooth_proxy")
BluetoothProxy = bluetooth_proxy_ns.class_("BluetoothProxy", cg.Component)

_CONNECTION_SCHEMA = bluetooth_connection.hub_connection_schema()


def _populate_connections(config: ConfigType) -> ConfigType:
    """Expand connection_slots into `connections` entries here, during schema
    validation -- not in to_code(). declare_id() (inside hub_connection_schema())
    only registers a fresh id in CORE.component_ids as a side effect of running
    within the normal top-down validation pass; calling it later, at codegen
    time, produces an id cg.register_component() cannot find (confirmed: it
    raised "Component ID  was not declared to inherit from Component" -- the
    blank id is str(var.base) failing to resolve). Mirrors core's own
    validate_connections()."""
    if not config[CONF_ACTIVE] or config[CONF_CONNECTION_SLOTS] == 0:
        return config
    return {
        **config,
        CONF_CONNECTIONS: [_CONNECTION_SCHEMA({}) for _ in range(config[CONF_CONNECTION_SLOTS])],
    }


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(BluetoothProxy),
            cv.Optional(CONF_ACTIVE, default=True): cv.boolean,
            cv.Optional(
                CONF_CONNECTION_SLOTS, default=DEFAULT_CONNECTION_SLOTS
            ): cv.All(cv.positive_int, cv.Range(min=0, max=MAX_CONNECTION_SLOTS)),
        }
    )
    .extend(ble_device_base.BLE_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA),
    _populate_connections,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_active(config[CONF_ACTIVE]))
    hub = await cg.get_variable(config[ble_device_base.CONF_BLE_HUB_ID])
    cg.add(var.set_ble_hub(hub))
    # Compiles the scanner-state push slot into the tracker and the matching
    # registration into the proxy; our tracker provides it (USE_ESP32_BLE_TRACKER
    # arm of ble_hub_impl.h), matching the core esp32 arm's own define.
    cg.add_define("USE_BLE_SCANNER_STATE_CALLBACK")

    connections = config.get(CONF_CONNECTIONS, [])
    cg.add_define("BLUETOOTH_PROXY_MAX_CONNECTIONS", len(connections))
    if connections:
        # Gates the connection and GATT half of the API surface. A proxy
        # without slots omits this, so a client never sends those requests
        # and their handlers/encoders are dead.
        cg.add_define("USE_BLUETOOTH_PROXY_CONNECTIONS")
        for connection_conf in connections:
            backend = await bluetooth_connection.new_gatt_backend(connection_conf)
            connection = cg.new_Pvariable(connection_conf[CONF_ID])
            cg.add(connection.set_backend(backend))
            cg.add(var.register_connection(connection))

    # Each advertisement is up to ~80 bytes packaged (protocol overhead
    # included); 16 * 80 = 1280 bytes, ~97% of the ~1320-byte usable WiFi MTU
    # payload -- matches core's own sizing rationale.
    cg.add_define("BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE", 16)
    cg.add_define("USE_BLUETOOTH_PROXY")
