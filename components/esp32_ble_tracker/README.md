# components/esp32_ble_tracker — SURCHARGE de `esphome/components/esp32_ble_tracker`

**Statut : implémenté, M2 + durcissement M7.** Démarrage/arrêt du scan et fan-out vers les
`ESPBTDeviceListener` enregistrés (logique lourde GATT/scan partagée vit dans
`ble_device_base`).

## M7 -- adv_queue (docs/SECURITY.md)

`BLE_GAP_EVENT_DISC`/`BLE_GAP_EVENT_DISC_COMPLETE` arrivent sur la tâche hôte NimBLE ; ils
ne touchent plus `ScanResponseMerger`/`AdvDispatcher` (donc les `ESPBTDeviceListener`
enregistrés, potentiellement `bluetooth_proxy` -> API/socket ESPHome) directement depuis
cette tâche -- `adv_queue.h/.cpp` marshalle vers la boucle principale, même patron que
`nimble_event.h` (client)/`nimble_server_event.h` (serveur). File bornée (64), compteur de
drops exposé :

```yaml
sensor:
  - platform: template
    name: BLE adv drops
    entity_category: diagnostic
    lambda: |-
      return id(the_tracker).get_adv_queue_dropped_count();
```
