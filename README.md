# XGIMI Titan Noir IR Remote via ESP32

<img width="1200" height="312" alt="banner" src="https://github.com/user-attachments/assets/0817c01b-239d-4115-843b-b0c1d7a0783f" />


This project makes an ESP32 board into an IR remote to Bluetooth translator for XGIMI Titan Noir projectors. Titan Noir projectors lack IR control capability and only accept bluetooth signals. This translator works around that limitation by translating infrared signals into bluetooth commands the projector accepts.

Three different IR remotes are supported for translation. Typically, users add one of these devices definitions to a universal remote rather than employing the actual physical remote . Once the translator is active, you can control your Titan Noir projector via IR signals from your universal remote.

The IR code sets translated are:

1.  TiVO Roamio
2.  Hisense 50U6G
3.  VC-VCR HR-S9600U
4.  LG Cinebeam HU810P

#### \*\*\* This fork does **NOT** require presence nor use of a Home Assistant Server \*\*\*

This fork concentrates on direct IR translation to Xgimi Titan Noir bluetooth and HA usage is not documented here. However, _Home Assistant_ integration capability has been preserved and can also be used.. See mrmachine's original github for _Home Assistant_ related information.

## Requirements

*   A BlueTooth BLE Capable ESP32 Board. Two boards detailed here are...
    * ESP32-C6-WROOM-1
    * ESP32-C3 SuperMini development board with integrated 0.42-inch OLED display

*   IR sensor module (three pin) for ESP32
    
*   XGIMI Titan Noir Max and its original Bluetooth remote
    
*   Windows, macOS or Linux computer with Bluetooth and Python 3.14 or newer
    
*   Data capable USB cable
    

Your IR sensor module needs three connections to your ESP32 board.

1.  IR signal --> GPIO pin (actual pin varies with board)
2.  IR Gnd --> Ground
3.  IR Vcc --> 3V (Avoid connecting Vcc to 5V to avoid risk of ESP32 board damage)
    

Here are pinouts of two styles of IR sensors and the correct connection points on a ESP32-C6-WROOM-1 board.
<img width="1000" height="827" alt="pinouts ir sensor and esp32-c6-wroom-1" src="https://github.com/user-attachments/assets/eb46dd0e-6ddc-4379-a20e-1d3b8a705a19" />

The IR sensor style that has the small pc board includes a red LED which lights with IR presence. That feedback LED may be useful during troubleshooting, but is not required. Either style of IR sensor will work.

### Selecting an ESP32 Board

If you want to use an ESP32 board than the two examples illustrated herein, be certain it has at least bluetooth version 4.2 and BLE (Bluetooth Low Energy) capability. Otherwise, power-on broadcast to Xgimi projector will not work.

Boards that should work:

*   ESP32-S3
*   ESP32-C3
*   ESP32-C6
*   ESP32 pico d4
*   ESP32-H2
*   ESP32-H4
    
<br>
Boards that will NOT work

*   Original ESP32 (WROOM-32 /DevKitC)
*   ESP32-S2 series
    
<br>
Each variation of ESP board has its own pinout and specific GPIO pins suitable for for IR signal input. It is particularly important to obtain pinout information and know which GPIO pins are actually free for use (not strapping pins or already assigned to other board functions)  

I have pre-selected appropriate GPIO pins for the two specific boards used during creation of this project. The example "make" files provided have already been configured for those specific boards. Easiest way to proceed is to use one of the exact same boards as I have for this project. If you follow the same board selection and pin wiring that I made, you can directly use one of the supplied make files to create an IR to Bluetooth translator.

### Using Other ESP Boards

To use a different board, obtain its pinout diagram and identify a free GPIO pin suitable for IR input. Beware that some GPIO pins are used for special functions such as boot strapping, timing signals, or may be allocated to devices on the PC board.  
Once you know the board pinout and identify an appropriate GPIO pin, make a copy of one of my "make" files and customize it for your particula board.

Typically only three values in the configuration section at start of file need be adjusted for your board.

1.  board\_type
2.  pin\_ir\_receiver
3.  pin\_optional\_wake\_btn
    

