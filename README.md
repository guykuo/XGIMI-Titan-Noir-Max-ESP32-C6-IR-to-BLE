# XGIMI Titan Noir Max remote on an M5Stack Atom Lite

This project turns an M5Stack Atom Lite into a Wi-Fi-connected Bluetooth remote
for an XGIMI Titan Noir Max projector. It appears in Home Assistant through the
native ESPHome integration and appears to the projector as a Bluetooth HID
remote named **M5Stack Atom Lite**.

The original Titan Noir Max remote has already been captured and mapped. A new
user does **not** need to learn its buttons, HID descriptor, names or Home
Assistant entities. The only per-remote value required is the original remote's
15-byte BLE wake token.

## What it provides

Home Assistant gets ready-to-use buttons for:

- Back and all cursor directions/Enter
- Home, Input, Mute, Picture and volume
- Settings Menu and the captured alternate Game Menu action
- Focus (Auto) and the captured alternate Focus (Manual) action
- Shortcut 1–4
- separate Power On, Power Off and Power Off (Immediately) actions

It also exposes Bluetooth connection/authentication diagnostics. The Atom's
front button sends Power On.

## How it works

When the projector is awake, the Atom maintains a bonded Bluetooth HID
connection and sends the same keyboard or consumer-control reports captured
from the original remote. Settings Menu and Focus use the two distinct
short/alternate reports produced by the physical remote. Immediate power-off
holds the Power report for the confirmed 1500 ms duration.

When the projector is fully asleep, Power On broadcasts all 256 rolling-counter
values with the original remote's stable 15-byte wake token. There is no
deliberate delay between advertisements. A connection-state guard prevents a
wake burst while the projector is already connected, avoiding advancement of
the projector's rolling-code state at the wrong time.

The firmware contains the tested Titan Noir Max HID descriptor and button map.
The token lives in the ignored `secrets.yaml`, not in the shareable source.

## Requirements

- XGIMI Titan Noir Max and its original Bluetooth remote
- M5Stack Atom Lite with a data-capable USB cable
- Windows, macOS or Linux computer with Bluetooth and Python 3.11 or newer
- Wi-Fi reachable by Home Assistant
- Home Assistant with the ESPHome integration

The supplied versions were tested with ESPHome 2026.7.3 and Bleak 2.1.1.

## Manual setup

1. Clone or copy this directory, create a virtual environment, and install the
   pinned tools:

   On macOS or Linux:

   ```sh
   python3 -m venv .venv
   .venv/bin/python -m pip install -r requirements.txt
   ```

   On Windows PowerShell or Command Prompt:

   ```powershell
   py -3 -m venv .venv
   .venv\Scripts\python.exe -m pip install -r requirements.txt
   ```

2. Physically unplug the projector so it cannot reconnect to the original
   remote. Turn Bluetooth on and grant the terminal Bluetooth access if the
   operating system asks. Run one of:

   ```sh
   .venv/bin/python scripts/capture_wake_token.py --duration 30
   ```

   ```powershell
   .venv\Scripts\python.exe scripts\capture_wake_token.py --duration 30
   ```

   Keep the remote close to the computer and press its Power button several
   times during the scan. A trustworthy capture has a stable 15-byte tail and
   at least two distinct first-byte rolling-counter values.

3. Create `secrets.yaml` with the captured token. The helper generates the API
   key and strong OTA/fallback-access-point passwords using Python's secure
   random generator:

   ```sh
   .venv/bin/python scripts/create_secrets.py --token "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88"
   ```

   ```powershell
   .venv\Scripts\python.exe scripts\create_secrets.py --token "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88"
   ```

   Replace the example argument with the 15-byte token printed by the capture
   script. The helper refuses to overwrite an existing `secrets.yaml`.

4. Connect the new Atom Lite by USB, validate, compile and flash:

   ```sh
   .venv/bin/esphome config m5stack-atom-lite-xgimi-remote.yaml
   .venv/bin/esphome run m5stack-atom-lite-xgimi-remote.yaml
   ```

   ```powershell
   .venv\Scripts\esphome.exe config m5stack-atom-lite-xgimi-remote.yaml
   .venv\Scripts\esphome.exe run m5stack-atom-lite-xgimi-remote.yaml
   ```

   Select the Atom's USB serial port when prompted (`COM…` on Windows or
   `/dev/…` on macOS/Linux). If no port appears, install the USB serial driver
   required by the Atom's USB interface and reconnect it.

5. After reboot, join the **M5Stack Atom Lite Setup** Wi-Fi network from a phone
   or laptop, enter the fallback AP password from `secrets.yaml`, then use the
   captive portal to put the Atom on the same network as Home Assistant.

6. Power on the projector and pair **M5Stack Atom Lite** in its Bluetooth remote
   or accessory settings. Keep the original remote paired as well.

