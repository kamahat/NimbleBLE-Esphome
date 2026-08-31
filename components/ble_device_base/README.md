# components/ble_device_base — SURCHARGE de `esphome/components/ble_device_base`

**Statut : squelette, M1-M3.** Implémente `BLEGattConnectionContract` (défini par
`ble_gatt_client.h` côté core — interface `GattClientListener` : `on_connection_state`,
`on_service_discovery_done`, `on_read_result`, `on_write_result`, `on_notify_state`,
`on_notify_data`, `on_pairing_result`) contre `components/nimble_ble/nimble_gattc.cpp` au
lieu de Bluedroid.

Fichiers à porter (confirmés présents côté core sur le commit figé, voir
`docs/ARCHITECTURE.md`) : `ble_device.cpp/.h`, `ble_client_state.cpp/.h`, `ble_gatt_client.h`
(interface, à implémenter par un nouveau `ble_gatt_client_nimble.cpp/.h`), `ble_aes_ccm.cpp/.h`
(pure crypto — vendorer à l'identique sauf si upstream a changé l'algorithme),
`scan_response_merger.cpp/.h` (pure logique — idem).
