# Architecture

## Commit ESPHome ciblé (M0 — TERMINÉ)

`esphome/esphome` commit `813c0006842681e1408d27e017897abd74a87b69` (branche par défaut,
snapshot pris le 2026-08-31). Les 8 répertoires pertinents ont été lus directement à ce
commit (listing de fichiers + interfaces publiques clés) — le détail ci-dessous, pas une
supposition.

## M1 -- premier compile réel réussi (2026-08-31)

`esphome compile` (ESPHome 2026.8.2, ESP-IDF 5.5.5, cible esp32s3) **réussit**
contre `tests/components/esp32_ble/test.esp32-idf.yaml` : `nimble_ble` (bring-up
contrôleur/host NimBLE) + la surcharge `esp32_ble` (event queue, advertising)
compilent et lient, binaire produit (`firmware.factory.bin`, Flash 25%, RAM 25%).
Test réalisé sur claude-mgmt (venv Python 3.12 dédié via `uv`, voir
`docs/UPSTREAM_SYNC.md` -- ESPHome ≥2026.7.0 exige Python ≥3.12, le système est
en 3.11).

**Correction supplémentaire découverte pendant ce premier compile** :
`ble_device_base` n'est PAS totalement platform-neutre comme documenté plus bas
le suggérait -- `ble_device.h` inclut `<esp_bt_defs.h>` sans condition sous
`USE_ESP32` (pour les méthodes historiques `ESPBTUUID::from_uuid/get_uuid` et
`ESPBTDevice::get_address_type`). Ce header vit sur le chemin d'include
Bluedroid d'ESP-IDF (`bt/host/bluedroid/api/include/api/`), invisible dès que
`CONFIG_BT_BLUEDROID_ENABLED=n` (confirmé par le diagnostic ninja lui-même).
`cg.add_build_flag("-I...")` ne résout PAS ce genre de problème pour les builds
ESP-IDF : confirmé en lisant `esphome/framework_helpers.py::
get_project_compile_flags()`, qui ne retient que les flags `-D`/`-W`, jamais
`-I` -- un flag `-I` passé par un `external_component` est silencieusement
ignoré sur ce framework (fonctionnerait sur Arduino/PlatformIO via CPPPATH,
pas ici).

