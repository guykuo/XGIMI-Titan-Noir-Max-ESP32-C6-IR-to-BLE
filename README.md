# Infrared XGIMI Titan Noir Max Remote via an ESP32-C6

<img width="1200" height="312" alt="ir-esp32-ble-xgimi" src="https://github.com/user-attachments/assets/c805cb25-0852-43c9-b07e-095054d24b49" />

## What This Does

This project turns an ESP32-C6 into an IR remote to Bluetooth translator
for XGIMI Titan Noir projectors. Titan Noir projectors lack IR input and
can only accept bluetooth signals. This translator works around that limitation
by enabling use of a TiVO Roamio or Hisense 50U6G infrared remote.

You choose which IR codes are accepted by installing the software version for desired remote.

Once IR control is enabled, universal remotes that communicate via IR, like the
Logitech Harmony Elite, can control your Titan Noir projector through use
of TiVo or Hisens IR codes.

This fork is based upon mrmachine's github which focuses on Home Assistant
control of the projector. Instead, this fork concentrates on IR control.
However, Home Assistant integration capability has been preserved.

*** This fork does NOT require presence nor use of a Home Assistant Server ***

Please see the original mrmachine github for Home Assistant related information.

## Requirements

- XGIMI Titan Noir Max and its original Bluetooth remote
- ESP32-C6-WROOM-1 (or equivalent ESP32) with a data-capable USB cable
- IR sensor module
- Windows, macOS or Linux computer with Bluetooth and Python 3.14 or newer

The ESP32-C6 gets wired to your 3 pin IR sensor via three connections. 

- GPIO10 <- IR receiver signal
- Gnd <- IR receiver Gnd
- 5V <- IR reciver Vcc

Here are pinouts of two styles of IR sensors and the respective connection points 
on a ESP32-C6-WROOM-1 board. 

The IR sensor style that has the small pc board includes a feedback LED that 
flashes red when the sensor detect IR. That feedback LED can be useful during
troubleshooting whether the sensor is working, but that style sensor may be 
less sensitive to IR. Either style of sensor will work for this project

<img width="1406" height="1164" alt="pinouts" src="https://github.com/user-attachments/assets/36d02d3a-eb5e-479e-a601-ad8ae361fd3b" />

** IMPORTANT ** If you use a different ESP32 board, be certain it has at least bluetooth
version 4.2 AND Bluetooth Low Energy (BLE) capability. Otherwise, power-on broadcast to 
Xgimi projector will not work. 

- Should work with: ESP32-S3, ESP32-C (C3, C6), esp32 pico d4, possibly ESP32-H2 / H4
  
- Avoid original ESP32 (WROOM-32 /DevKitC), and ESP32-S2 Series

If a board, other than the ESP32-C6-WROOM-1 is used, you MUST change board definition and specify
the actual GPIO pin you use in my YAMLs. For simplicity, I suggest using exactly the 
ESP32-C6-WROOM-1 to avoid any need to edit my YAML or find your own pinouts.

Pinout for the ESP32-C6-WROOM-1 board I used is below. I have marked the connectors of interest.
<img width="1000" height="540" alt="ESP32-C6 -WROOM-1" src="https://github.com/user-attachments/assets/c19da29d-63c3-4620-8664-57e0bb5918c4" />


Here is a completed translator wired with IR sensor that does not have feedback LED
<img width="1200" height="900" alt="completed esp32IR" src="https://github.com/user-attachments/assets/15c51e16-8e48-4e8f-91a3-542f1145a961" />


If using another board, check its pinouts for location of GPIO-10, 5V, and GND. Don't assume
they are in same physical location between differing boards. For instance, below is a
pinout for another EPS32-C6 board

<img width="896" height="990" alt="71CVmIKYAyL _AC_SL1080_" src="https://github.com/user-attachments/assets/02ca05f1-13a3-41c3-8e4e-64bf1c060016" />

Sometimes, there is not GPIO10 to use and you must use another GPIO pin. This ESP32-C3 with 
OLED display has a recommendation of GPIO04 for IR sensor. 

Again, you must find its equivalent connectors to wire and also change the board id in the YAML.
<img width="1500" height="1194" alt="esp32-c3 with OLEDd" src="https://github.com/user-attachments/assets/a5a1b437-23d3-4db0-abfc-8ed8d99c2364" />

This project supports the esp32-c3 OLED display to show information about received IR signals.
<img width="1300" height="620" alt="oled esp32-c3" src="https://github.com/user-attachments/assets/e1fbe2bf-8b2d-46c1-8c74-4ffbb251cc33" />

Easiest is to use the same exact same board as mine, but it is possible to user other boards.

