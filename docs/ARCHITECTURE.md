# Architecture

## Commit ESPHome ciblé (M0)

**À figer définitivement en fin de M0.** Snapshot de référence utilisé pour la conception :
`esphome/esphome` commit `813c0006842681e1408d27e017897abd74a87b69` (branche par défaut,
2026-08-31).

Confirmé par lecture directe à cette date :
- `esphome/components/ble_device_base/` existe et contient : `__init__.py`, `automation.h`,
  `automation.py`, `ble_aes_ccm.cpp`/`.h`, `ble_client_state.cpp`/`.h`, `ble_device.cpp`/`.h`,
  `ble_gatt_client.h`, `ble_hub.h`, `ble_hub_impl.h`, `scan_response_merger.cpp`/`.h`.
  Ce composant porte le contrat `BLEGattConnectionContract` (interface `GattClientListener` :
  `on_connection_state`, `on_service_discovery_done`, `on_read_result`, `on_write_result`,
  `on_notify_state`, `on_notify_data`, `on_pairing_result` — extrait de `ble_gatt_client.h`).
- `esphome/components/esp32_ble_tracker/` est réduit à un fin wrapper : `__init__.py`,
  `automation.h`, `esp32_ble_tracker.cpp`/`.h`.
- `esphome/components/bluetooth_proxy/` contient `__init__.py`, `bluetooth_proxy.cpp`/`.h`
  (pas de fichier `bluetooth_connection.*` séparé à cette date — la logique de chunking GATT
  a été repliée dans `bluetooth_proxy.cpp`).

**Encore à confirmer en M0** (non lu dans cette session) : contenu exact de
`esphome/components/esp32_ble/`, `esp32_ble_client/`, `esp32_ble_server/`, `esp32_ble_beacon/`
sur ce même commit — nécessaire avant d'écrire le moindre override.

## Fait déterminant : pourquoi un override complet, pas un plugin

La pile BLE d'ESPHome (Bluedroid) et NimBLE sont mutuellement exclusives au niveau du
`sdkconfig` ESP-IDF (`CONFIG_BT_BLUEDROID_ENABLED` vs `CONFIG_BT_NIMBLE_ENABLED`). Le
composant `bluetooth_proxy`/`ble_device_base` sélectionne son backend GATT via un dispatch
**compile-time** (`#if defined(...) #else #error`) — ce n'est pas un point d'extension
runtime ni un point d'extension `external_component` au sens habituel (ajouter un nouveau
composant à côté). La seule voie sans PR upstream : **surcharger entièrement** les
répertoires concernés via `external_components:` (mécanisme confirmé : `esphome/loader.py`,
`ComponentMetaFinder` inséré en tête de `sys.meta_path` ; documenté dans
`external_components.mdx` ; démontré en pratique par `rjt-rockx/esphome-host-linux`, qui
surcharge exactement cette même chaîne).

## Répertoires à surcharger (override = remplacement intégral)

1. `esp32_ble` — init contrôleur/host NimBLE, advertising
2. `ble_device_base` — implémente `BLEGattConnectionContract` contre NimBLE
3. `esp32_ble_tracker` — scan, fan-out des listeners
4. `esp32_ble_client` — connexion/découverte/lecture/écriture GATT client ; **timeout borné
   de découverte** posé ici (correctif central, voir HARDWARE_VALIDATION.md)
5. `esp32_ble_server` — rôle GATT serveur/périphérique (dans le périmètre v1)
6. `esp32_ble_beacon`
7. `bluetooth_proxy` — intégration HA ; aucun code réseau propre, uniquement des handlers
   invoqués par `APIConnection` → hérite gratuitement de l'auth Noise tant qu'aucun listener
   parallèle n'est ouvert (voir SECURITY.md)

## Couche NimBLE partagée (nouveau, pas une surcharge)

`components/nimble_ble/` — bring-up contrôleur/host NimBLE (API ESP-IDF NimBLE brutes :
`nimble_port.h`, `host/ble_gap.h`, `host/ble_gattc.h`, `host/ble_gatts.h`), traduction des
callbacks C NimBLE en événements C++ typés, adaptateur UUID (`ESPBTUUID` ↔ NimBLE),
moteur de state machine généré (`nimble_fsm/`, voir plus bas).

## State machine de connexion/découverte

Voir `spec/transitions.json` (source unique de vérité) — états `Idle, Scanning, Connecting,
Discovering, Ready, Disconnecting, Backoff`, avec deadline explicite posée à l'entrée de
`Connecting`/`Discovering`, corrigeant l'absence de timeout côté device dans Bluedroid/ESPHome
aujourd'hui (voir HARDWARE_VALIDATION.md pour la preuve terrain de ce manque).

## Jalons

- **M0** — Recon & pin : figer le commit ESPHome exact, confirmer la surface publique des
  7 répertoires. Sortie : ce fichier nomme le commit figé (section ci-dessus complétée).
- **M1** — Bring-up NimBLE : `nimble_ble` + surcharge `esp32_ble` compilent, advertise.
- **M2** — Parité scan.
- **M3** — Parité client GATT + découverte bornée (test de non-régression : verrou hors
  portée/éteint → sortie bornée, pas de hang).
- **M4** — `bluetooth_proxy` bout-en-bout + validation matérielle réelle (voir
  HARDWARE_VALIDATION.md).
- **M5** — Rôle serveur GATT.
- **M6** — Spec formelle + propriétés TLC vertes en CI.
- **M7** — Hardening (backoff, pairing policy, adv queue — voir SECURITY.md).
- **M8** — CI, docs, première release taguée.
