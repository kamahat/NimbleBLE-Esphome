# components/ble_client — SURCHARGE de `esphome/components/ble_client`

**Statut : TERMINÉ 2026-09-03, validé matériel.** Surface YAML `ble_client:` (mac_address,
auto_connect, on_connect/on_disconnect, actions `ble_client.connect`/`ble_client.disconnect`)
sur `esp32_ble_client::BLEClientBase`. `BLEClientNode` simplifié par rapport au core :
`on_ble_client_connected()`/`on_ble_client_disconnected()` au lieu de
`gattc_event_handler`/`gap_event_handler` façon Bluedroid — `ble_client::BLEClient::set_state()`
traduit la `ClientState` neutre (`ble_device_base`) en notifications aux noeuds enregistrés.

Portée sciemment non reprise (voir `docs/OVERRIDE_CAVEATS.md`) : pairing/passkey,
`ble_client.ble_write` déclaratif (équivalent via lambda :
`id(mon_client).engine().write_characteristic(...)`), actions connect/disconnect "complexes"
(les nôtres sont fire-and-forget, pas `play_complex_`), plateformes sensor/switch/text_sensor.

Validé sur ESP32-C5 réel (`tests/components/ble_client/test.esp32c5-idf.yaml`, adresse
volontairement injoignable) : timeout borné + boucle de reconnexion à délai fixe, 3 cycles
complets observés dans une seule capture série.
