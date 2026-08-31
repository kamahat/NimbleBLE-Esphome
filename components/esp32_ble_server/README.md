# components/esp32_ble_server — SURCHARGE de `esphome/components/esp32_ble_server`

**Statut : squelette, M5.** Rôle GATT serveur/périphérique — dans le périmètre v1 (décision
utilisateur : les deux rôles client ET serveur dès la première version, pas seulement le
cas d'usage scan+connect vers le verrou Boks). Fichiers attendus côté core (à confirmer sur
le commit figé) : `ble_server.cpp/.h`, `ble_service.cpp/.h`, `ble_characteristic.cpp/.h`,
`ble_descriptor.cpp/.h`, `ble_2902.cpp/.h`, `ble_server_automations.cpp/.h`.
