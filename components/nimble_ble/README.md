# components/nimble_ble

**Statut : squelette, M1.** Pas une surcharge d'un composant ESPHome existant — bibliothèque
interne partagée par tous les répertoires surchargés.

Contenu prévu (voir `docs/ARCHITECTURE.md`) :
- `nimble_controller.cpp/.h` — `nimble_port_init()`/`nimble_port_freertos_init()`, assertions
  sdkconfig (échec explicite si `CONFIG_BT_BLUEDROID_ENABLED` est actif ailleurs).
- `nimble_gap.cpp/.h` — traduction des callbacks GAP NimBLE (`ble_gap_event_fn`) en
  événements C++ typés.
- `nimble_gattc.cpp/.h` — wrappers des opérations GATT client (`ble_gattc_*`).
- `nimble_gatts.cpp/.h` — wrappers du rôle serveur GATT (`ble_gatts_*`).
- `nimble_advertising.cpp/.h` — construction advertising/scan-response.
- `nimble_uuid.h` — adaptateur UUID 128/32/16-bit ↔ `ESPBTUUID` (ESPHome core).
- `nimble_fsm/` — moteur de state machine **généré** depuis `spec/transitions.json`
  (`tools/gen_state_machine/gen_cpp.py`) — ne jamais éditer `ble_connection_fsm.h/.cpp`
  à la main.
