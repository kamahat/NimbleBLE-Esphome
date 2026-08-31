# components/bluetooth_proxy — SURCHARGE de `esphome/components/bluetooth_proxy`

**Statut : squelette, M4 — jalon d'intégration HA.** Doit préserver exactement la classe et
le singleton `global_bluetooth_proxy` attendus par le dispatcher `APIConnection` (voir
`docs/SECURITY.md` — c'est ce qui permet d'hériter gratuitement de l'authentification Noise).

Ajouts propres à ce dépôt (corrections des faiblesses identifiées lors de l'audit du firmware
`fl4p/nimble-ble-proxy-esphome`, voir `docs/SECURITY.md`) :
- `gatt_response_chunking.cpp/.h` — pagination `MAX_PACKET_SIZE`, pas de troncature silencieuse.
- `connect_backoff.cpp/.h` — backoff exponentiel par adresse.
- `pairing_policy.cpp/.h` — allowlist + passkey par device, pas d'auto-accept global.
- `adv_queue.cpp/.h` — file bornée + capteur de diagnostic HA pour le compteur de drops.

**Invariant vérifié en CI** (`tools/ci/check_no_listener.sh`) : aucun fichier de ce répertoire
ne doit ouvrir de listener réseau indépendant.
