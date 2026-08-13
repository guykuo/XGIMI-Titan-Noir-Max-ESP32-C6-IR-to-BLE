# Infrared XGIMI Titan Noir Max Remote via an ESP32-C6

<img width="1200" height="312" alt="ir-esp32-ble-xgimi" src="https://github.com/user-attachments/assets/c805cb25-0852-43c9-b07e-095054d24b49" />

## What This Does

This project turns an ESP32-C6 into an IR remote to Bluetooth translator
for XGIMI Titan Noir projectors. Titan Noir projectors lack IR input and
can only accept bluetooth signals. This translator works around that limitation
by enabling use of a TiVO Roamio, Hisense 50U6G, or JVC-VCR infrared remote.

You choose which IR codes are accepted by installing the software version for desired remote.

Once IR control is enabled, universal remotes that communicate via IR, like the
Logitech Harmony Elite, can control your Titan Noir projector through use
of TiVo, Hisense, or JVC-VCR IR codes.

This fork is based upon mrmachine's github which focuses on Home Assistant
control of the projector. Instead, this fork concentrates on IR control.
However, Home Assistant integration capability has been preserved.

*** This fork does NOT require presence nor use of a Home Assistant Server ***

Please see the original mrmachine github for Home Assistant related information.

## Requirements

- XGIMI Titan Noir Max and its original Bluetooth remote
- A BlueTooth BLE Capable ESP32 Board such as...
   - ESP32-C6-WROOM-1 
   - ESP32-C3 SuperMini development board with integrated 0.42-inch OLED display
- Data capable USB cable
- IR sensor module (three pin)
- Windows, macOS or Linux computer with Bluetooth and Python 3.14 or newer

The ESP32-C6 gets wired to your 3 pin IR sensor via three connections. 

- GPIO pin <- IR receiver signal (actual pin varies with board)
- Gnd <- IR receiver Gnd
- 3V <- IR reciver Vcc (Avoid connecting Vcc to 5V for ESP32 chip safety)

Here are pinouts of two styles of IR sensors and the respective connection points 
on a ESP32-C6-WROOM-1 board. 

<img width="1000" height="827" alt="pinouts ir sensor and esp32-c6-wroom-1" src="https://github.com/user-attachments/assets/37c8a749-6a99-43d8-a89c-f2404c5ad8be" />

The IR sensor style that has the small pc board includes a red LED that 
flashes when the sensor detect IR. That feedback LED may be useful during
troubleshooting, but is not required. Either style of sensor will work.

** IMPORTANT ** If you want to use a different ESP32 board than the ones illustrated herein, 
be certain it has at least bluetooth version 4.2 and Bluetooth Low Energy (BLE) capability. 
Otherwise, power-on broadcast to Xgimi projector will not work. 

- These Should Work: ESP32-S3, ESP32-C (C3, C6), esp32 pico d4, possibly ESP32-H2 / H4
  
- Avoid These board: Original ESP32 (WROOM-32 /DevKitC), and ESP32-S2 Series

You MUST know your board's pinout and which GPIO pins are suitable for IR signal input.
I have included the pinouts for the two specific boards used in my testing.
The files provided here have already been configured for those two boards.

If you use another board, you will need to modify the beginning of one my "make" files
to match your board. Typically only three values in the configuration section at
start of file need be adjusted for your board.

- board_type
- pin_ir_receiver
- pin_optional_wake_btn 

```YAML
# ============= Begin Configuration to Match Physical ESP32 Board and Wiring. KUO
# Also see end of this file where include package sets which IR remote is to be decoded (TiVo v Hisens)

  ble_remote_name: "Hisense IR to Xgimi"  # Name remote to indicate IR remote expected. KUO
  
  board_type: "esp32-c6-devkitc-1" # <== set board type here
  framework_type: "esp-idf" # <====  set board framewark. 
  pin_ir_receiver: GPIO10 # <======= set GPIO pin for IR sensor. 
  pin_optional_wake_btn: GPIO11 # <= set GPIO pin for optional wake button. 

# Except for selection of IR mapping file at end, the remainder of this
# file typically needs no further changes to adapt to board and wiring
# ============= End of settings for configurig board KUO
```
Also, at the bottom of the make file is where you select which remote (TiVo vs Hisense vs JVC VCR)
you want the translator to understand. That selection is done by uncommenting one (and ONLY one)
line specifying which irmap_package to include as IR map.


Pinout for the ESP32-C6-WROOM-1 board I used is below. I have marked the connectors of interest.
<img width="1326" height="712" alt="pinouts esp32-c6-wroom-1-marked" src="https://github.com/user-attachments/assets/a876da02-1484-45f8-9585-a5f9fb1a7d7a" />

Here is the pinout for the Lonely Binary ESP32-C3 SuperMini development board with integrated 0.42-inch OLED display used.
<img width="1500" height="983" alt="esp32-c3 with OLED pins marked" src="https://github.com/user-attachments/assets/cebbac3d-2199-4a22-8d92-2a6312e32242" />



Here are ESP32 boards wired with two different style IR sensors. 

** NB: Connecting 5V to Vcc of IR sensor may cause damage to 
ESP32 boards. Rather than wiring as in this pict, wire Vcc to 3 volt pin.
<img width="1000" height="624" alt="ir to xgimi BLE translators" src="https://github.com/user-attachments/assets/cd51003f-41d2-4311-a372-37cbe5ec166d" />



If using another board, check its pinouts for location of GPIO-10, 3V, and GND. Don't assume
they are in same physical location between differing boards.

