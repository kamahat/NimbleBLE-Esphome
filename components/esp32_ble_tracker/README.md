# components/esp32_ble_tracker — SURCHARGE de `esphome/components/esp32_ble_tracker`

**Statut : squelette, M2.** Côté core sur le commit figé, ce composant est déjà réduit à un
fin wrapper (`__init__.py`, `automation.h`, `esp32_ble_tracker.cpp/.h` — confirmé, voir
`docs/ARCHITECTURE.md`) : démarrage/arrêt du scan et fan-out vers les `ESPBTDeviceListener`
enregistrés. La logique lourde (GATT/scan partagé) vit désormais dans `ble_device_base`.
