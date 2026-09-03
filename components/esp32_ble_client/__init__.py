"""SURCHARGE de esphome/components/esp32_ble_client -- backend NimBLE.

Bibliothèque interne (pas de CONFIG_SCHEMA, comme le core) : BLEClientBase
connexion/découverte/lecture/écriture GATT, sur le moteur partagé
nimble_ble.NimbleGattEngine plutôt que sur les callbacks Bluedroid du core --
voir ble_client_base.h. `ble_client:` (composant séparé, la surface YAML)
en hérite.
"""

import esphome.codegen as cg

AUTO_LOAD = ["ble_device_base"]
CODEOWNERS = ["@kamahat"]
DEPENDENCIES = ["esp32"]

esp32_ble_client_ns = cg.esphome_ns.namespace("esp32_ble_client")
BLEClientBase = esp32_ble_client_ns.class_("BLEClientBase", cg.Component)

# No CONFIG_SCHEMA / to_code here (matches core): this component is never a
# YAML key on its own, only AUTO_LOADed by ble_client, which is where
# ble_device_base.request_gatt_client() actually gets called (a to_code()
# here would never run to make that call -- nothing instantiates this
# module directly). See ble_client/__init__.py.