```yaml
# ============= Begin Configuration to Match Physical ESP32 Board and Wiring. KUO
# Also see end of this file where include package sets which IR remote is to be decoded

  ble_remote_name: "Hisense IR to Xgimi"  # Name remote to indicate IR remote expected. KUO
  
  board_type: "esp32-c3-devkitm-1" # <== set board type here
  framework_type: "esp-idf" # <====  set board framewark. 
  pin_ir_receiver: GPIO1 # <======= set GPIO pin for IR sensor. 
  pin_optional_wake_btn: GPIO3 # <= set GPIO pin for optional wake button. 

# Except for selection of IR mapping file at end, the remainder of this
# file typically needs no further changes to adapt to board and wiring
# ============= End of settings for configurig board KUO
```

Also, At the bottom of the make file is where you choose the IR remote code set desired (TiVo vs Hisense vs JVC VCR).

IR code selection is via uncommenting **_one and ONLY one_** line specifying the irmap\_package to include as IR map.

#### ESP32-C6-WROOM-1 Pinout and GPIO Pin Selection Example

ESP32-C6-WROOM-1 board has GPIO10 freely usable as input for I used as IR signal input. Pin GPIO10 has already been specified in the "make" files provided in this project. I have marked the connectors of interest on its pinout diagram.
<img width="1326" height="712" alt="pinouts esp32-c6-wroom-1-marked" src="https://github.com/user-attachments/assets/c01e511b-e68a-442d-b883-c19569586023" />


#### ESP32-C3 SuperMini Dev Board with Integrated 0.42-inch OLED Display Example

Lonely Binary's ESP32-C3 OLED board has GPIO01 usable for IR signal input. GPIO01 is on same side as its 3V and Gnd connectos, making wiring simpler than GPIO pins on the opposing edge of the board.
<img width="1500" height="983" alt="esp32-c3 with OLED pins marked" src="https://github.com/user-attachments/assets/33e10bc0-86a0-48b4-ab33-a4aaa9b7b8ed" />


#### Completed ESP32 Boards

Here are two ESP32-C6-WROOM-1 boards wired with two different style IR sensors.
<img width="1000" height="624" alt="ir to xgimi BLE completed" src="https://github.com/user-attachments/assets/f6219f27-d2a9-41a8-a603-ff9e0276e722" />


Here is ESP32-C3 OLED board. My firmware displays inbound IR codes and equivalent Xgimi projector translation.
<img width="1300" height="620" alt="oled esp32-c3" src="https://github.com/user-attachments/assets/04c25845-2c3f-4be7-bd48-4127073c7f61" />

#### Powering Your ESP32 Board

ESP32 boards require very modest poewr for operation. Any stable USB-C power supply should suffice.

