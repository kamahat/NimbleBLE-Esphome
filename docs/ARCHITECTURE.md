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
moteur de state machine généré (voir plus bas). `NimbleGattClient`
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
- **M2** — Parité scan : **TERMINÉ 2026-09-03** (`esp32_ble_tracker` +
  triggers `on_ble_advertise`/`on_ble_service_data_advertise`/
  `on_ble_manufacturer_data_advertise`/`on_scan_end`, validé matériel sur ESP32-C5).
  Écart architectural assumé : contrairement à Bluedroid (un seul callback GAP
  global), NimBLE attend un callback par opération (`ble_gap_disc`, `ble_gap_connect`,
  `ble_gap_adv_start`) -- le tracker possède donc directement sa session
  `ble_gap_disc()` au lieu de s'enregistrer sur un dispatch central `esp32_ble`
  (les helpers Python `esp32_ble.register_gap_event_handler`/etc. du core
  n'existent pas dans notre surcharge -- décision assumée, pas un oubli). NimBLE
  livrant advertisement et scan response comme deux événements séparés
  (contrairement à Bluedroid qui les fusionne déjà), le tracker réutilise
  `ble_device_base::ScanResponseMerger`/`AdvDispatcher` (déjà vendorés, prévus
  exactement pour ce cas).

  **Bug M2 trouvé + corrigé sur matériel réel (2026-09-03)** : premier flash sur
  ESP32-C5, scan/timing corrects (`Scan complete` à l'heure) mais **zéro
  advertisement capturé** malgré 10+ sources BLE réelles à moins de 10m -- signal
  fort que ce n'était pas l'antenne (contra `project-boks-esp32c5-antenna`, qui
  documentait déjà une carte+antenne faibles, mais pas zéro-sur-dix-sources-proches).
  Cause racine (lecture directe du code, pas supposée) : `ble_device_base`
  n'active son stockage de listeners (`StaticVector` dans `scan_response_merger.h`,
  boucle de dispatch dans `AdvDispatcher::dispatch()`) que si `to_code()` d'un
  consommateur appelle le `cg.slot_counter(LISTENER_COUNT_DEFINE)` requis
  ("no requests, no define: the guarded storage ... compile out entirely",
  docstring de `cpp_helpers.slot_counter`). `esp32_ble_tracker/__init__.py`
  construisait ses 4 triggers à la main sans jamais appeler ce compteur --
  `ESPHOME_BLE_DEVICE_BASE_LISTENER_COUNT` n'était donc **jamais défini**, et tout
  le bloc d'enregistrement/dispatch des listeners compilait à vide : chaque
  advertisement réelle atteignait bien `dispatch()`, mais y était silencieusement
  jetée, indépendamment de l'antenne. Fix (commit `af4b40b`) : ajout de
  `ble_device_base.request_listener_slot()` (même patron que `request_gatt_client()`
  juste au-dessus dans le même fichier) appelé une fois par trigger construit dans
  `esp32_ble_tracker::to_code()`. Vérifié sur la même carte après recompilation
  bastion + reflash local : **19 appareils BLE distincts capturés** sur une seule
  fenêtre de scan de 30s (contre 0 avant le fix).
- **M3** — Parité GATT bout-en-bout : **EN COURS, chemin prioritaire TERMINÉ 2026-09-03.**
  Moteur partagé `components/nimble_ble/` : `nimble_event.h/.cpp` (file d'événements
  thread-safe -- callbacks GAP/GATT NimBLE marshalés depuis la tâche host NimBLE vers la
  boucle principale ESPHome ; risque déjà signalé mais différé en M1, M2 s'en sortait sans
  car le scan est read-only, un client GATT stateful non), `nimble_uuid.h`, `nimble_gattc.h/.cpp`
  (`NimbleGattEngine` : connect/discover/read/write sur `host/ble_gap.h`+`host/ble_gatt.h` bruts,
  découverte séquentielle profondeur-d'abord car NimBLE n'autorise qu'une procédure GATT à la
  fois par connexion, enveloppe `BleConnectionFsm` pour la deadline bornée Connecting/Discovering).
  `ble_connection_fsm.h/.cpp` déplacé hors d'un sous-répertoire `nimble_fsm/` : la copie de fichiers
  des `external_components` d'ESPHome est plate (non récursive) -- découvert par le premier compile
  M3 qui échouait, le fichier imbriqué étant simplement absent de l'arbre de build.
  `components/bluetooth_connection/` (nouveau, ESP32/NimBLE uniquement -- ni RP2 ni Bluedroid ne
  sont dans le périmètre de ce projet) : `bluetooth_connection_nimble.h` (`NimbleGattClient`,
  enveloppe `Component` fine autour de `NimbleGattEngine`, satisfait `BLEGattConnectionContract`
  confirmé par la compilation propre de son `static_assert`), `bluetooth_connection_gatt_backend.h`
  (surcharge liant `USE_ESP32_BLE_NIMBLE` -- nouveau define distinct de `USE_ESP32_BLE` que le core
  émet pour n'importe quelle pile, ajouté à `esp32_ble/__init__.py`), `__init__.py` minimal
  (clé `bluetooth_connection:` autonome pour test compile/matériel ; `bluetooth_connection_hub.h/.cpp`,
  la façade côté `bluetooth_proxy`, reste hors périmètre M3, sera fait en M4).

  **Validation matérielle (même XIAO ESP32-C5)** : connexion vers une adresse volontairement
  injoignable -- la deadline Connecting (5000ms) déclenche sa propre sortie bornée
  (`on_connection_state(connected=false, error=BLE_HS_ETIMEOUT)`) au lieu d'un hang, le correctif
  direct du fait que `esp_ble_gattc_search_service()` n'a aujourd'hui aucune deadline propre
  (`docs/HARDWARE_VALIDATION.md`). Un vrai bug trouvé et corrigé dans ce test : l'événement CONNECT
  asynchrone de la tentative annulée arrive plus tard via la file et rapportait le même résultat
  une seconde fois -- gardé par `connect_result_reported_`, revérifié propre (un seul appel
  `on_connection_state`) après correctif.

  **Chemin legacy `esp32_ble_client`/`ble_client` : TERMINÉ 2026-09-03, validé matériel.**
  `esp32_ble_client::BLEClientBase` (connexion/découverte/état, sur le même
  `nimble_ble::NimbleGattEngine`) + `ble_client::BLEClient` (surface YAML `ble_client:`,
  triggers `on_connect`/`on_disconnect` via une interface `BLEClientNode` neutre --
  `on_ble_client_connected`/`on_ble_client_disconnected`, pas de
  `gattc_event_handler`/`gap_event_handler` façon Bluedroid). Ajout à l'engine partagé :
  `retry_connect()` (fait passer Backoff -> Idle via l'événement `BACKOFF_ELAPSED` avant de
  relancer `connect()` -- Backoff n'accepte que cet événement, `connect()` seul y serait un
  no-op silencieux) et `state()`. Boucle de reconnexion à délai fixe (5000ms) dans
  `BLEClientBase::loop()` -- backoff exponentiel FAIT en M7 (`connect_backoff.h`)
  (voir OVERRIDE_CAVEATS.md pour le reste du périmètre sciemment non repris : pairing/passkey,
  `ble_client.ble_write`, plateformes sensor/switch/text_sensor).

  **Validation matérielle (même XIAO ESP32-C5, `ble_client: mac_address: <adresse injoignable>`)** :
  3 cycles complets observés dans une seule capture -- timeout borné (`error=13`), trigger
  `on_disconnect` déclenché exactement une fois par cycle (aucune régression du double-appel
  corrigé plus haut), puis nouvelle tentative après le délai de backoff fixe. Bout en bout,
  chemin legacy compris.
- **M4** — `bluetooth_proxy` bout-en-bout + validation matérielle réelle (voir
  HARDWARE_VALIDATION.md).
- **M5** — TERMINÉ. Rôle serveur GATT (`esp32_ble_server`) sur table statique NimBLE
  (`ble_gatts_count_cfg` + `ble_gatts_add_svcs` + `ble_gatts_start`, un seul enregistrement
  au lieu de la création asynchrone service-par-service de Bluedroid) : `BLEServer`/
  `BLEService`/`BLECharacteristic`/`BLEDescriptor`/`BLE2902`, file d'événements
  `ServerEventQueue` (même patron thread-marshaling que le client M3), `ble_server_automations`
  repris tel quel du core (confirmé neutre vis-à-vis de Bluedroid). CCCD (0x2902) auto-gérée
  par NimBLE pour toute caractéristique notify/indicate ; un 0x2902 déclaré par l'utilisateur
  est accepté mais exclu de la table réellement enregistrée (parité API, pas de doublon).
  Portée v1 volontairement réduite face au core -- voir OVERRIDE_CAVEATS.md.

  **Validation matérielle (téléphone Android, deux familles de puces -- ESP32-C5 et ESP32
  classique ESP32-D0WD, voir HARDWARE_VALIDATION.md)** : connexion, découverte du service/
  caractéristique/descripteurs exactement conforme à la config, read, write (`on_write`
  confirmé côté ESP32), notify (valeur poussée après écriture, reçue par le central) --
  tous validés de bout en bout sur les deux cartes. Un vrai bug trouvé via matériel réel
  (payload d'advertising legacy plafonné à 31 octets, jamais rencontré avant M5 car aucun
  rôle précédent n'advertit) corrigé par dégradation progressive dans
  `nimble_controller.cpp::start_advertising()`.
- **M6** — TERMINÉ. Modèle formel `spec/ble_state_machine.tla` (TLA+ direct, généré par
  `tools/gen_state_machine/gen_tla.py` depuis le même `spec/transitions.json` que le moteur
  C++ -- pas de PlusCal intermédiaire, les deux générateurs partagent une seule source de
  vérité). Deadlines représentées par des constantes symboliques (`Deadline_CONNECT_REQUEST`,
  `Deadline_GAP_CONNECT_OK`) fixées à de petites valeurs abstraites (2/3 ticks) dans
  `ble_state_machine.cfg` pour garder l'espace d'états tractable -- la forme de la garantie
  est invariante d'échelle ; les vraies valeurs (5000ms/8000ms) ne vivent que dans le moteur
  C++ livré, désormais confirmées par les mesures matérielles réelles de M3/M4 (voir
  HARDWARE_VALIDATION.md).

  **TLC vert** : `TypeOK`, `BoundedWait`, `NoUnauthorizedPairing` (invariants) et
  `EventuallyExits` (propriété temporelle sous fairness faible) tous vérifiés sans erreur
  (587 états distincts après avoir borné `backoff_count` -- sans cette borne, l'espace
  d'états explose : un cycle Idle→Connecting→Backoff sans jamais appeler Tick peut faire
  croître `backoff_count` indéfiniment, observé concrètement à 900k+ états toujours en
  croissance avant correctif). `tools/gen_state_machine/check_reachability.py` fait une
  seconde vérification indépendante (recherche de cycle sur le graphe d'états exporté par
  TLC) redondante avec `EventuallyExits`, dans l'esprit prouver, pas croire du projet.

  `tests/unit/test_ble_connection_fsm.cpp` rejoue deux traces réelles extraites de TLC
  (idiome standard : assertion d'un invariant volontairement faux pour obtenir un
  contre-exemple concret) contre le moteur C++ compilé, avec les vraies deadlines en ms --
  confirmant que le modèle vérifié et le code livré produisent bien les mêmes transitions
  sur les mêmes séquences d'événements.

  CI : `.github/workflows/state-machine-check.yml` -- régénère TLA+/C++ depuis le spec et
  échoue si le résultat commité a divergé, lance TLC, lance le check de reachability, compile
  et exécute le test unitaire host-buildable.
- **M7** — Hardening, voir SECURITY.md pour le détail complet.
  - **connect_backoff** (fait) : backoff exponentiel + jitter (`components/nimble_ble/
    connect_backoff.h`), keyed off `BleConnectionFsm::backoff_count()` directement --
    remplace le délai fixe 5000ms de `BLEClientBase::loop()` (seule boucle de reconnexion
    autonome de ce dépôt ; `bluetooth_proxy` ne retente jamais de lui-même, chaque connexion
    vient d'une requête HA explicite).
  - **pairing policy** (partiel, assumé) : `ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT`
    rend la classe de vulnérabilité fl4p (passkey codé en dur auto-accepté) structurellement
    impossible. Une vraie allowlist par MAC/service avec accept/reject explicite reste NON
    écrite -- documenté comme tel plutôt que livré non vérifié, faute d'un pair matériel qui
    initie un pairing pour le tester cette session.
  - **adv_queue** (fait) : vrai bug de thread-safety trouvé en cours de route --
    `handle_gap_event_()` appelait `ScanResponseMerger`/`AdvDispatcher` (donc les
    `ESPBTDeviceListener`, potentiellement l'API ESPHome via `bluetooth_proxy`) directement
    depuis la tâche hôte NimBLE, pas depuis la boucle principale -- seul endroit du projet où
    ce principe de marshaling déjà établi (M3/M5) n'était pas respecté.
    `components/esp32_ble_tracker/adv_queue.h/.cpp` corrige ça avec le même patron de file
    bornée, plus le compteur de drops exposé demandé par SECURITY.md.
  - **gatt_response_chunking** (investigué, pas nécessaire) : `bluetooth_connection_hub.cpp`
    utilise déjà `std::vector<uint8_t>`/longueur réelle partout, aucune troncature possible ;
    la pagination de la liste de services existe déjà nativement (`bluetooth_gatt_send_services()`).
    Rien à corriger dans cette réimplémentation clean-room -- pas de fichier dédié créé.

  Vérifié par compilation sans régression sur les 5 cibles de test concernées (M2 tracker,
  M3 client, M4 proxy, M5 server). Backoff exponentiel vérifié sur matériel réel (arbiter
  de nouveau joignable) : 5 cycles consécutifs sur l'adresse injoignable du test M3,
  délais observés 4500/10900/18400/34800/69000ms -- progression ×2 conforme jusqu'au
  plafond (60000ms), jitter ±20% correct à chaque cycle (voir HARDWARE_VALIDATION.md).
- **M8** — CI, docs, première release taguée.
