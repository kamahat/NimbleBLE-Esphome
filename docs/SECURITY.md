# Modèle de sécurité

## Invariant architectural non négociable : aucun listener réseau propre

`bluetooth_proxy` (core ESPHome) n'ouvre aucun socket lui-même : il n'implémente que des
handlers invoqués par le dispatcher généré de `APIConnection` (`api_pb2_service.cpp`). Ce
dispatcher applique `check_authenticated_()` à **tout** message sauf `Hello`/`Disconnect`/
`Ping` — ce qui inclut nos futurs messages Bluetooth-proxy. Tant que ce dépôt s'intègre en
tant que handlers de `bluetooth_proxy`/`APIConnection` (et n'ouvre jamais lui-même un
`Socket`/`AsyncServer`/`listen()`/`bind()`), il hérite **gratuitement** de l'authentification
et du chiffrement Noise d'ESPHome (`api: encryption: key:`, `api_frame_helper_noise.cpp`).

**Ce point est vérifié en CI**, pas seulement documenté : `tools/ci/check_no_listener.sh`
grep le code nouvellement ajouté pour toute création de listener réseau et fait échouer le
build si trouvée. C'est directement la leçon tirée de l'audit du firmware `fl4p/
nimble-ble-proxy-esphome` (qui ouvre son propre listener TCP sans aucune authentification —
voir historique du projet) : un composant qui ouvre son propre canal de contrôle perd
automatiquement toute protection Noise, quel que soit le soin apporté par ailleurs.

## OTA — hors périmètre, recommandation de déploiement

Le composant OTA d'ESPHome (`ota: platform: esphome`) est un listener TCP indépendant, non
authentifié par défaut (mot de passe optionnel, non chiffré au niveau transport). C'est un
risque pré-existant dans ESPHome core, pas quelque chose que ce projet peut corriger depuis
un composant BLE. **Recommandation à tout utilisateur de ce dépôt : toujours configurer
`ota: password:`.**

## Pairing BLE — pas de passkey codé en dur, jamais d'auto-accept

Faiblesse identifiée dans le firmware `fl4p/nimble-ble-proxy-esphome` : passkey de pairing
codé en dur (`123456`), auto-accepté pour tout périphérique demandant un pairing
KEYBOARD_ONLY, sans allowlist ni consentement.

**M7 (implémenté) : la classe de vulnérabilité elle-même est rendue structurellement
impossible**, pas seulement rejetée par une policy après coup.
`NimbleController::setup()` (`components/nimble_ble/nimble_controller.cpp`) déclare
`ble_hs_cfg.sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT` : ce firmware n'a ni écran ni clavier pour
un pairing par passkey, donc NimBLE ne peut ni en générer un à afficher, ni en accepter un
saisi par l'utilisateur — il n'existe tout simplement aucun chemin de code où une valeur de
passkey serait générée, comparée ou acceptée. `sm_bonding = 0` et `sm_mitm = 0` : aucune
persistance de bond pour un pairing non sollicité, et aucune exigence d'authentification
forcée qu'on ne pourrait de toute façon pas satisfaire sans capacité d'E/S.

**Périmètre volontairement pas encore couvert, documenté plutôt que simulé** : une vraie
allowlist par MAC/UUID de service qui accepterait/rejetterait explicitement un pairing
Just Works selon la politique (au lieu de simplement ne jamais présenter la vulnérabilité
passkey) demande un handler d'événements GAP de sécurité dédié (`BLE_GAP_EVENT_REPEAT_PAIRING`
et voisins) — non écrit, faute d'un pair matériel qui initie réellement un pairing pour le
vérifier cette session (ni la Boks, ni les bancs de test M1-M6 ne le font). Écrire ce handler
sans le vérifier sur du matériel réel violerait la rigueur que ce projet s'impose par
ailleurs (voir HARDWARE_VALIDATION.md) : mieux vaut documenter le manque que livrer un
handler de sécurité non testé. La propriété formelle `NoUnauthorizedPairing`
(`spec/ble_state_machine.tla`) reste aujourd'hui un garde-fou de régression sur la table de
transitions elle-même (aucune ligne ne peut faire passer `drop_unsolicited_pairing` vers
`Ready`), pas une preuve que l'allowlist est appliquée en pratique — ce sera vrai le jour où
ce handler existera et déclenchera réellement cet événement.

## Rate-limiting des reconnexions

Faiblesse identifiée chez fl4p : aucun backoff sur les tentatives de connexion répétées,
permettant à un périphérique défaillant d'affamer le scan/la radio partagée. **M7
(implémenté)** : `components/nimble_ble/connect_backoff.h` impose un backoff exponentiel
(base 5000ms, plafond 60s, jitter ±20%) utilisé par `esp32_ble_client::BLEClientBase`'s
boucle de reconnexion automatique — la seule boucle de ce dépôt qui retente une connexion de
façon autonome (`bluetooth_proxy` ne le fait jamais : chaque connexion est déclenchée
explicitement par Home Assistant via l'API, donc rate-limitée par le client lui-même, pas par
ce firmware). Clé par adresse pour de bon : une instance `BLEClientBase`/`NimbleGattEngine`
correspond déjà à exactement une adresse configurée, donc le compteur d'échecs déjà tenu par
`BleConnectionFsm::backoff_count()` sert directement sans structure de tracking séparée.

## Visibilité des pertes (pas de silence)

Faiblesse identifiée chez fl4p : troncature silencieuse des réponses GATT volumineuses, drops
d'advertisements sous charge sans compteur exposé.

**Réponses GATT (lecture/notify) : vérifié, pas de troncature.** Lecture directe de
`bluetooth_connection_hub.cpp::on_read_result()`/`on_notify_data()` : chaque réponse porte sa
longueur réelle (`uint16_t len`, jusqu'à la MTU négociée) dans un `std::vector<uint8_t>`, pas
de buffer de taille fixe. Aucune troncature possible pour une valeur de caractéristique/
descripteur individuelle (bornée nativement par l'ATT MTU, largement sous la taille d'un
message protobuf). La seule pagination légitimement nécessaire — la liste des services d'un
périphérique, potentiellement nombreux — est déjà gérée par l'envoi incrémental
service-par-service de `bluetooth_gatt_send_services()` (hérité du core). Pas de fichier
`gatt_response_chunking.cpp/.h` dédié : il n'y a rien à corriger ici dans cette
réimplémentation clean-room, contrairement à fl4p.

**Advertisements sous charge : M7 (implémenté), et un vrai bug de thread-safety trouvé au
passage.** En relisant `esp32_ble_tracker::handle_gap_event_()` pour ajouter le compteur de
drops demandé, `BLE_GAP_EVENT_DISC`/`_COMPLETE` appelaient directement
`ScanResponseMerger`/`AdvDispatcher` (donc les `ESPBTDeviceListener` enregistrés — pour
`bluetooth_proxy`, l'API/socket ESPHome) **depuis la tâche hôte NimBLE elle-même**, en
violation du principe de marshaling déjà établi partout ailleurs dans ce projet (M3/M5 :
`nimble_event.h`/`nimble_server_event.h`). `components/esp32_ble_tracker/adv_queue.h/.cpp`
ferme cette faille avec le même patron (file FreeRTOS bornée à 64, drain depuis `loop()`) et
expose `get_adv_queue_dropped_count()` (voir le README du composant pour le snippet
`sensor: platform: template`).
