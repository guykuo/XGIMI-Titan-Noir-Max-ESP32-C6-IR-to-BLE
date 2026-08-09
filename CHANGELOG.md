# Changelog

All notable changes to this project are documented in this file.

## [2.10.0] - 2026-08-04

### Changed

- Make the Bluetooth HID and wake-advertisement identity configurable with
  `ble_remote_name`, defaulting to `M5Stack Atom Lite` so the Atom can coexist
  with the original remote. Choosing `XGIMI RC` exposes the projector's
  Shortcut settings but can replace or clash with an original remote pairing.

## [2.9.0] - 2026-08-04

### Added

- Initial shareable firmware for cloning the XGIMI Titan Noir Max Bluetooth
  remote on an M5Stack Atom Lite.