7. In Home Assistant, open **Settings → Devices & services**, add the discovered
   **M5Stack Atom Lite** ESPHome device, and supply the API encryption key from
   `secrets.yaml` if requested.

8. Turn the projector off, wait until the Atom's **Projector Remote Connected**
   diagnostic becomes false, then test **Power On**.

Do not commit `secrets.yaml` or a personalised firmware binary: both contain
device credentials, and the binary also embeds the wake token.

## Prompt for Codex, Claude or another coding agent

Copy the prompt below into an agent that can use your terminal and USB device.
Start it from this project directory with a new Atom Lite connected. The agent
should perform the computer-side work and pause only for the stated physical or
Home Assistant actions.

```text
Set up the XGIMI Titan Noir Max M5Stack Atom Lite remote project in the current
directory on Windows, macOS or Linux. A new M5Stack Atom Lite is connected to
this computer by a data-capable USB cable. You may inspect files, create a local
Python virtual environment, install the pinned requirements, use this
computer's Bluetooth, and compile and flash the connected Atom.

Important project contract:
- The Titan Noir Max HID descriptor, all button codes, entity names, icons and
  alternate actions are already captured and implemented. Do not flash a HID
  learner and do not ask me to recapture or map any buttons.
- The only XGIMI-specific value you must learn is my original remote's stable
  15-byte wake token.
- Never print or commit Wi-Fi credentials, API keys, OTA passwords or the final
  token unnecessarily. Ensure secrets.yaml remains ignored by Git.
- Do not change the device or Bluetooth name from "M5Stack Atom Lite".

Please do the following:
1. Inspect README.md, the ESPHome YAML and scripts before acting.
2. Detect the operating system. Create .venv and install requirements.txt using
   the correct executable paths: .venv/bin on macOS/Linux or .venv\Scripts on
   Windows. Do not assume POSIX shell commands on Windows.
3. Ask me to physically unplug the projector and place the original remote near
   this computer. Then run scripts/capture_wake_token.py for long enough to
   capture multiple advertisements while I press Power several times. Help me
   enable Bluetooth and grant permission if the operating system requires it.
4. Accept the token only when the XGIMI company ID 0x0046 payload is 16 bytes,
   multiple observations have different first-byte counter values, and the
   remaining 15 bytes are identical. Retry rather than guessing if that check
   fails.
5. Run scripts/create_secrets.py with that 15-byte token so it creates
   secrets.yaml with a 32-byte base64 ESPHome API encryption key and strong
   random OTA and fallback-AP passwords. Do not modify secrets.example.yaml
   with my values and do not overwrite an existing secrets.yaml without asking.
6. Validate and compile m5stack-atom-lite-xgimi-remote.yaml. Resolve the Atom's
   USB serial port unambiguously (`COM…` on Windows or `/dev/…` on
   macOS/Linux), show me which port you found, then flash it. Do not flash any
   unrelated serial device.
7. Confirm from serial logs that version 2.9.0 boots and advertises as
   "M5Stack Atom Lite". Do not press remote buttons during verification.
8. Tell me to join the "M5Stack Atom Lite Setup" Wi-Fi using the fallback AP
   password, select my main Wi-Fi in the captive portal, and wait for the Atom
   to appear on the network.
9. Tell me to power on the projector and pair "M5Stack Atom Lite" in its
   Bluetooth accessory/remote settings. The original remote should remain
   paired too.
10. Tell me how to add the discovered ESPHome device in Home Assistant and use
    the generated API key if asked. If network access is available, verify the
    device reports project version 2.9.0 and all 23 expected remote buttons,
    including "Settings Menu" and "Game Menu", without activating any button.
11. Finish with a concise test: turn the projector off, wait for "Projector
    Remote Connected" to become false, then press "Power On" once.

Stop and ask me before any step that requires a physical action, Bluetooth or
serial permission, choosing between multiple serial devices, Wi-Fi credentials,
projector interaction, or Home Assistant interaction. Preserve evidence of the
captured token validation, but redact the token itself from the final summary.
```

## Project layout

- `m5stack-atom-lite-xgimi-remote.yaml` — ESPHome device, HID services and HA entities
- `components/xgimi_remote/` — wake sequencing, connection guard and HID reports
- `components/esp32_ble_server/` — BLE server support adapted for this HID peripheral
- `scripts/capture_wake_token.py` — captures and validates only the per-remote token
- `scripts/create_secrets.py` — creates cross-platform credentials and token config
- `docs/protocol.md` — sanitised, model-wide HID capture reference
- `secrets.example.yaml` — safe template; copy to ignored `secrets.yaml`

## Scope and safety

This was captured and tested on an XGIMI Titan Noir Max. Other XGIMI models or
remote revisions may use a different HID map or wake format. This project is
independent and is not affiliated with XGIMI, M5Stack, ESPHome or Home Assistant.

## Licence

This project is distributed under GPLv3 because its adapted ESPHome C++ runtime
code is GPLv3. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
