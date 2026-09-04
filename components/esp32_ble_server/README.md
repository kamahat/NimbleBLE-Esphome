# components/esp32_ble_server — SURCHARGE de `esphome/components/esp32_ble_server`

**Statut : implémenté, M5.** Rôle GATT serveur/périphérique sur NimBLE.

Écart architectural face au serveur Bluedroid du core : NimBLE enregistre une table
**statique** de services/caractéristiques/descripteurs en un seul appel
(`ble_gatts_count_cfg` + `ble_gatts_add_svcs` + `ble_gatts_start`), alors que Bluedroid
crée chaque service de façon asynchrone. Comme la config `esp32_ble_server:` est connue
entièrement à la compilation, `BLEServer::setup()` construit la table complète une seule
fois au lieu de reproduire la machine à états asynchrone du core — voir
`docs/ARCHITECTURE.md`.

Fichiers : `nimble_server_event.h/.cpp` (file d'événements thread-safe host-task → boucle
principale), `ble_descriptor.h/.cpp`, `ble_2902.h/.cpp`, `ble_characteristic.h/.cpp`,
`ble_service.h/.cpp`, `ble_server.h/.cpp`, `ble_server_automations.h/.cpp` (vendored
verbatim depuis le core — confirmé neutre vis-à-vis de Bluedroid).

Portée v1 réduite face au core — voir `docs/OVERRIDE_CAVEATS.md` : pas de service Device
Information auto-généré, pas de CUD/CCCD sugar, pas de valeurs templatées (lambda) pour
`value:`, pas d'options d'encodage avancées ni d'appearance BLE.

Test de compilation : `tests/components/esp32_ble_server/test.esp32c5-idf.yaml` (ESP32-C5,
vert sur claude-mgmt). Vérification matérielle (un central BLE réel se connectant au
serveur exposé) : pas encore faite — prochaine étape.
