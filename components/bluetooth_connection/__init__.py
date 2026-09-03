"""SURCHARGE de esphome/components/bluetooth_connection -- ESP32/NimBLE only
(ce projet cible exclusivement ESP32 : pas de RP2, pas de Bluedroid -- voir
docs/ARCHITECTURE.md).

Backend : NimbleGattClient (satisfait ble_device_base::BLEGattConnectionContract).
gatt_client_schema()/hub_connection_schema()/new_gatt_backend() sont l'API réelle
qu'appelle le codegen de bluetooth_proxy par slot de connexion configuré (M4).
Le CONFIG_SCHEMA/to_code `bluetooth_connection:` autonome plus bas date de M3
(test compile/matériel du backend seul) et reste en place comme test de
non-régression léger.
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
HubBluetoothConnection = bluetooth_connection_ns.class_("BluetoothConnection")

CONF_BACKEND_ID = "backend_id"


def gatt_client_schema() -> cv.Schema:
    """Schema fragment pour une instance de backend GATT : son id généré.
    ESP32/NimBLE uniquement, donc pas de dispatch par plateforme (contrairement
    au core, qui a aussi un bras rp2) -- une seule classe de backend possible
    dans ce build."""
    return cv.Schema({cv.GenerateID(CONF_BACKEND_ID): cv.declare_id(NimbleGattClient)})


def hub_connection_schema() -> cv.Schema:
    """Schema par slot pour les enveloppes de connexion du proxy : l'id de
    l'enveloppe en plus du fragment backend, plus les clés de composant
    (setup_priority et consorts s'appliquent au backend, le vrai Component du
    slot)."""
    return (
        gatt_client_schema()
        .extend({cv.GenerateID(): cv.declare_id(HubBluetoothConnection)})
        .extend(cv.COMPONENT_SCHEMA)
    )


async def new_gatt_backend(config) -> cg.MockObj:
    """Instancie le backend déclaré par gatt_client_schema() et l'enregistre
    comme composant. Le slot de connexion est réclamé à la validation (les
    connection_slots du proxy), pas ici."""
    ble_device_base.request_gatt_client()
    backend = cg.new_Pvariable(config[CONF_BACKEND_ID])
    await cg.register_component(backend, config)
    return backend


# ---- clé YAML autonome `bluetooth_connection:` (M3) : test compile/matériel
# du backend seul, gardé comme test de non-régression léger -- pas utilisé par
# bluetooth_proxy, qui pilote new_gatt_backend() directement. ----

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(NimbleGattClient),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    ble_device_base.request_gatt_client()
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
