# Sanitised Titan Noir Max Bluetooth HID reference

This model-wide reference was captured from an original XGIMI Titan Noir Max
remote with an ESP32 HID host. Per-user identifiers and the wake token are
deliberately omitted.

## Identity and descriptor

- Advertised name: `XGIMI RC`
- Manufacturer string: `Seneasy`
- HID reports: keyboard report ID 1 (8 bytes) and consumer-control report ID 2
  (6 bytes)
- Release: an all-zero report of the same type

HID report descriptor:

```text
05010906A10105070906A101850195087508150025FF190029FF8100C0050C0901A1018502950375101500269C0219002A9C028100C0C0
```

## Captured reports

| HA action      | Type     | Press data                |
| -------------- | -------- | ------------------------- |
| Cursor Up      | Keyboard | `00 00 52 00 00 00 00 00` |
| Cursor Down    | Keyboard | `00 00 51 00 00 00 00 00` |
| Cursor Left    | Keyboard | `00 00 50 00 00 00 00 00` |
| Cursor Right   | Keyboard | `00 00 4F 00 00 00 00 00` |
| Cursor Enter   | Keyboard | `00 00 28 00 00 00 00 00` |
| Home           | Keyboard | `00 00 4A 00 00 00 00 00` |
| Back           | Keyboard | `00 00 29 00 00 00 00 00` |
| Settings Menu  | Keyboard | `00 00 41 00 00 00 00 00` |
| Game Menu      | Keyboard | `00 00 65 00 00 00 00 00` |
| Focus (Auto)   | Keyboard | `00 00 44 00 00 00 00 00` |
| Focus (Manual) | Keyboard | `00 00 3D 00 00 00 00 00` |
| Volume Up      | Keyboard | `00 00 80 00 00 00 00 00` |
| Volume Down    | Keyboard | `00 00 81 00 00 00 00 00` |
| Power          | Keyboard | `00 00 7F 00 00 00 00 00` |
| Mute           | Consumer | `BD 01 00 00 00 00`       |
| Input          | Consumer | `BC 01 00 00 00 00`       |
| Picture        | Consumer | `23 02 00 00 00 00`       |
| Shortcut 1     | Consumer | `1D 02 00 00 00 00`       |
| Shortcut 2     | Consumer | `1F 02 00 00 00 00`       |
| Shortcut 3     | Consumer | `21 02 00 00 00 00`       |
| Shortcut 4     | Consumer | `22 02 00 00 00 00`       |

Settings and Focus classify a hold inside the original remote and emit their
alternate report as a short tap after the threshold; the ordinary code is not
sent first. Power stays active until release and is held by this firmware for
the confirmed 1500 ms duration to skip the shutdown confirmation.

### Original remote tap timing

A timestamped HID-host capture on 2026-08-09 measured natural presses of the
original remote. Each tap produced one key-down report followed by one all-zero
release report; a held arrow produced no repeated reports.

| Button | Natural taps | Observed key-down durations | Mean |
| ------ | ------------ | --------------------------- | ---- |
| Right  | 10           | 90, 100, 90, 100, 90, 90, 110, 110, 90, 90 ms | 96 ms |
| Left   | 10           | 90, 90, 130, 130, 150, 100, 150, 150, 140, 150 ms | 128 ms |
| Up     | 5            | 90, 90, 90, 90, 100 ms | 92 ms |

The combined median is 100 ms and the combined mean is 108 ms. A separate
800 ms Right hold consisted only of the initial key-down and final release.
Ordinary cloned taps therefore preserve 100 ms of active key-down time rather
than sending press and release notifications back-to-back.

The held-power button and Home Assistant's stateful Power switch both request
desired Power off. Once the command is transmitted, the firmware waits 500 ms
after key-up and repeats the 1500 ms hold while BLE-derived actual Power remains
on, even if desired changes back to on. It stops when the HID link disconnects,
waits 15 seconds for shutdown to settle, then reconciles the latest desired
state. A transmitted wake likewise completes before a newer desired-off request
is applied. This makes missed commands self-recovering without overlapping
transitions.

## Wake advertisement

The BLE manufacturer company ID is `0x0046`. Bleak exposes a 16-byte value for
that company ID: one rolling counter byte followed by a stable 15-byte token.
The token is specific to the user's original remote and belongs only in
`secrets.yaml`. Ordinary Power On begins after the persisted last-sent counter,
holds each successive value for a configurable dwell (1500 ms by default),
stops advertising for a genuine off-air gap of at least 500 ms, wraps after
`0xFF`, and continues until the projector establishes its HID connection. A
newer desired-off request is applied after that transition. The off-air
boundary prevents a projector standby scan/rejection state observed when
payloads were changed continuously as mains
returned. This also avoids relying on one rapid burst when shutdown, CEC or
another remote has left the receiver's rolling state out of sync.

A separate exact-counter action first creates an off-air boundary, then holds
one selected payload in the BLE advertisement for a configurable duration
(4000 ms by default) without changing the selected value. The firmware records
the most recently submitted counter and exposes per-attempt value and full-cycle
counts for diagnostics. Passive desktop BLE observation is not packet-complete:
host stacks may omit callbacks even when advertisements were submitted by the
ESP32.
