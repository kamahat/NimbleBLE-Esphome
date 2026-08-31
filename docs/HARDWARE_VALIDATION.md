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

_Aucun essai NimBLE encore réalisé — section à remplir en M4._
