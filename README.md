# NimbleBLE-Esphome

Suite de composants **ESPHome `external_component`** qui remplace la pile BLE native
d'ESPHome (Bluedroid, imposée par `esp32_ble`/`ble_device_base`/`esp32_ble_tracker`/
`esp32_ble_client`/`esp32_ble_server`/`esp32_ble_beacon`/`bluetooth_connection`/
`bluetooth_proxy`) par une
implémentation basée sur **NimBLE** (API ESP-IDF NimBLE brutes), tout en préservant les
mêmes classes/namespaces/schémas de configuration — donc un remplacement quasi transparent
pour un utilisateur ESPHome existant.

## Pourquoi

ESPHome ne propose aujourd'hui aucune option pour utiliser NimBLE dans son proxy Bluetooth
(`bluetooth_proxy`) ou son GATT client (`esp32_ble_client`) — c'est Bluedroid, sans option
de configuration, et les deux piles sont mutuellement exclusives au niveau du `sdkconfig`
ESP-IDF. Sur certains périphériques BLE, la découverte de services Bluedroid ne se termine
jamais dans la fenêtre attendue, alors que NimBLE la termine de façon fiable (constaté à de
multiples reprises sur un ESP32-S3 dialoguant avec un verrou BLE, voir
[`docs/HARDWARE_VALIDATION.md`](docs/HARDWARE_VALIDATION.md)). Ce n'est pas un problème
d'antenne/RSSI : le lien radio est bon, seule la découverte GATT Bluedroid échoue.

## ⚠️ Ceci n'est pas un patch — c'est un remplacement complet de répertoire

ESPHome permet de surcharger un composant natif en publiant un `external_component` de même
nom (mécanisme documenté officiellement : *"Bundled components can be overridden using this
feature"*). Mais l'override remplace **tout le répertoire**, pas fichier par fichier. **Lire
[`docs/OVERRIDE_CAVEATS.md`](docs/OVERRIDE_CAVEATS.md) avant toute modification** : si
ESPHome upstream change l'interface publique d'un des composants surchargés, ce dépôt diverge
silencieusement jusqu'à re-synchronisation (procédure : [`docs/UPSTREAM_SYNC.md`](docs/UPSTREAM_SYNC.md)).

## État du projet

M0 (recon & pin de la version ESPHome ciblée) en cours. Pas encore utilisable en production —
voir les jalons dans `docs/ARCHITECTURE.md`.

## Documentation

| Document | Contenu |
|---|---|
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Graphe des composants surchargés, contrat GATT, commit ESPHome ciblé, jalons |
| [OVERRIDE_CAVEATS.md](docs/OVERRIDE_CAVEATS.md) | Ce qui casse si upstream change une interface |
| [UPSTREAM_SYNC.md](docs/UPSTREAM_SYNC.md) | Procédure de re-diff contre un nouveau tag ESPHome |
| [SECURITY.md](docs/SECURITY.md) | Modèle de pairing/allowlist, invariant "pas de listener parallèle" |
| [HARDWARE_VALIDATION.md](docs/HARDWARE_VALIDATION.md) | Protocole de test d'acceptation matériel |

## Licence

Voir [LICENSE](LICENSE).
