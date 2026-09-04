# Protocole de test d'acceptation matériel

## Périphérique de référence : verrou BLE "Boks"

Ce projet a été motivé par un échec reproductible et déjà documenté de la découverte GATT
Bluedroid sur ce périphérique précis — c'est notre test d'acceptation réel, pas un
benchmark synthétique.

**Ne jamais coder le MAC en dur** — le récupérer depuis `secrets.yaml` (convention déjà en
place dans `E:\Dev\esphome-boks-spike` sur le poste local, `boks_mac: "CD:05:E3:65:D6:7F"`
dans les faits consignés le 2026-07-17, à re-confirmer avec l'utilisateur car une MAC BLE
peut être aléatoire/tournante selon le périphérique).

Services/characteristics standard utilisables pour un test de lecture read-only (sans toucher
au service custom Boks `a7630001-…`, hors périmètre de ce projet) :
- Battery Service `0x180F` / Battery Level `0x2A19`
- Device Information `0x180A` / Firmware Revision `0x2A26`, Software Revision `0x2A28`

## Baseline Bluedroid déjà mesurée (à ne PAS re-découvrir, à améliorer)

Constaté sur `esphome-boks-spike` (ESPHome 2026.7.0, ESP32-S3 "Heemol" N16R8, antenne u.FL
externe, RSSI −82 dBm, dongle officiel AtomS3U débranché — sinon la Boks n'advertise plus) :

- Connexion GATT : réussie, latence ~0,7–1,0 s. **La radio et l'accroche BLE ne sont pas le
  problème.**
- Découverte de services : **échoue systématiquement**. Log : *"Remote closed during
  discovery"*, `reason=0x13` (remote terminated), **exactement à 30,000 s** à chaque cycle
  (~7 cycles observés sur 10 min). Le scanner BLE est confirmé en pause pendant la connexion
  (pas d'interférence de scan) ; le lien radio tient les 30 s pleines sans supervision
  timeout — le facteur limitant est la pile, pas le matériel.
- Comparaison confirmée avec deux centrals natifs sur le même périphérique : dongle officiel
  AtomS3U (NimBLE) → fiable ; RPi4 (BlueZ/noble) → fiable, 6/6 lectures, à −93 dBm (RSSI
  *pire* que le test Bluedroid à −82 dBm, ce qui élimine l'hypothèse radio/RSSI).

**Conclusion déjà établie (2026-07-19)** : la Boks éjecte les centraux "inactifs" après 30 s
(watchdog documenté côté protocole Boks), et Bluedroid ne boucle jamais la découverte GATT
dans cette fenêtre sur ce périphérique — probable corruption ACL/L2CAP en coexistence
WiFi/BLE que Bluedroid gère mal et que NimBLE/BlueZ gèrent bien.

## Cible du test d'acceptation M4

Reproduire le même protocole que `esphome-boks-spike` (carte Heemol ESP32-S3 N16R8, mêmes
réglages coexistence WiFi/BLE — interval 320 ms / window 30 ms ≈ 9 % duty cycle), mais avec
la pile NimBLE de ce dépôt à la place de Bluedroid :

1. **≥ 10 essais** de connexion + découverte de services, RSSI comparable (viser un
   emplacement où le RSSI Boks est ≥ −85 dBm, cf. grille de décision de l'esphome-boks-spike).
2. Relever pour chaque essai : latence de connexion, latence de découverte, succès/échec.
3. **Cible** : découverte < 30 s de façon fiable sur la totalité des essais — idéalement
   proche du ~6 s déjà observé avec le pattern `blecent` du dongle officiel (référence :
   dépôt `skob`, doc 09).
4. **Essai déliberé "périphérique hors portée/éteint"** : confirmer que le `discover_timeout`
   de la state machine (voir `spec/transitions.json`) produit une sortie bornée et propre
   (transition vers `Backoff`) plutôt qu'un hang — c'est la propriété formelle `BoundedWait`
   vérifiée côté spec, à confirmer ici côté matériel réel.
5. Documenter les résultats (timing, taux de succès) dans ce fichier, avec date et commit
   testé.

## Environnement de développement (build + flash = poste local, pas le bastion)

Le build ESP-IDF/PlatformIO et le premier flash série d'un ESP32 se font sur le **poste
local** (E:\Dev), pas sur claude-mgmt (pas d'accès USB) — convention déjà établie pour ce
type de projet. Le dépôt git, la doc, la spec et les outils de génération vivent ici sur le
bastion ; le poste local ne fait que cloner ce dépôt pour la phase build+flash le moment venu,
puis bascule vers OTA/observation via le bastion une fois le firmware flashé une première
fois.

- ESP-IDF déjà installé localement : `E:\Dev\esp-idf` (installeur `idf-env`, contraintes
  `espidf.constraints.v5.5.txt`).
- Carte de test précédente : ESP32-S3 "Heemol" N16R8 — **PSRAM non fonctionnelle** (mode
  `octal` → bootloop RTCWDT, mode `quad` → "PSRAM chip is not connected" ; PSRAM désactivée
  dans la config sans impact, RAM interne suffisante). Flash 16 Mo Macronix, quad, conforme.
  Port de travail : **COM12** (pont CH343, auto-reset esptool fonctionnel) — le port USB
  natif S3 (COM10/COM11) ne supporte pas l'auto-reset avec le firmware d'usine.
- Config ESPHome de référence (Bluedroid, pour comparaison side-by-side) :
  `E:\Dev\esphome-boks-spike\boks-spike-heemol-s3.yaml` sur le poste local.

## Résultats (à compléter au fil des essais)

### M1 — bring-up NimBLE confirmé sur matériel réel (2026-09-01)

Carte utilisée : **XIAO ESP32-C5** (déjà caractérisée dans
`project-boks-esp32c5-antenna` — antenne stock, pas besoin de l'antenne
externe pour ce test d'advertising simple), MAC `38:44:be:ba:09:12` (BLE) /
`38:44:be:ff:fe:ba:09:10` (base), port natif USB-Serial/JTAG (pas de puce
CH34x — reconnu directement par Windows, VID `303A`/PID `1001`).

Flashé avec `tests/components/esp32_ble/test.esp32c5-idf.yaml`
(`esphome run ... --device COM<n>`, ESPHome 2026.8.2, ESP-IDF 5.5.5, cible
`esp32c5`). Logs série capturés (voir gotchas ci-dessous) :

```
[C][component:164]: Setup esp32_ble took 636ms
[I][app:117]: setup() finished successfully!
[D][nimble_ble:038][nimble_host]: NimBLE host synced
[C][esp32_ble.nimble:066]: BLE (NimBLE):
  MAC address: 38:44:BE:BA:09:12
  Active: YES
  Advertising: YES
```

**Confirmé** : le contrôleur/host NimBLE s'initialise, se synchronise, et
l'advertising démarre — sur un vrai ESP32-C5, pas seulement en compilation.
Reste à confirmer visuellement via un scanner BLE téléphone que "Nimble M1
Test C5" est bien visible en l'air (pas fait depuis cette session — aucun
accès à un scanner BLE externe depuis l'environnement d'exécution).

**Gotchas rencontrés (à ne pas re-découvrir)** :
- `esphome logs` n'a produit **aucune sortie** pendant 15s à chaque tentative
  (deux essais), malgré un flash et un firmware fonctionnels confirmés
  ensuite. Contournement : lecture série brute (`System.IO.Ports.SerialPort`
  en PowerShell) avec un toggle DTR/RTS explicite avant lecture a immédiatement
  produit les logs complets dès le boot ROM. Cause précise non élucidée
  (probablement un problème de timing/relance du port natif USB-Serial/JTOG
  après le "Hard resetting via RTS pin" d'esptool) — mais le contournement est
  fiable, à réutiliser si `esphome logs` reste silencieux sur ce type de carte.
- **`logger: hardware_uart: USB_SERIAL_JTAG` est obligatoire** sur cette carte
  — sans cette option explicite, ESPHome ne route pas la console sur le port
  USB natif pour le profil de board générique `esp32-c5-devkitc-1` (UART0
  physique par défaut, non connecté sur ce module).
- L'avertissement esptool *"Crystal frequency mismatch... configured for
  0MHz"* persiste même avec `CONFIG_XTAL_FREQ_48: 'y'` dans
  `sdkconfig_options` (le sdkconfig généré garde `CONFIG_XTAL_FREQ_AUTO=y`
  quoi qu'il arrive) — **sans conséquence pratique constatée** : les logs et
  le comportement runtime sont corrects avec AUTO. Ne pas perdre de temps à
  re-creuser cet avertissement à l'avenir sauf symptôme réel.
- Build+flash faits en local (`E:/Dev/NimbleBLE-Esphome-flash`, clone
  temporaire jetable) via un venv Python local dédié (`python -m venv`,
  Python 3.13 système) — **jamais via Git Bash** pour les commandes
  `esphome compile`/`run`/`upload`/`logs` : ESP-IDF refuse explicitement
  l'environnement MSYS/Mingw (`MSys/Mingw is not supported`). Utiliser
  PowerShell natif pour toute invocation esphome/ESP-IDF sur ce poste.


## M4 — bug réel trouvé et corrigé : connect() bloqué en silence pendant un scan (2026-09-03)

Test ad hoc (non commité, `tmp/m4_real_connect/`, gitignoré) : cibler dynamiquement
le premier appareil BLE réel assez fort (RSSI seuil variable) vu par
`esp32_ble_tracker` et tenter un vrai `connect()`/`discover_services()` dessus via
`bluetooth_connection::NimbleGattClient`, sur le même XIAO ESP32-C5.

**Symptôme observé** : avec `esp32_ble_tracker:` (scan continu actif) et
`bluetooth_connection:` dans le même firmware, `connect()` était appelé (log
"Targeting..." absent par endroits à cause de la congestion du port série sous
VERY_VERBOSE, voir plus bas) mais **plus aucun événement ne suivait jamais** --
ni `on_connection_state`, ni le moindre signe côté NimBLE. Diagnostic par ajout
de logs directement dans `parse_device()` (confirmé appelé, RSSI correct) et
dans le lambda lui-même (confirmé appelé avec RSSI valides, y compris > seuil)
a éliminé un bug de câblage trigger/automation -- la fonction s'exécutait bien,
`connect()` était bien appelé, mais rien ne se produisait après.

**Cause racine** (confirmée en ajoutant un `stop_scan()` manuel avant le
`connect()` de test, qui a immédiatement débloqué un vrai résultat
asynchrone) : NimBLE refuse categoriquement `ble_gap_connect()` **pendant
qu'une procédure de découverte (scan) est en cours** -- documenté
(`BLE_HS_EBUSY if initiating a connection is not possible because scanning is
in progress`) mais jamais géré par `NimbleGattEngine::connect()`, qui appelait
`ble_gap_connect()` sans jamais vérifier ni gérer l'état du scan. Sans ce fix,
**tout chemin GATT client (esp32_ble_client ET bluetooth_connection/
bluetooth_proxy) échoue en silence dès qu'un scan continu tourne en même
temps** -- exactement le cas d'usage central de `bluetooth_proxy` (scanner en
continu ET accepter des connexions).

**Fix** (`components/nimble_ble/nimble_gattc.cpp`, `NimbleGattEngine::connect()`) :
appel de `ble_gap_disc_cancel()` avant `ble_gap_connect()`, inconditionnel et
sans dépendance vers le tracker. Ce choix évite tout couplage direct engine <->
tracker : annuler un scan en cours génère le même `BLE_GAP_EVENT_DISC_COMPLETE`
qu'une fin de scan normale, qu'`esp32_ble_tracker` traite déjà correctement
(`dispatcher_.on_scan_end()`, `scanner_state_` -> `IDLE`) ; son propre `loop()`
relance ensuite le scan tout seul au prochain tick si `scan_continuous_` est
actif (mécanisme M2 réutilisé tel quel, aucune modification côté tracker
nécessaire). Confirmé après fix : plusieurs cycles connect() réels contre des
appareils BLE effectifs à proximité produisent maintenant une réponse HCI
asynchrone authentique (`error=13`/timeout borné correct, ou d'autres codes
HCI selon l'appareil ciblé -- ex. `error=531`, un code d'échec HCI réel plutôt
que le silence total observé avant le fix).

**Reste non prouvé dans cette passe** : un connect+discover **réussi** contre
un pair réellement connectable (les appareils environnants identifiés --
iBeacons, balises Microsoft Swift Pair -- sont typiquement non-connectables
par conception). Le chemin d'échec borné est prouvé de bout en bout contre du
matériel réel ; le chemin de succès reste à confirmer avec un vrai pair GATT
connectable à proximité (ou directement le Boks, hors périmètre de cette
passe -- voir décision utilisateur du 2026-09-03).

**Gotcha série reconfirmé** (déjà documenté M1, reproduit plusieurs fois ici) :
la lecture série brute post-reset reste intermittente sur ce port
USB-Serial/JTAG natif -- certaines relectures immédiates après flash ne
produisent rien du tout (le firmware tourne, mais aucune sortie série
n'apparaît), sans lien avec le code testé. Un `esptool.py ... chip_id` (reset
propre, sans reflash) juste avant la lecture rétablit systématiquement le flux
dans cette session ; à défaut, réessayer une seconde fois suffit généralement.
Ne pas confondre ce symptôme avec un vrai crash/watchdog (aucun indicateur de
reset trouvé dans les captures concernées : pas de bannière de boot répétée,
pas de "Guru Meditation Error").


## M4 — acceptance test réel contre la Boks (2026-09-04)

Test dédié (`tmp/m4_boks_test/`, non commité, gitignoré) sur l'ESP32-S3 précédemment
utilisé pour `project-boks-nimble-bridge` (MAC `AC:27:6E:A4:EA:38`), rebranché en USB
localement le temps du test (la carte tournait auparavant le pont fl4p, remplacé pour ce
test avec confirmation explicite de l'utilisateur -- ce pont proxifiait réellement la Boks
dans Home Assistant). Cible : adresse réelle de la Boks (`CD:05:E3:65:D6:7F`, adresse
statique random -- bits de poids fort `11`), connexion directe par adresse (pas de scan
préalable), 10 tentatives connect()+discover_services() consécutives via
`bluetooth_connection::NimbleGattClient`, chronométrées.

**Gotcha rencontré** : aucune sortie série sur le port CH343 (COM12) avec la config
logger par défaut -- contrairement à l'ESP32-C5 (qui a besoin de
`hardware_uart: USB_SERIAL_JTAG`), cette carte S3 a besoin de l'inverse,
**`hardware_uart: UART0` explicite**, sinon la console part ailleurs (JTAG natif de la
puce) et le pont CH343 ne voit rigoureusement rien (0 octet reçu, vérifié en lecture
brute avant de comprendre la cause). Gotcha symétrique à celui du C5, à retenir pour
toute carte S3/C3/C6 avec pont UART physique séparé du contrôleur natif.

**Résultat** : 10/10 tentatives bornées, aucune n'a hang, timing quasi identique à
chaque fois --

| Tentative | Résultat | error | Durée |
|---|---|---|---|
| 1 | timeout | 13 (`BLE_HS_ETIMEOUT`) | 5008ms |
| 2 | timeout | 13 | 5014ms |
| 3 | timeout | 13 | 5017ms |
| 4 | timeout | 13 | 5004ms |
| 5 | timeout | 13 | 5006ms |
| 6 | timeout | 13 | 5008ms |
| 7 | timeout | 13 | 5011ms |
| 8 | timeout | 13 | 5013ms |
| 9 | timeout | 13 | 5015ms |
| 10 | timeout | 13 | 5017ms |

Chaque tentative respecte la deadline `Connecting` de 5000ms à ±17ms près (jitter de
planification/logging) -- **aucune dérive sur 10 essais consécutifs**. C'est la comparaison
directe recherchée face au Bluedroid stock : là où `esp_ble_gattc_search_service()` n'a
aucune deadline côté device (le ~30s documenté dans `project-boks-esp32-spike` est le
timeout du CLIENT HA, pas une protection du firmware), notre FSM borne côté device, de
façon déterministe, à chaque tentative -- un gain qualitatif (borné vs pas borné du tout),
pas seulement quantitatif (5s vs 30s).

**Non prouvé dans ce test** : aucune des 10 tentatives n'a atteint `connected=1` -- la
Boks n'a pas répondu une seule fois. Cause la plus probable : la carte a été rebranchée en
USB au poste de travail pour ce test, donc plus à portée BLE réelle de la Boks (zone
`exterieur_sud` dans HA, donc à l'extérieur -- la carte est maintenant à l'intérieur, sur
le bureau). Le chemin d'échec borné est donc prouvé de bout en bout contre la cible réelle
(pas un stand-in) ; le chemin de succès (connect+discover réel contre la Boks) nécessite de
rapprocher physiquement la carte de la boîte aux lettres, pas encore fait.
