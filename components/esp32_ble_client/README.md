# components/esp32_ble_client — SURCHARGE de `esphome/components/esp32_ble_client`

**Statut : squelette, M3 — jalon central du projet.** `BLEClientBase` : connexion,
découverte, lecture/écriture GATT côté client. C'est ici que vit le **timeout borné de
découverte** piloté par `components/nimble_ble/nimble_fsm/ble_connection_fsm` — le correctif
direct du hang Bluedroid documenté dans `docs/HARDWARE_VALIDATION.md` (`reason=0x13` à 30 s
pile, jamais de deadline côté device aujourd'hui).

Critère de sortie M3 : un test matériel forçant un périphérique injoignable pendant la
découverte confirme une sortie bornée (transition `discover_timeout` → `Backoff`) au lieu
d'un hang.
