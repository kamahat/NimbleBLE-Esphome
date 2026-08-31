"""Shared NimBLE controller/host bring-up, used internally by esp32_ble's override.

Not user-facing: no CONFIG_SCHEMA, always pulled in via esp32_ble's AUTO_LOAD.
"""
import esphome.codegen as cg

CODEOWNERS = ["@kamahat"]
DEPENDENCIES = ["esp32"]

nimble_ble_ns = cg.esphome_ns.namespace("nimble_ble")