You must find equivalent connectors to wire and/or change the board id in a make YAML.
<img width="1500" height="1194" alt="esp32-c3 with OLEDd" src="https://github.com/user-attachments/assets/a5a1b437-23d3-4db0-abfc-8ed8d99c2364" />

I particularly enjoy the ESP32-C3 OLED board version. I programmed the board to show display 
commands and underlying IR codes.
<img width="1300" height="620" alt="oled esp32-c3" src="https://github.com/user-attachments/assets/e1fbe2bf-8b2d-46c1-8c74-4ffbb251cc33" />

Easiest way to proceed is to useone of the exact same boards as mine. If you follow the same
pin wiring and board choices that I made, you can directlyuse one of the four make files to create
either a TiVo or Hisense ready translator with either ESP32 board.

However, it is fairly simple to edit one of make files to to use other ESP32 boards.

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

   4a (Example build firmware for TIVO on ESP32-C6) Connect the new ESP32-C6 by USB, validate, compile and flash:

   MacOS or Linux
   ```sh
   .venv/bin/esphome config make-esp32-c6-wroom-1-GPIO10-IR-TiVo.yaml
   .venv/bin/esphome run make-esp32-c6-wroom-1-GPIO10-IR-TiVo.yaml
   ```

   Windows Powershell
   ```powershell
   .venv\Scripts\esphome.exe config make-esp32-c6-wroom-1-GPIO10-IR-TiVo.yaml
   .venv\Scripts\esphome.exe run make-esp32-c6-wroom-1-GPIO10-IR-TiVo.yaml
   ```


   4b (Example build firmware for HISENSE on ESP32-C3 with OLED display) Connect the new ESP32-C3 by USB, validate, compile and flash:

   MacOS or Linux
   ```sh
   .venv/bin/esphome config make-esp32-c3-OLED-GPIO1-IR-Hisense.yaml
   .venv/bin/esphome run make-esp32-c3-OLED-GPIO1-IR-Hisense.yaml
   ```

   Windows Powershell
   ```powershell
   .venv\Scripts\esphome.exe make-esp32-c3-OLED-GPIO1-IR-Hisense.yaml
   .venv\Scripts\esphome.exe make-esp32-c3-OLED-GPIO1-IR-Hisense.yaml
   ```


   Once firmware is built, you should be presented with a choice of how to
   flash your ESP32 board.

   Select the ESP32's USB serial port when prompted (`COM…` on Windows or
   `/dev/…` on macOS/Linux). If no port appears, install the USB serial driver
   required by the ESP32 USB interface and try again.

  
7. Power on the projector and add your TiVo / Hisense to Xgimi as another Bluetooth remote.
   Keep the original remote paired as well.

   
8. Test your TiVo / Hisense IR remote's ability to control your Titan Noir projector. 
   For universal remotes, add a TiVo Roamio or Hisense 50U6G device to your remote.
   
   Tables listing Xgimi button and corresponding TiVO or Hisense remote button are below.


CAUTION: Do not commit your `secrets.yaml` or a personalised firmware binary: both contain
device credentials, and the binary also embeds the wake token.



## Tivo to Xgimi Titan Noir Mapping

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



## Hisense to Xgimi Titan Noir Mapping

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



## JVC-VCR to Xgimi Titan Noir Mapping

# Remote Control Mapping

Below is the IR code mapping between Xgimi remote commands and JVC VCR remote buttons.

| Xgimi Remote Command | JVC IR code from JVC VCR | JVC VCR Remote Btn |
| :--- | :--- | :--- |
| back | 0xC2C3 | rewind |
| cursor_down | 0xC218 | arrow down actual |
| cursor_down | 0xC261 | arrow down harmony |
| cursor_enter | 0xC23C | OK |
| cursor_left | 0xC2A8 | arrow left |
| cursor_right | 0xC228 | arrow right |
| cursor_up | 0xC241 | arrow up harmony |
| cursor_up | 0xC298 | arrow up actual |
| focus_auto | 0xC2E4 | 7 |
| focus_manual | 0xC214 | 8 |
| home | 0xC26C | cancel |
| input | 0xC2C8 | tv/vcr |
| settings_menu | 0xC2EC | menu actual remote |
| settings_menu | 0xC207 | menu harmony |
| game_menu | 0xC230 | play |
| mute | 0xC2B0 | pause |
| picture | 0xC260 | fast forward |
| power_on | 0xC2D0 | power toggle |
| power_on | 0xC2B8 | power on |
| power_off | 0xC258 | power off |
| power_off | 0xC2CC | 0 |
| shortcut_1 | 0xC283 | Prog |
| shortcut_2 | 0xC2BC | Prog Check |
| shortcut_3 | 0xC28C | SP/EP |
| shortcut_4 | 0xC269 | Skip Search |
| volume_down | 0xC293 | Start - |
| volume_up | 0xC213 | Start + |
| bluetooth_pairing_start | 0xC284 | 1 |
| bluetooth_pairing_clear | 0xC244 | 2 |

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
- Added support for JVC VCR remote IR codes

- Changed file naming convention to board-GPIOpin-IRmapping
  
- WARNING - I learned that using 5v for Vcc on IR sensors may endanger the ESP32 boards.
  Changed pinout diagrams and instructions to use the 3 volt board connection.
  
- Refactored to support of other ESP32 boards via adjusting a few entries in "make" file.

- Latest bluetooth stack from mrmachine incorporated into fork

- Adjusted IR code mapping to work around errors in remote definitions stored
  at MyHarmony's library. With new mappings, translator should work with codes learned from actual
  remote or (flawed ones) downloaded from MyHarmony library.

- Logging output from ESP32-C6-WROOM is only on its USB port. THe COMS port can be used
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
