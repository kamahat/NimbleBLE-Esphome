# Avertissement — ce dépôt REMPLACE des répertoires ESPHome core, il ne les patche pas

## Le mécanisme

Quand un utilisateur référence ce dépôt via :

```yaml
external_components:
  - source: github://kamahat/NimbleBLE-Esphome@main
    components: [esp32_ble, ble_device_base, esp32_ble_tracker, esp32_ble_client,
                 esp32_ble_server, esp32_ble_beacon, bluetooth_connection, bluetooth_proxy]
```

ESPHome charge **la totalité** de chaque répertoire `components/<name>/` de ce dépôt à la
place du répertoire `esphome/components/<name>/` correspondant dans le core ESPHome installé.
Ce n'est **pas un merge, pas un patch, pas un diff appliqué** — c'est un remplacement complet
de tous les fichiers `.py`/`.cpp`/`.h` de ce répertoire.

Conséquence directe : **tout fichier qui existe dans le répertoire core mais pas dans le
répertoire surchargé de ce dépôt disparaît purement et simplement** pour l'utilisateur final.
Si upstream ajoute un fichier à l'un de ces 7 répertoires (nouvelle automation, nouveau
sensor, nouvelle option de config) sans que ce dépôt ne le reprenne, cette fonctionnalité est
silencieusement absente — aucune erreur de compilation, aucun avertissement.

## Ce qui casse silencieusement si upstream évolue sans qu'on s'en aperçoive