**Fix retenu** : `ble_device_base` EST surchargé après tout, mais de façon
strictement minimale -- `components/ble_device_base/` vendore le core à
l'identique, sauf `ble_device.h` où `#include <esp_bt_defs.h>` devient
`#include "esp_bt_defs_compat.h"` (quoted, même répertoire -- résolu par
recherche same-directory, sans mécanisme d'include global). Le shim
(`esp_bt_defs_compat.h`) ne définit que les 2 types réellement utilisés
(`esp_bt_uuid_t`, `esp_ble_addr_type_t`). **Point d'attention pour
`docs/UPSTREAM_SYNC.md`** : toute resynchronisation contre un nouveau commit
ESPHome doit re-vérifier que ce seul écart (une ligne d'include) reste
suffisant.

**Piège de configuration à ne pas répéter** : un composant override doit être
listé explicitement dans `external_components: components: [...]` du YAML --
sinon ESPHome retombe silencieusement sur la version core (aucune erreur,
juste le comportement Bluedroid non patché). `ble_device_base` a dû être
ajouté à cette liste en plus de `esp32_ble`/`nimble_ble`.

## Découverte majeure (correction de la conception initiale)

Le premier passage de conception (recherche via agents) avait supposé 7 répertoires à
surcharger, dont `ble_device_base`, en pensant que c'était là que vivait le contrat GATT
concret. **Une lecture directe du code a révélé deux erreurs, corrigées ici :**

1. **`ble_device_base` est déjà platform-neutre et n'a PAS besoin d'être surchargé.**
   `ble_gatt_client.h` y définit `BLEGattConnectionContract` comme un **concept C++20**
   (pas une classe virtuelle) : `template<typename T> concept BLEGattConnectionContract =
   requires(T conn, ...) { conn.connect(...); conn.discover_services(); ... }`. N'importe
   quel backend (Bluedroid, NimBLE, BTstack sur RP2040) satisfait ce concept sans toucher
   à `ble_device_base`. On implémente juste un nouveau backend qui satisfait le concept.

2. **Un 8e répertoire, absent de la conception initiale, est celui qui compte vraiment
   pour le remplacement de backend : `esphome/components/bluetooth_connection/`.**
   Découvert par recherche de code (`bluetooth_connection_gatt_backend.h` n'apparaissait
   dans aucun des 7 répertoires initialement listés). Contenu confirmé :
   - `bluetooth_connection_gatt_backend.h` — le point de liaison exact :
     ```cpp
     #if defined(USE_RP2040_BLE)
       #include "bluetooth_connection_rp2.h"
       #define ESPHOME_BLE_GATT_CONNECTION_TYPE bluetooth_connection::RP2GattClient
     #elif defined(USE_ESP32_BLE)
       #include "bluetooth_connection_bluedroid.h"
       #define ESPHOME_BLE_GATT_CONNECTION_TYPE bluetooth_connection::BluedroidGattClient
     #else
       #error "USE_BLE_GATT_CLIENT is set but this build has no GATT backend"
     #endif
     namespace esphome::ble_device_base {
       using BLEGattConnection = ESPHOME_BLE_GATT_CONNECTION_TYPE;
       static_assert(BLEGattConnectionContract<BLEGattConnection>, "...");
     }
     ```
     **C'est exactement le point d'insertion pour NimBLE** : ajouter un troisième bras
     (`bluetooth_connection::NimbleGattClient`) et forcer sa sélection via notre propre
     define de sdkconfig (pas besoin de dépendre de `USE_ESP32_BLE`/`USE_RP2040_BLE` du
     core — on peut définir notre propre macro dans `esp32_ble/__init__.py`).
   - `bluetooth_connection_bluedroid.cpp/.h` (33 Ko) — le backend Bluedroid actuel
     (`BluedroidGattClient`), à ne PAS reprendre (clean-room) mais à égaler
     fonctionnellement.
   - `bluetooth_connection_hub.cpp/.h` (25 Ko) — **dans le namespace `esphome::
     bluetooth_proxy`** malgré son emplacement dans ce répertoire : c'est le hub qui
     consomme le `GattClientListener`, gère le chunking `GATTGetServicesResponse`
     (confirme le fait #7 de la conception initiale — MAX_PACKET_SIZE existe bien, juste
     pas dans `bluetooth_proxy.cpp` comme supposé, mais ici).
   - `bluetooth_connection.cpp/.h` — classe de base commune (wrapper `BluetoothConnection`).
   - `bluetooth_connection_rp2.cpp/.h` — backend BTstack pour RP2040 (référence de style
     pour un second backend dans ce même répertoire — utile comme modèle structurel).
   - Une "arme" de test existe déjà dans `bluetooth_connection_gatt_backend.h` pour les
     tests host (`StubGattBackend`, activée par `USE_BLE_GATT_CLIENT_STUB_BACKEND`) —
     son jeu de méthodes est un gabarit exact de ce que `NimbleGattClient` doit
     implémenter : `set_listener`, `connect(uint64_t, uint8_t)`, `gatt_disconnect()`,
     `cancel_gatt_disconnect()`, `discover_services()`, `read_characteristic(uint16_t)`,
     `write_characteristic(uint16_t, const uint8_t*, uint16_t, bool)`,
     `read_descriptor(uint16_t)`, `write_descriptor(uint16_t, const uint8_t*, uint16_t)`,
     `notify_characteristic(uint16_t, bool)`, `pair()`,
     `update_connection_params(uint16_t, uint16_t, uint16_t, uint16_t)`,
     `get_service_table()`, `release_services()`, `set_connection_type(ConnectionType)`.

## Répertoires à surcharger (override = remplacement intégral) — liste finale

1. **`esp32_ble`** — init contrôleur/host, advertising. Confirmé : la classe
   `ESP32BLE final : public Component` a sa file d'événements (`BLEEvent`,
   `LockFreeQueue`) **typée directement sur les structures Bluedroid**
   (`esp_gap_ble_cb_event_t`, `esp_ble_gap_cb_param_t`, `esp_gattc_cb_event_t`,
   `esp_ble_gattc_cb_param_t`, `esp_gatts_cb_event_t`) — ce n'est pas juste une question
   de sdkconfig, la surface C++ elle-même doit être réécrite pour NimBLE (types
   d'événements différents). Singleton `extern ESP32BLE *global_ble`.
2. **`esp32_ble_tracker`** — scan. `ESP32BLETracker final : public Component, public
   Parented<ESP32BLE>` appelle directement les API Bluedroid de scan GAP
   (`gap_scan_result_`, `gap_scan_set_param_complete_`, etc.). Réexporte pour
   compat descendante des types désormais définis dans `ble_device_base`
   (`ClientState`, `ConnectionType`, `ScannerState`, `ESPBTDevice` — ceux-là **restent
   inchangés**, seule la mécanique Bluedroid doit être remplacée). Contient déjà une
   state machine de **timeout de scan** (`ScanTimeoutState`, `scan_timeout_ms_ =
   scan_duration_ * 2000`) — à distinguer du **timeout de découverte GATT qui, lui,
   n'existe nulle part** (absence confirmée, voir HARDWARE_VALIDATION.md).
3. **`esp32_ble_client`** — chemin legacy `ble_client:`/`BLEClientNode`, **distinct du
   chemin `bluetooth_proxy`** (décision utilisateur : gardé dans le périmètre v1 malgré
   ce découplage). `BLEClientBase : public espbt::ESPBTClient, public Component` —
   state machine de connexion Bluedroid-native complète (30 Ko), avec déjà un
   **timeout de sécurité de 10s sur DISCONNECTING** (`set_disconnecting_()`,
   commentaire : *"the DISCONNECTING timeout check in loop() would never run if
   CLOSE_EVT gets lost"*) — donc un precedent existant de "timeout de sécurité"
   dans ESPHome core, à réutiliser comme modèle pour notre `discover_timeout` FSM.
   C'est le chemin qu'utilisait l'ancien composant custom `boks_spike` (projet
   `esphome-boks-spike`, sur le poste local) via `BLEClientNode`.
4. **`esp32_ble_server`** — rôle GATT serveur/périphérique. `BLEServer final : public
   Component, public Parented<ESP32BLE>` — dépend uniquement de `esp32_ble` (pas de
   `bluetooth_connection`/`ble_device_base`), portage relativement autonome.
5. **`esp32_ble_beacon`** — `ESP32BLEBeacon final : public Component` — dépend
   uniquement de `esp32_ble` (advertising), le plus simple des 7.
6. **`bluetooth_connection`** (le répertoire découvert, voir ci-dessus) — c'est ici
   que vit le vrai point d'insertion NimBLE pour le chemin `bluetooth_proxy`/HA :
   `bluetooth_connection_gatt_backend.h` (binding), notre futur
   `bluetooth_connection_nimble.h/.cpp` (`NimbleGattClient`, satisfaisant
   `BLEGattConnectionContract`), et `bluetooth_connection_hub.*` (chunking/hub,
   dans le namespace `bluetooth_proxy`).
7. **`bluetooth_proxy`** — glue de haut niveau (`__init__.py`, `bluetooth_proxy.cpp/h`
   seulement) ; aucun code réseau propre — hérite de l'auth Noise via `APIConnection`
   (voir SECURITY.md).

**`ble_device_base` EST surchargé**, mais de façon minimale (un seul include patché,
voir section M1 ci-dessus) -- pas une réécriture, un vendoring quasi à l'identique.

## Couche NimBLE partagée (nouveau, pas une surcharge)

`components/nimble_ble/` — bring-up contrôleur/host NimBLE (API ESP-IDF NimBLE brutes :
`nimble_port.h`, `host/ble_gap.h`, `host/ble_gattc.h`, `host/ble_gatts.h`), traduction des
callbacks C NimBLE en événements C++ typés, adaptateur UUID (`ESPBTUUID` ↔ NimBLE),
moteur de state machine généré (`nimble_fsm/`, voir plus bas). `NimbleGattClient`
(satisfaisant `BLEGattConnectionContract`) sera implémenté dans
`components/bluetooth_connection/bluetooth_connection_nimble.cpp/.h` et s'appuiera sur
cette couche partagée.

## State machine de connexion/découverte

Voir `spec/transitions.json` (source unique de vérité) — états `Idle, Scanning, Connecting,
Discovering, Ready, Disconnecting, Backoff`, avec deadline explicite posée à l'entrée de
`Connecting`/`Discovering`, corrigeant l'absence de timeout côté device dans Bluedroid/ESPHome
aujourd'hui (voir HARDWARE_VALIDATION.md pour la preuve terrain de ce manque). Le précédent
du timeout de sécurité 10s de `esp32_ble_client::BLEClientBase::set_disconnecting_()`
(point 3 ci-dessus) confirme que ce type de garde-fou est un pattern déjà accepté dans
ESPHome core, pas une invention de ce projet.

## Jalons

- **M0** — Recon & pin : **TERMINÉ 2026-08-31.** Commit figé, 8 répertoires lus
  directement, contrat `BLEGattConnectionContract` et point d'insertion exacts confirmés.
- **M1** — Bring-up NimBLE : **compile vérifié 2026-08-31** (`nimble_ble` +
  surcharge `esp32_ble` + patch minimal `ble_device_base`). Restant M1 : flash
  réel sur ESP32-S3 (poste local, port COM12/CH343) + confirmation visuelle
  d'advertising via un scanner BLE téléphone -- pas encore fait depuis le bastion
  (pas d'USB).
- **M2** — Parité scan : **compile vérifié 2026-09-01** (`esp32_ble_tracker` +
  triggers `on_ble_advertise`/`on_ble_service_data_advertise`/
  `on_ble_manufacturer_data_advertise`/`on_scan_end`). Écart architectural assumé :
  contrairement à Bluedroid (un seul callback GAP global), NimBLE attend un
  callback par opération (`ble_gap_disc`, `ble_gap_connect`, `ble_gap_adv_start`) --
  le tracker possède donc directement sa session `ble_gap_disc()` au lieu de
  s'enregistrer sur un dispatch central `esp32_ble` (les helpers Python
  `esp32_ble.register_gap_event_handler`/etc. du core n'existent pas dans notre
  surcharge -- décision assumée, pas un oubli). NimBLE livrant advertisement et
  scan response comme deux événements séparés (contrairement à Bluedroid qui les
  fusionne déjà), le tracker réutilise `ble_device_base::ScanResponseMerger`/
  `AdvDispatcher` (déjà vendorés, prévus exactement pour ce cas). Reste M2 : flash
  matériel + vérification qu'un vrai appareil BLE proche est détecté (pas encore fait).
- **M3** — Parité GATT bout-en-bout : `bluetooth_connection` (chemin `bluetooth_proxy`/HA,
  jalon prioritaire) ET `esp32_ble_client` (chemin legacy `ble_client:`/`BLEClientNode`,
  dans le périmètre v1 par décision utilisateur) ; découverte bornée dans les deux.
- **M4** — `bluetooth_proxy` bout-en-bout + validation matérielle réelle (voir
  HARDWARE_VALIDATION.md).
- **M5** — Rôle serveur GATT (`esp32_ble_server`).
- **M6** — Spec formelle + propriétés TLC vertes en CI.
- **M7** — Hardening (backoff, pairing policy, adv queue — voir SECURITY.md).
- **M8** — CI, docs, première release taguée.
