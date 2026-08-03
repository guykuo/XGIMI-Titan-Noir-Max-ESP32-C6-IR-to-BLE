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

## Wake advertisement

The BLE manufacturer company ID is `0x0046`. Bleak exposes a 16-byte value for
that company ID: one rolling counter byte followed by a stable 15-byte token.
The token is specific to the user's original remote and belongs only in
`secrets.yaml`. Power On broadcasts the complete counter range `0x00`–`0xFF`
with that stable tail.