\[This one works well.\]([https://www.amazon.com/dp/B0DZ6J62C3](https://www.amazon.com/dp/B0DZ6J62C3))

### How Bluetooth to Titan Noir Works

When the projector is awake, the ESP32 maintains a bonded Bluetooth HID connection and sends the same keyboard or consumer-control reports captured from the original remote. Settings Menu and Focus use the two distinct short/alternate reports produced by the physical remote. Immediate power-off holds the Power report for the confirmed 1500 ms duration.

When the projector is fully asleep, Power On broadcasts all 256 rolling-counter values with the original remote's stable 15-byte wake token. There is no deliberate delay between advertisements. A connection-state guard prevents a wake burst while the projector is already connected, avoiding advancement of the projector's rolling-code state at the wrong time.

The firmware contains the tested Titan Noir Max HID descriptor and button map.

Your specific token lives in the `secrets.yaml`, not in the shareable source.

Mrmachine already captured and mapped an original Titan Noir Max remote. We do not need to learn its buttons, HID descriptor, names or Home Assistant entities. That has already been done for us by mrmachine.

**_The only per-remote value we must acquire is the original remote's 15-byte BLE wake token._**

## Installing This Software on ESP32

### 1\. Clone or copy this directory to your computer.
Cloning and downloading controls are within Gitbub green "<> Code" button.
<img width="856" height="802" alt="gihub directory clone" src="https://github.com/user-attachments/assets/f7c4ce8f-e0fb-49c3-a893-f6e6023dad4e" />


<br>
<br>
### Create a virtual environment, and install the pinned tools

On macOS or Linux, cd to directory of this project. You can readily do so by typing "cd " and dragging your directory into terminal.
Once terminal is at the correct working directory, you can proceed with below scripts.

### 2\. Install Python environment

On MacOS or Linux

```sh
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
```

On Windows PowerShell or Command Prompt:

```powershell
py -3 -m venv .venv
.venv\\Scripts\\python.exe -m pip install -r requirements.txt
```

### 3\. Capture Your Remote's Token

Physically unplug the projector so it cannot reconnect to the original remote. Turn Bluetooth on and grant the terminal Bluetooth access if the operating system asks.

Run one of:

On MacOS or Linux

```sh
.venv/bin/python scripts/capture_wake_token.py --duration 30
```

On Windows Powershell

```powershell
.venv\Scripts\python.exe scripts\capture_wake_token.py --duration 30
```

Keep the remote close to the computer and press its Power button several times during the scan. A trustworthy capture has a stable 15-byte tail and at least two distinct first-byte rolling-counter values.

### 4\. Create `secrets.yaml` with the captured token.

The helper generates the API key and strong OTA/fallback-access-point passwords using Python's securerandom generator:

Replace the below script's token with the 15-byte token printed by the capture

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

### 5\. Build and Flash Firmware for Your Choice of IR Remote

For your convenience, YAML "make" files have been created to directly support the two example ESP32 boards with any of three IR code sets. The name of provided make files indicate board type, GPIO pin, and IR code set.  
  
Substitute name of specific "make" file for the one listed in below example scripts to create the version desired.  
If you create a "make" file for a different board, GPIO, or IR code, substitute your make filename in the scripts.

#### 5a. Build firmware for TIVO on ESP32-C6

Connect the new ESP32-C6 by USB, validate, compile and flash:

MacOS or Linux

```sh

.venv/bin/esphome config make-esp32-c6-wroom-1-GPIO10-IR-TiVo.yaml
.venv/bin/esphome run make-esp32-c6-wroom-1-GPIO10-IR-TiVo.yaml

```

Windows Powershell

```powershell

.venv\\Scripts\\esphome.exe config make-esp32-c6-wroom-1-GPIO10-IR-TiVo.yaml
.venv\\Scripts\\esphome.exe run make-esp32-c6-wroom-1-GPIO10-IR-TiVo.yaml

```

#### 5b. Build Firmware for HISENSE on ESP32-C3 with OLED display)

Connect the new ESP32-C3 by USB, validate, compile and flash:

MacOS or Linux

```sh

.venv/bin/esphome config make-esp32-c3-OLED-GPIO1-IR-Hisense.yaml
.venv/bin/esphome run make-esp32-c3-OLED-GPIO1-IR-Hisense.yaml

```

Windows Powershell

```powershell

.venv\\Scripts\\esphome.exe make-esp32-c3-OLED-GPIO1-IR-Hisense.yaml
.venv\\Scripts\\esphome.exe make-esp32-c3-OLED-GPIO1-IR-Hisense.yaml

```

Once firmware is built, you should be presented with a choice of how to flash your ESP32 board.

Select the ESP32's USB serial port when prompted `COM…` on Windows or

`/dev/…` on macOS/Linux). If no port appears, install the USB serial driver required by the ESP32 USB interface and try again.

## 6\. Power on the projector and Add Your ESP32 Remote as another Bluetooth remote.

The provided make files name your new ESP32 as readily recognizable when adding as another Bluetooth remote.

Keep the original remote paired as well.
<br>
<br>
<br>
### 7\. Test your TiVo / Hisense IR remote's ability to control your Titan Noir projector.

For universal remotes, add a TiVo Roamio or Hisense 50U6G device to your remote.

CAUTION: Do not commit your `secrets.yaml` or a personalised firmware binary: both contain device credentials, and the binary also embeds the wake token.

<br>
<br>

## Xgimi Command and IR Remote Button Tables
<br>

### Tivo to Xgimi Titan Noir Mapping
|Xgimi Remote Command | Tivo Remote | NEC IR code of Tivo Roamio Remote |
|-------- | -------- | -------------------- |
|back |zoom |address=0x3085, command=0xB044 |
|cursor\_down |arrow down |address=0x3085, command=0xE016 |
|cursor\_enter |select |address=0x3085, command=0xE019 |
|cursor\_left |arrow left |address=0x3085, command=0xE017 |
|cursor\_right |arrow right |address=0x3085, command=0xE015 |
|cursor\_up |arrow up |address=0x3085, command=0xE014 |
|focus\_auto |7 |address=0x3085, command=0xD02E |
|focus\_manual |8 |address=0x3085, command=0xD02F |
|home |channel up |address=0x3085, command=0xE01E |
|input |input |address=0x3085, command=0xC034 |
|settings\_menu |tivo (actual remote) |address=0x3085, command=0xF00C |
|settings\_menu |tivo (incorrectly on MyHarmony) |address=0x3085, command=0xF00D |
|game\_menu |guide |address=0x3085, command=0xC036 |
|mute |mute |address=0x3085, command=0xE01B |
|picture |info |address=0x3085, command=0xE013 |
|power\_on |TV Power (Use as Discrete Power On) |address=0x3085, command=0xE010 |
|power\_off |Live TV (Use as Discrete Off) |address=0x3085, command=0xE011 |
|power\_off |0 (equivalent as Discrete OFF) |address=0x3085, command=0xC031 |
|shortcut\_1 |A yellow |address=0x3085, command=0x9060 |
|shortcut\_2 |B blue |address=0x3085, command=0x9061 |
|shortcut\_3 |C red |address=0x3085, command=0x9062 |
|shortcut\_4 |D green |address=0x3085, command=0x9063 |
|volume\_down |volume down |address=0x3085, command=0xE01D |
|volume\_up |volume up |address=0x3085, command=0xE01C |
|bluetooth\_pairing\_start |enter |address=0x3085, command=0xC033 |
|bluetooth\_pairing\_clear |clear |address=0x3085, command=0xC032 |

<br>
<br>

### Hisense to Xgimi Titan Noir Mapping
|Xgimi Remote Command | Hisense Remote Button | NEC IR code of Hisense 50U6G Remote |
|-------- | -------- | -------------------- |
|back |back |0xFB04 |
|cursor\_down |arrow down |0xFB04 |
|cursor\_enter |select |0xA55A |
|cursor\_left |arrow left |0xA758 |
|cursor\_right |arrow right |0xA659 |
|cursor\_up |arrow up |0xA956 |
|focus\_auto |7 |0xE817 |
|focus\_manual |8 |0xE718 |
|settings\_menu |menu (on actual remote) |0xBC43 |
|settings\_menu |menu (incorrectly in MyHarmony ) |0xB54A |
|home |home (on actual remote) |0xB54A |
|home |home (on My Harmony) |0x718E |
|input |input |0xF609 |
|game\_menu |apps |0x35CA |
|mute |mute |0xF609 |
|picture |channel up |0xFF00 |
|power\_on |discrete power on |0x8E71 |
|power\_off |discrete power off |0xEF10 |
|power\_off |0 (Do NOT USE) |0x8D72 |
|shortcut\_1 |yellow |0xAB54 |
|shortcut\_2 |blue |0xAA55 |
|shortcut\_3 |red |0xAD52 |
|shortcut\_4 |green |0xAC53 |
|volume\_down |volume down |0xFC03 |
|volume\_up |volume up |0xFD02 |
|bluetooth\_pairing\_start |prime video |0xB847 |
|bluetooth\_pairing\_clear |youtube |0xB649 |

<br>
<br>

### LG Cinebeam HU810P to Xgimi Titan Noir Mapping

(Some items duplicated to accept alternative buttons)
| Xgimi Remote Command | LG Cinebeam Button | NEC IR Code |
| :--- | :--- | :--- |
| back | back | 0x14EB |
| cursor down | cursor down | 0x827D |
| cursor enter |OK  | 0x22DD |
| cursor left | cursor left | 0xE01F |
| cursor right | cursor right | 0x609F |
| cursor up | cursor up | 0x02FD |
| focus auto | aspect ratio | 0x9E61 |
| focus manual | play | 0x0DF2 |
| home | home menu | 0x3EC1 |
| input | input | 0xD02F |
| settings_menu | Quick Menu | 0xC23D |
| settings_menu | Picture Mode | 0xB24D |
| game_menu | channel down | 0x807F |
| mute | mute | 0x906F |
| picture | picture | 0xA956 |
| power on | Discrete Power On | 0xB34C |
| power on | Discrete Power On | 0x23DC |
| power on | Power Toggle | 0x10EF |
| power off | Discrete Power Off | 0xA53C |
| power off | 0 | 0x08F7 |
| shortcut 1 | 1 | 0x8877 |
| shortcut 2 | 2 | 0x48B7 |
| shortcut 3 | 3 | 0xC837 |
| shortcut 4 | 4 | 0x28D7 |
| shortcut 1 | red | 0x4EB1 |
| shortcut 2 | green | 0x8E71 |
| shortcut 3 | yellow | 0xC639 |
| shortcut 4 | blue | 0x8679 |
| volume down | volume down | 0xC03F |
| volume up | volume up | 0x40BF |
| BT start pairing | netflix | 0x6A95 |
| BT clear pairing | prime | 0x3AC5 |

<br>
<br>

### JVC-VCR to Xgimi Titan Noir Mapping
| Xgimi Remote Command | JVC VCR Remote Btn | JVC IR code from JVC VCR |
| :--- | :--- | :--- |
| back | rewind | 0xC2C3 |
| cursor\_down | arrow down actual | 0xC218 |
| cursor\_down | arrow down harmony | 0xC261 |
| cursor\_enter | OK | 0xC23C |
| cursor\_left | arrow left | 0xC2A8 |
| cursor\_right | arrow right | 0xC228 |
| cursor\_up | arrow up harmony | 0xC241 |
| cursor\_up | arrow up actual | 0xC298 |
| focus\_auto | 7 | 0xC2E4 |
| focus\_manual | 8 | 0xC214 |
| home | cancel | 0xC26C |
| input | tv/vcr | 0xC2C8 |
| settings\_menu | menu actual remote | 0xC2EC |
| settings\_menu | menu harmony | 0xC207 |
| game\_menu | play | 0xC230 |
| mute | pause | 0xC2B0 |
| picture | fast forward | 0xC260 |
| power\_on | power toggle | 0xC2D0 |
| power\_on | power on | 0xC2B8 |
| power\_off | power off | 0xC258 |
| power\_off | 0 | 0xC2CC |
| shortcut\_1 | Prog | 0xC283 |
| shortcut\_2 | Prog Check | 0xC2BC |
| shortcut\_3 | SP/EP | 0xC28C |
| shortcut\_4 | Skip Search | 0xC269 |
| volume\_down | Start - | 0xC293 |
| volume\_up | Start + | 0xC213 |
| bluetooth\_pairing\_start | 1 | 0xC284 |
| bluetooth\_pairing\_clear | 2 | 0xC244 |



## Recent Notes:
*   Added support for LG Cinebeam HU810P IR codes

*   Added support for JVC VCR remote IR codes
    
*   Changed file naming convention to board-GPIOpin-IRmapping
    
*   WARNING - Using 5v for Vcc on IR sensors may endanger the ESP32 boards.  
    Changed pinout diagrams and instructions to use the 3 volt board connection.
    
*   Refactored to support of other ESP32 boards via adjusting a few entries in "make" file.
    
*   Latest bluetooth stack from mrmachine incorporated into fork
    
*   Adjusted IR code mapping to work around errors in remote definitions stored  
    at MyHarmony's library. With new mappings, translator works with codes learned from  
    actual remote or ones downloaded from MyHarmony library.
    
*   Logging output from ESP32-C6-WROOM is only on its USB port. THe COMS port can  
    be used for programming the ESP32, but will not output any log data.
    
*   If older Python is on your Mac, completes flashing ESP32 successfully,  
    but does not start logging. Instead an architecture error appears in terminal.  
    Fix this issue by updating to python-3.14.7 before building firmware.
    

## Scope and safety

This was captured and tested on an XGIMI Titan Noir Max. Other XGIMI models or remote revisions may use a different HID map or wake format. This project is independent and is not affiliated with XGIMI, M5Stack, ESPHome or Home Assistant.

## License

This project is distributed under GPLv3 because its adapted ESPHome C++ runtime code is GPLv3. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