To power the ESP32, you will need a small USB-C power supply for your ESP32-C6.
[This one works well.](https://www.amazon.com/dp/B0DZ6J62C3)


## How Bluetooth to Titan Noir Works

When the projector is awake, the ESP32 maintains a bonded Bluetooth HID
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
Your specific token lives in the `secrets.yaml`, not in the shareable source.

Mrmachine already captured and mapped an original Titan Noir Max remote. We do not
need to learn its buttons, HID descriptor, names or Home Assistant entities. That
has already been done for us by mrmachine.

** The only per-remote value we must acquire is the original remote's
15-byte BLE wake token.

## Installing This Software on ESP32

1. Clone or copy this directory to your computer. 
   (Under MacOS, compiling works better if directory is on desktop.)
   
   Create a virtual environment, and install the pinned tools:

   On macOS or Linux, cd to directory of this project. You can readily do so
   by type "cd " and then dragging your director into terminal.

   Once your terminal is at the correct working directory, you can proceed
   with below scripts.

   Install Python environment
   On MacOS or Linux
   ```sh
   python3 -m venv .venv
   .venv/bin/python -m pip install -r requirements.txt
   ```

   On Windows PowerShell or Command Prompt:
   ```powershell
   py -3 -m venv .venv
   .venv\Scripts\python.exe -m pip install -r requirements.txt
   ```

3. Capture Your Remote's Token

   Physically unplug the projector so it cannot reconnect to the original
   remote. Turn Bluetooth on and grant the terminal Bluetooth access if the
   operating system asks. Run one of:

   On MacOS or Linux
   ```sh
   .venv/bin/python scripts/capture_wake_token.py --duration 30
   ```

   On Windows Powershell
   ```powershell
   .venv\Scripts\python.exe scripts\capture_wake_token.py --duration 30
   ```

   Keep the remote close to the computer and press its Power button several
   times during the scan. A trustworthy capture has a stable 15-byte tail and
   at least two distinct first-byte rolling-counter values.


4. Create `secrets.yaml` with the captured token.
   The helper generates the API key and strong OTA/fallback-access-point passwords using Python's secure
   random generator:

   Replace the below script token with the 15-byte token printed by the capture

   MacOS or Linux
   ```sh
   .venv/bin/python scripts/create_secrets.py --token "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88"
   ```

   Windows Powershell
   ```powershell
   .venv\Scripts\python.exe scripts\create_secrets.py --token "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88"
   ```

   Now you have a secrets.yaml file containing your specific token.
   
   The helper refuses to overwrite an existing `secrets.yaml`.

   
5. Build Firmware for Your Choice of IR Remote (TiVo vs Hisense)

   YAML files are supplied to directly support two boards and either TiVo Roamio or Hisense 50U6G Remote 

   Substitute name of "make" file for the one listed in below example scripts to create the version
   desired.

   4a (Build Firmware for TIVO on ESP32-C6) Connect the new ESP32-C6 by USB, validate, compile and flash:

   MacOS or Linux
   ```sh
   .venv/bin/esphome config make-IR-TiVo-esp32-c6-wroom-1.yaml
   .venv/bin/esphome run make-IR-TiVo-esp32-c6-wroom-1.yaml
   ```

   Windows Powershell
   ```powershell
   .venv\Scripts\esphome.exe config make-IR-TiVo-esp32-c6-wroom-1.yaml
   .venv\Scripts\esphome.exe run make-IR-TiVo-esp32-c6-wroom-1.yaml
   ```


   4b (Build Firmware for HISENSE on ESP32-C3 OLED) Connect the new ESP32-C3 by USB, validate, compile and flash:

   MacOS or Linux
   ```sh
   .venv/bin/esphome config make-IR-Hisense-esp32-c6-wroom-1.yaml
   .venv/bin/esphome run make-IR-Hisense-esp32-c6-wroom-1.yaml
   ```

   Windows Powershell
   ```powershell
   .venv\Scripts\esphome.exe config make-IR-Hisense-esp32-c6-wroom-1.yaml
   .venv\Scripts\esphome.exe run make-IR-Hisense-esp32-c6-wroom-1.yaml
   ```


   Select the ESP32's USB serial port when prompted (`COM…` on Windows or
   `/dev/…` on macOS/Linux). If no port appears, install the USB serial driver
   required by the ESP32 USB interface and reconnect it.

  
7. Power on the projector and add your TiVo/dHisense to Xgimi as another Bluetooth remote.
   Keep the original remote paired as well.

   
8. Test the TiVo or Hisense IR remote's ability to control your Titan Noir projector. 
   If you are using a universal remote, add a TiVo Roamio or Hisense 50U6G device to your remote.
   
   Tables listing Xgimi button and corresponding TiVO or Hisense remote button are below.

   Note: My Harmony library TiVo Roamio has a faulty TiVo button stored.
   Recommend relearning that TiVo button on your Harmony Elite (Fix button in My Harmony)


Do not commit your `secrets.yaml` or a personalised firmware binary: both contain
device credentials, and the binary also embeds the wake token.


## Tivo IR code and Xgimi Titan code table

