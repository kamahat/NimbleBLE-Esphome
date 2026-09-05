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

**v0.1.0** — M1 à M7 terminés et vérifiés (compilation + matériel réel quand pertinent) :
rôles client et serveur GATT, `bluetooth_proxy` bout-en-bout avec Home Assistant, state
machine de connexion/découverte vérifiée formellement (TLA+/TLC), durcissement sécurité
(backoff exponentiel, classe de vulnérabilité pairing fl4p fermée, thread-safety des
advertisements). Détail complet, y compris les limitations connues de portée v1, dans
`docs/ARCHITECTURE.md` et `docs/OVERRIDE_CAVEATS.md`. Reste en dehors du périmètre v1 :
allowlist de pairing complète (voir `docs/SECURITY.md`), pairing/passkey côté client BLE.

## Démarrage rapide

```yaml
external_components:
  - source: github://kamahat/NimbleBLE-Esphome
    components: [esp32_ble, nimble_ble, ble_device_base, esp32_ble_tracker, bluetooth_proxy]

esp32_ble_tracker:

bluetooth_proxy:
  active: true

api:
  encryption:
    key: !secret api_encryption_key

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password
```

Rôle serveur GATT (`esp32_ble_server`) : voir `tests/components/esp32_ble_server/` pour un
exemple complet (service/caractéristique/descripteur, `on_connect`/`on_write`/notify).

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