- Un utilisateur ESPHome qui utilise `bluetooth_proxy: active: true` avec une option ajoutée
  par une version ESPHome plus récente que celle sur laquelle ce dépôt est basé : l'option
  sera acceptée par le schéma Python (si on ne l'a pas encore, `cv.Schema` lèvera une erreur
  de config claire — c'est le cas "heureux") ou pire, silencieusement ignorée si le nom
  existe mais le comportement C++ correspondant n'a pas été porté.
- Un changement de signature dans `BLEGattConnectionContract` (`ble_gatt_client.h`) côté
  upstream : rien ne nous prévient tant qu'on ne recompile pas contre la nouvelle version
  d'ESPHome — la compilation échouera (bon signe, détectable), mais seulement au moment où
  un utilisateur tente de monter de version ESPHome sans que ce dépôt n'ait suivi.
- Toute nouvelle dépendance interne ajoutée par upstream entre deux de ces 7 répertoires
  (ex. un huitième composant qui vient s'insérer dans la chaîne) : invisible tant qu'on ne
  re-diffe pas activement contre un nouveau tag upstream (voir UPSTREAM_SYNC.md).

## Règle pratique

**Ne jamais supposer que "ça compile" veut dire "on a toute la fonctionnalité upstream".**
Avant de committer un override de l'un de ces répertoires, comparer explicitement (fichier
par fichier, pas juste "les gros noms de classes") avec le répertoire core correspondant sur
le commit ESPHome ciblé (voir ARCHITECTURE.md pour le commit figé), et documenter dans ce
fichier toute fonctionnalité upstream sciemment non reprise (avec la raison).

## Fonctionnalités upstream sciemment non reprises (à tenir à jour)

### `esp32_ble_client` / `ble_client` (M3, 2026-09-03)

- **Pairing/passkey** : `on_passkey_request`, `on_passkey_notification`,
  `on_numeric_comparison_request`, `ble_client.passkey_reply`,
  `ble_client.numeric_comparison_reply`, `ble_client.remove_bond` -- aucun n'est repris.
  `BLEClientBase`/`nimble_ble::NimbleGattEngine` exposent `pair()`
  (`ble_gap_security_initiate`) et `on_pairing_result` existe déjà côté
  `GattClientListener`, mais rien ne les relie encore à une automation YAML.
- **`ble_client.ble_write` déclaratif** : pas repris. Équivalent disponible via lambda :
  `id(mon_client).engine().write_characteristic(handle, data, len, response)`, le handle
  se résolvant via `id(mon_client).get_characteristic(service_uuid, char_uuid)`.
- **Type d'adresse appris par scan** : le core Bluedroid enregistre le client comme
  `ESPBTClient` auprès du tracker pour observer l'advertisement du pair avant de se
  connecter (et en déduire PUBLIC vs RANDOM). Notre port NimBLE compose directement
  `ble_gap_connect()` par adresse sans scan préalable (écart architectural déjà assumé --
  voir ARCHITECTURE.md M2/M3), et suppose donc **toujours BLE_ADDR_TYPE_PUBLIC**. Un pair
  utilisant une adresse RANDOM/RPA ne sera jamais joignable en l'état.
- **Aucune plateforme sensor/switch/text_sensor consommant `BLEClientNode`** n'est portée
  (ex. `ble_sensor`, `ble_binary_sensor`, `ble_client.switch`, etc. du core) -- hors
  périmètre décidé pour ce projet (voir ARCHITECTURE.md, l'objectif est le correctif du
  hang Bluedroid, pas l'écosystème de capteurs BLE complet). `register_ble_node()` existe
  et suit l'interface neutre `BLEClientNode` (`on_ble_client_connected`/
  `on_ble_client_disconnected`/`loop()`) pour qu'une future plateforme puisse s'y attacher,
  mais celle-ci diffère du `gattc_event_handler` Bluedroid du core -- un futur portage d'une
  plateforme core devra être réécrit contre cette interface, pas simplement recompilé.
- **`ble_client.connect`/`ble_client.disconnect` ne sont pas "complex" actions** : contrairement
  au core (`play_complex_`/`num_running_`, qui suspend la chaîne d'automation jusqu'à la fin
  réelle de la connexion/déconnexion), nos actions sont fire-and-forget -- `play()` lance
  l'opération et la chaîne continue immédiatement. `on_connect`/`on_disconnect` restent le
  moyen de réagir au résultat réel.
- ~~Backoff de reconnexion fixe (5000ms), pas exponentiel~~ -- **fait en M7** :
  `components/nimble_ble/connect_backoff.h`, vérifié sur matériel réel (voir
  HARDWARE_VALIDATION.md).

### `esp32_ble_server` (M5, 2026-09-04)

- **Pas de service Device Information auto-généré** (`manufacturer_name_string:`,
  `firmware_version:`, `model:`, etc. du core) -- un utilisateur qui en a besoin le déclare
  lui-même comme un service `0x180A` normal avec ses caractéristiques standard.
- **Pas de sugar CUD/CCCD** : le core génère automatiquement un descripteur Characteristic
  User Description et gère certaines options CCCD de haut niveau. Ici, un CUD (`0x2901`) se
  déclare explicitement comme n'importe quel descripteur ; la CCCD (`0x2902`) reste
  auto-gérée par NimBLE lui-même pour toute caractéristique notify/indicate (pas par ce
  composant) -- un `0x2902` déclaré par l'utilisateur est accepté (parité API) mais exclu de
  la table réellement enregistrée.
- **Pas de valeurs templatées (lambda) pour `value:`** : valeur statique uniquement (liste
  d'octets ou chaîne). Les mises à jour dynamiques passent par les actions
  `ble_server.characteristic.set_value`/`.notify`/`ble_server.descriptor.set_value`
  (elles-mêmes byte-list-only, pas de `std::string` -- voir `set_buffer()` du core vendored
  tel quel).
- **Pas d'options d'encodage avancées** (string_encoding/endianness) ni d'appearance BLE.

### Pairing BLE (M7, 2026-09-05)

- **Pas d'allowlist par MAC/service avec accept/reject explicite** : voir SECURITY.md pour
  ce qui EST fait (la classe de vulnérabilité fl4p -- passkey codé en dur auto-accepté --
  est rendue structurellement impossible via `sm_io_cap = BLE_HS_IO_NO_INPUT_OUTPUT`) versus
  ce qui reste ouvert (un vrai handler `BLE_GAP_EVENT_REPEAT_PAIRING` avec allowlist, non
  écrit faute d'un pair matériel pour le vérifier).


## Piège vérifié en pratique (M1, 2026-08-31)

Un composant absent de la liste `components:` d'un `external_components:` **ne surcharge rien** -- ESPHome retombe silencieusement sur la version core, sans erreur ni avertissement. `ble_device_base` a été oublié de cette liste lors du premier essai de compile M1 : le patch (voir ARCHITECTURE.md) était bien écrit sur disque mais jamais utilisé, et l'erreur observée (`esp_bt_defs.h: No such file or directory`) était identique à avant le patch -- rien dans le message d'erreur ne signale ce piège, il faut vérifier la liste `components:` en premier reflexe face à un override qui semble ignoré.