|Xgimi Remote Command | NEC IR code of Tivo Roamio Remote | Tivo Remote |
|-------- | -------------------- | -------- |
|back	|address=0x3085, command=0xB044	|zoom |
|cursor_down	|address=0x3085, command=0xE016	|arrow down
|cursor_enter	|address=0x3085, command=0xE019	|select |
|cursor_left	|address=0x3085, command=0xE017	|arrow left |
|cursor_right	|address=0x3085, command=0xE015	|arrow right |
|cursor_up	|address=0x3085, command=0xE014	|arrow up |
|focus_auto	|address=0x3085, command=0xD02E	|7 |
|focus_manual	|address=0x3085, command=0xD02F	|8 |
|home	|address=0x3085, command=0xE01E	|channel up |
|input	|address=0x3085, command=0xC034	|input |
|settings_menu	|address=0x3085, command=0xF00C	|tivo (actual remote) |
|settings_menu	|address=0x3085, command=0xF00D	|tivo (incorrectly on MyHarmony) ||
|game_menu	|address=0x3085, command=0xC036	|guide |
|mute	|address=0x3085, command=0xE01B	|mute |
|picture	|address=0x3085, command=0xE013	|info |
|power_on	|address=0x3085, command=0xE010	|TV Power (Use as Discrete Power On)|
|power_off	|address=0x3085, command=0xE011	|Live TV (Use as Discrete Off)|
|power_off	|address=0x3085, command=0xC031	|0 (equivalent as Discrete OFF)|
|shortcut_1	|address=0x3085, command=0x9060	|A yellow |
|shortcut_2	|address=0x3085, command=0x9061	|B blue |
|shortcut_3	|address=0x3085, command=0x9062	|C red |
|shortcut_4	|address=0x3085, command=0x9063	|D green |
|volume_down	|address=0x3085, command=0xE01D	|volume down |
|volume_up	|address=0x3085, command=0xE01C	|volume up |
|bluetooth_pairing_start	|address=0x3085, command=0xC033	|enter |
|bluetooth_pairing_clear	|address=0x3085, command=0xC032	|clear |


## Hisense IR code and Xgimi Titan code table

|Xgimi Remote Command | NEC IR code of Hisense 50U6G Remote | Hisense Remote Button |
|-------- | -------------------- | -------- |
|back	|0xFB04	|back |
|cursor_down	|0xFB04	|arrow down
|cursor_enter	|0xA55A	|select |
|cursor_left	|0xA758	|arrow left |
|cursor_right	|0xA659	|arrow right |
|cursor_up	|0xA956	|arrow up |
|focus_auto	|0xE817	|7 |
|focus_manual	|0xE718	|8 |
|settings_menu	|0xBC43	|menu (on actual remote) |
|settings_menu	|0xB54A	|menu (incorrectly in MyHarmony ) |
|home	|0xB54A	|home (on actual remote)|
|home	|0x718E	|home (on My Harmony)|
|input	|0xF609	|input |
|game_menu	|0x35CA	|apps |
|mute	|0xF609	|mute |
|picture	|0xFF00	|channel up |
|power_on	|0x8E71	|discrete power on |
|power_off	|0xEF10	|discrete power off |
|power_off	|0x8D72	| 0 (Do NOT USE)|
|shortcut_1	|0xAB54	|yellow |
|shortcut_2	|0xAA55	|blue |
|shortcut_3	|0xAD52	|red |
|shortcut_4	|0xAC53	|green |
|volume_down	|0xFC03 |
|volume_up	|0xFD02	|
|bluetooth_pairing_start	0xB847	|prime video |
|bluetooth_pairing_clear	|0xB649	|youtube |

## Project layout

- `esp32c6-IR-to-xgimiBLE-KuoHisense.yaml` — Hisense version of ESP32 IR remote
- `esp32c6-IR-to-xgimiBLE-KuoTiVo.yaml` — TiVO IR Hisense version of ESP32 IR remote
- `components/xgimi_remote/` — wake sequencing, connection guard and HID reports
- `components/esp32_ble_server/` — BLE server support adapted for this HID peripheral
- `scripts/capture_wake_token.py` — captures and validates only the per-remote token
- `scripts/create_secrets.py` — creates cross-platform credentials and token config
- `docs/protocol.md` — sanitised, model-wide HID capture reference
- `secrets.example.yaml` — safe template; copy to ignored `secrets.yaml`

## Recent Notes:
- Latest bluetooth stack from mrmachine incorporated into fork

- Thanks to techjmw at AVSforum for work re Hisense 50U6G code mapping from Harmony library.

- Logging output from ESP32-C6-WROOM is only out its USB port. THe COMS port can be used
for programming the ESP32, but will not output any log data.

- If older Python is on your Mac, completes flashing ESP32 successfully, but does not
start logging. Instead an architecture error appears in terminal.

Fix this issue by updating to python-3.14.7 before building firmware.


## Scope and safety

This was captured and tested on an XGIMI Titan Noir Max. Other XGIMI models or
remote revisions may use a different HID map or wake format. This project is
independent and is not affiliated with XGIMI, M5Stack, ESPHome or Home Assistant.

## Licence

This project is distributed under GPLv3 because its adapted ESPHome C++ runtime
code is GPLv3. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
