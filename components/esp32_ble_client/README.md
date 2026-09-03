# components/esp32_ble_client — SURCHARGE de `esphome/components/esp32_ble_client`

**Statut : TERMINÉ 2026-09-03, validé matériel.** `BLEClientBase` (connexion, découverte,
lecture/écriture GATT côté client) sur le moteur partagé `nimble_ble::NimbleGattEngine` --
pas de `gattc_event_handler`/`gap_event_handler` Bluedroid, pas d'enregistrement comme
`ESPBTClient` auprès du tracker (NimBLE compose `ble_gap_connect()` directement par adresse,
sans scan préalable). Le timeout borné de découverte vient de
`components/nimble_ble/ble_connection_fsm` (embarquée dans `NimbleGattEngine`) -- le correctif
direct du hang Bluedroid documenté dans `docs/HARDWARE_VALIDATION.md`
(`esp_ble_gattc_search_service()` sans deadline propre).

Critère de sortie M3 confirmé sur ESP32-C5 réel (`tests/components/ble_client/
test.esp32c5-idf.yaml`, adresse volontairement injoignable) : sortie bornée
(`on_connection_state(error=BLE_HS_ETIMEOUT)`) au lieu d'un hang, plus la boucle de
reconnexion à délai fixe de `ble_client::BLEClient` -- 3 cycles complets observés dans une
seule capture série, `on_disconnect` déclenché exactement une fois par cycle.

Portée sciemment non reprise (voir `docs/OVERRIDE_CAVEATS.md`) : pairing/passkey,
`ble_client.ble_write` déclaratif, type d'adresse appris par scan (PUBLIC toujours supposé),
plateformes sensor/switch/text_sensor consommant `BLEClientNode`, backoff exponentiel
(prévu M7).
