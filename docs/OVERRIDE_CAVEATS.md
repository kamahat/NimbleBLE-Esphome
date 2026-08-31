# Avertissement — ce dépôt REMPLACE des répertoires ESPHome core, il ne les patche pas

## Le mécanisme

Quand un utilisateur référence ce dépôt via :

```yaml
external_components:
  - source: github://kamahat/NimbleBLE-Esphome@main
    components: [esp32_ble, ble_device_base, esp32_ble_tracker, esp32_ble_client,
                 esp32_ble_server, esp32_ble_beacon, bluetooth_proxy]
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

_Aucune entrée pour l'instant — ce fichier doit être mis à jour à chaque milestone qui
touche un répertoire surchargé._
