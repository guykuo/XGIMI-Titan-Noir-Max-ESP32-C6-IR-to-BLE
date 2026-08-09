# Changelog

All notable changes to this project are documented in this file.

## [2.20.0] - 2026-08-09

### Added

- Add a selectable exact-counter wake action with a configurable advertisement
  duration and a privacy-preserving desktop counter observer.

## [2.18.0] - 2026-08-04

### Added

- Add a stateful `Power` switch for the desired state and a read-only `Power`
  binary sensor backed by the projector's BLE HID connection.
- Expose the persisted last wake counter and per-attempt sweep diagnostics.

### Changed

- Replace the rapid 256-value wake burst with sequential counter advertisements
  held for 1500 ms and separated by a genuine 500 ms off-air gap.
- Persist the last advertised counter and continue from the next value until the
  projector connects.
- Use the configured Bluetooth identity for both HID and wake advertisements.

### Fixed

- Complete an already-transmitted transition before reconciling a newer desired
  state, so rapid `ON -> OFF -> ON` and `OFF -> ON -> OFF` changes converge.
- Repeat the confirmed 1500 ms held shutdown command after 500 ms release gaps
  until the projector disconnects.
- Wait 15 seconds after an observed shutdown before beginning a queued wake,
  preventing wake advertisements during the projector's shutdown transition.
- Adopt uncommanded projector changes from the original remote or HDMI-CEC as
  the new desired state.

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
