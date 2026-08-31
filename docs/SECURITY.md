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

## Pairing BLE — allowlist, jamais d'auto-accept global

Faiblesse identifiée dans le firmware `fl4p/nimble-ble-proxy-esphome` : passkey de pairing
codé en dur (`123456`), auto-accepté pour tout périphérique demandant un pairing
KEYBOARD_ONLY, sans allowlist ni consentement. **Ce dépôt ne reproduit pas ce design** :
`components/bluetooth_proxy/pairing_policy.cpp/.h` implémente une allowlist explicite par
MAC/UUID de service, avec passkey par device (pas de secret global), et rejette tout pairing
non sollicité par défaut. Vérifié par la propriété formelle `NoUnauthorizedPairing`
(`state' = Ready ⟹ address ∈ Allowlist`) dans `spec/ble_state_machine.cfg`.

## Rate-limiting des reconnexions

Faiblesse identifiée chez fl4p : aucun backoff sur les tentatives de connexion répétées,
permettant à un périphérique défaillant ou un client API malveillant d'affamer le scan/la
radio partagée. `components/bluetooth_proxy/connect_backoff.cpp/.h` impose un backoff
exponentiel par adresse, indépendant du comportement du client (HA).

## Visibilité des pertes (pas de silence)

Faiblesse identifiée chez fl4p : troncature silencieuse des réponses GATT volumineuses,
drops d'advertisements sous charge sans compteur exposé. `components/bluetooth_proxy/
gatt_response_chunking.cpp/.h` pagine correctement (pas de troncature), et `adv_queue.cpp/.h`
expose un capteur de diagnostic HA pour le compteur de drops.
