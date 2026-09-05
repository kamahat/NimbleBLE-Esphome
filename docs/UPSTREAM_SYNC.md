# Procédure de re-synchronisation contre un nouveau tag ESPHome

À exécuter avant toute mise à jour du commit ESPHome ciblé (voir `ARCHITECTURE.md`), et
périodiquement (recommandé : à chaque release mineure ESPHome) même sans besoin fonctionnel
immédiat — pour éviter l'accumulation silencieuse de divergence documentée dans
`OVERRIDE_CAVEATS.md`.

1. **Identifier le nouveau commit/tag cible** dans `esphome/esphome`.
2. **Pour chacun des 7 répertoires surchargés**, récupérer le contenu du répertoire
   `esphome/components/<name>/` sur ce nouveau commit et le differ contre la dernière version
   snapshotée (celle citée dans `ARCHITECTURE.md`) — pas contre notre override (qui a
   volontairement divergé), contre l'ancien commit core.
3. **Classer chaque changement détecté** :
   - Changement d'implémentation interne sans impact sur l'interface publique (namespace,
     classes exposées, schéma de config) → rien à faire côté override, sauf ré-vérifier
     qu'aucun de nos fichiers ne dépendait d'un détail interne changé.
   - Changement d'interface publique (signature de méthode, nouveau champ de schéma, nouvelle
     dépendance croisée entre composants) → porter le changement dans le fichier correspondant
     de ce dépôt.
   - Nouveau fichier ajouté au répertoire core → décider : le porter, ou le documenter comme
     fonctionnalité sciemment non reprise dans `OVERRIDE_CAVEATS.md`.
4. **Recompiler la matrice de tests** (`tests/components/*/test.esp32-idf.yaml`) contre le
   nouveau commit avant de merger la mise à jour.
5. **Mettre à jour `ARCHITECTURE.md`** avec le nouveau commit figé et la date de
   re-synchronisation.

## M8 addition -- le tag Docker de la CI aussi

`.github/workflows/compile-matrix.yml` épingle `ESPHOME_IMAGE_TAG` (image Docker
`esphome/esphome`) sur la même version que le commit ESPHome ciblé documenté ici. Une
resynchronisation qui oublie de le mettre à jour fait tourner la CI contre une version
différente de celle réellement utilisée/vérifiée matériellement -- ajouter cette étape à la
liste ci-dessus à chaque resynchronisation.
