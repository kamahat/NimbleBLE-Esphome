# components/esp32_ble — SURCHARGE de `esphome/components/esp32_ble`

**Statut : squelette, M1.** Doit préserver la classe/namespace `esphome::esp32_ble::ESP32BLE`
et la surface publique du composant core (voir commit figé dans `docs/ARCHITECTURE.md`) pour
que tout le reste d'ESPHome core continue de compiler sans modification. `__init__.py` force
`CONFIG_BT_NIMBLE_ENABLED=y` / `CONFIG_BT_BLUEDROID_ENABLED=n` au lieu des options sdkconfig
Bluedroid du core, et échoue explicitement si un autre composant tente d'activer Bluedroid
(voir `components/nimble_ble/nimble_controller.cpp`).

Avant d'écrire le moindre code ici : confirmer la surface publique exacte de
`esphome/components/esp32_ble/` sur le commit ESPHome figé (M0 — pas encore fait, voir
`docs/ARCHITECTURE.md` § "encore à confirmer").
