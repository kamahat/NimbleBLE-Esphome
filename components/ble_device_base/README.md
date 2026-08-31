# components/ble_device_base — SURCHARGE minimale de `esphome/components/ble_device_base`

**Statut : compile vérifié, M1 (2026-08-31).** Contrairement à la conception
initiale (voir `docs/ARCHITECTURE.md` § Découverte majeure), ce répertoire
**doit** être surchargé — mais de façon quasi triviale : tous les fichiers
sont vendorés à l'identique du core (même logique, même comportement), sauf
`ble_device.h` où une seule ligne change : `#include <esp_bt_defs.h>`
devient `#include "esp_bt_defs_compat.h"`.

**Pourquoi** : `ble_device.h` inclut ce header Bluedroid sans condition sous
`USE_ESP32` (pour 2 méthodes historiques jamais utilisées hors esp32). Le
vrai `esp_bt_defs.h` d'ESP-IDF vit sur le chemin d'include Bluedroid
(`bt/host/bluedroid/api/include/api/`), invisible dès que
`CONFIG_BT_BLUEDROID_ENABLED=n`. `esp_bt_defs_compat.h` (même répertoire,
quoted include) fournit seulement les deux types réellement utilisés.

**Piège vérifié** : ce composant doit être explicitement listé dans le
`components:` du bloc `external_components:` de tout YAML utilisant
`esp32_ble`/`nimble_ble` — sinon ESPHome retombe silencieusement sur la
version core non patchée (voir `docs/OVERRIDE_CAVEATS.md`).

À chaque `docs/UPSTREAM_SYNC.md`, revérifier que ce seul écart (une ligne
d'include) suffit toujours — si upstream change la structure de
`ble_device.h`/`.cpp`, le diff pourrait s'élargir.
