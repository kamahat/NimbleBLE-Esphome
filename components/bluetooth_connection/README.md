# components/bluetooth_connection — SURCHARGE de `esphome/components/bluetooth_connection`

**Statut : squelette, M3-M4 — jalon central du chemin `bluetooth_proxy`/HA.** Découvert
tardivement lors du recon M0 (absent de la liste initiale des 7 répertoires) : c'est ici,
pas dans `bluetooth_proxy/`, que vit le vrai point d'insertion pour remplacer Bluedroid par
NimBLE côté proxy HA.

Fichiers confirmés côté core (commit figé, voir `docs/ARCHITECTURE.md`) :
`bluetooth_connection.cpp/.h` (classe de base commune), `bluetooth_connection_bluedroid.cpp/.h`
(`BluedroidGattClient`, à ne pas reprendre mais à égaler fonctionnellement),
`bluetooth_connection_gatt_backend.h` (le binding `#if defined(...)` — notre override ajoute
un bras pour notre backend NimBLE), `bluetooth_connection_hub.cpp/.h` (namespace
`esphome::bluetooth_proxy` malgré l'emplacement — chunking `GATTGetServicesResponse`),
`bluetooth_connection_rp2.cpp/.h` (référence de style, backend BTstack RP2040 — pas concerné
par notre override mais utile comme modèle).

À écrire : `bluetooth_connection_nimble.cpp/.h` — `NimbleGattClient`, implémentant le
concept `BLEGattConnectionContract` de `ble_device_base/ble_gatt_client.h` (interface exacte
confirmée : `set_listener`, `connect(uint64_t, uint8_t)`, `gatt_disconnect()`,
`cancel_gatt_disconnect()`, `discover_services()`, `read_characteristic(uint16_t)`,
`write_characteristic(uint16_t, const uint8_t*, uint16_t, bool)`, `read_descriptor(uint16_t)`,
`write_descriptor(uint16_t, const uint8_t*, uint16_t)`, `notify_characteristic(uint16_t, bool)`,
`pair()`, `update_connection_params(uint16_t, uint16_t, uint16_t, uint16_t)`,
`get_service_table()`, `release_services()`, `set_connection_type(ConnectionType)`) contre
`components/nimble_ble/nimble_gattc.cpp`.

**`ble_device_base` n'est PAS surchargé** (voir `docs/ARCHITECTURE.md`, section Découverte
majeure) — le contrat `BLEGattConnectionContract` y est un concept C++20 platform-neutre,
satisfait directement par notre nouveau backend sans toucher à ce répertoire.
