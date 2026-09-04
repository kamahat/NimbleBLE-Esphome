// ble_2902.h -- SURCHARGE NimbleBLE-Esphome, identique au core dans l'esprit
// (une BLEDescriptor à UUID fixe 0x2902) mais son rôle réel change : NimBLE
// gère la CCCD automatiquement pour toute caractéristique notify/indicate
// (voir ble_descriptor.h) -- un BLE2902 explicitement déclaré par
// l'utilisateur est détecté et exclu de la table native construite par
// BLECharacteristic::do_create(), documenté dans docs/OVERRIDE_CAVEATS.md.
#pragma once

#include "ble_descriptor.h"

#ifdef USE_ESP32

namespace esphome::esp32_ble_server {

class BLE2902 : public BLEDescriptor {
 public:
  BLE2902();
};

}  // namespace esphome::esp32_ble_server

#endif
