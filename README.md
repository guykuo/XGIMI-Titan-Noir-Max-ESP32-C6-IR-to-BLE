# XGIMI Titan Noir IR Remote via ESP32

<img width="1200" alt="banner for git" src="https://github.com/user-attachments/assets/7fcef2fa-453c-4113-b921-eb75118d7cbc" />



This project transforms an ESP32 board into an IR remote to Bluetooth translator for XGIMI Titan Noir projectors. Titan Noir projectors lack IR control capability and only accept bluetooth signals. This translator works around that limitation by translating infrared signals into bluetooth commands the projector accepts. Typically, users add one of these devices definitions to a universal remote and control their Titan Noir by sending IR codes specified to be translated. This effectively adds IR capability to the otherwise bluetooth bound Titan Noir.

Note: This fork concentrates on direct IR translation to Xgimi Titan Noir bluetooth. Home Assistant usage is not documented here. However, _Home Assistant_ integration capability has been preserved and can also be used.. See mrmachine's original github for _Home Assistant_ related information.

This translator requires the special wake token transmitted by your original Xgimi remote. The translator ESP32 board can itself sniff the token from your original remote. 

You can additionally do a tedious, manual capture and process the hexadecimal token into a secrets.yaml file, but it is much easier to skip that process and let the translator ESP32 sniff and capture the wake token.

## Requirements

*   A BlueTooth BLE Capable ESP32 Board. Two boards detailed here are...
    * ESP32-C3 SuperMini dev board with integrated 0.42-inch OLED display (current preferred)
    * ESP32-C6-WROOM-1 (good choice)

*   IR sensor module (three pin) for ESP32
    
*   XGIMI Titan Noir Max and its original Bluetooth remote
    
*   Windows, macOS or Linux computer with Bluetooth and Python 3.14 or newer
    
*   Data capable USB cable

  
## What You Will Accomplish
Getting the translator to work requires you to...

* Choose IR mapping to translate
* Obtain ESP32 board and IR sensor to be this translator
* Choose Name of the translator 
* Assemble ESP32 board and IR sensor
* Create a copy of make-esp32-TEMPLATE.yaml
* Rename it something like make-my-spec-board.yaml
* Edit make-my-spec-board.yaml to specify name for your remote, which ESP32 board you are using, and which IR translation map.
* Create compiler evironment
* Compile your new ESP32 firmware and send it to your ESP32 board.
* Set up your universal remote to use the IR mapped codes.


# Layout of this Git
Project Git is arranged as below. You will find make, hardware, and IRmap files that are combined to create your own combination of IR code set translation and particular ESP32 board the translator is to run upon.

<img width="1200" height="1319" alt="project git layout" src="https://github.com/user-attachments/assets/4c0e3408-2380-4790-8e67-8c003cb49541" />





# The Software
## Make Files
"Make" files are where you specify desired combination of board type, IR mapping, and name your new IR translator. Several example make files are provided.
These each specify a ESP32 board type and desired IR mapping, but you can also create your own combinations.

* make-esp32-c3-OLED-GPIO1-IR-Hisense.yaml
* make-esp32-c3-OLED-GPIO1-IR-LGcinebeam.yaml
* make-esp32-c3-OLED-GPIO1-IR-TiVo.yaml
* make-esp32-c6-wroom-1-GPIO10-IR-Hisense.yaml
* make-esp32-c6-wroom-1-GPIO10-IR-TiVo.yaml
* make-esp32-m5stack-atom-lite-Hisense.yaml
* make-esp32-TEMPLATE.yaml

You create your own combinations by 

1 Cloning make-esp32-TEMPLATE.yaml 
2 Editing your custom make file

At the top of your custom make file are places to specify remote name, esp32 board, and desired IR mapping.

Be sure to leave the ir-common-kuo/ prefix when you replaced the HARDWARE and IRMAP
Also, do not move or rename existing files in ir-common-kuo/ subdirectory

```YAML
# ============= BEGIN Configuration Options KUO ==========

# Only three things to configure here
#   ble_remote_name
#   hardware_package
#   irmap_package

substitutions:
  ble_remote_name: "IR to Xgimi"  # <===== Name your IR remote. (20 char max)

packages: 

  # ******* NB do not accidentally leave out prefix ir-common-kuo/

  # --- hardware settings of ESP32 board
  hardware_package: !include ir-common-kuo/HARDWAREFILE.yaml # <===== Replace HARDWAREFILE.yaml with your board.yaml

  # --- IR mapping options
  irmap_package:    !include ir-common-kuo/IRMAP.yaml # <===== Replace IRMAP.yaml with your IRMAP.yaml 
```
You will find available hardware and irmap files in ir-common-kuo/ subdirectory


## IR Maps
In terms of IR mapping, this translator project includes several IR translations as irmap files.

The main IR maps, which have been vetted, were chosen because they are less likely to be in a home theater that is using an Xgimi Titan Noir projector.
You should usually choose one that does not conflict with existing devices in your system.

An alternate strategy for chossing an IRmap is to intentionally choose a mapping for a projector that is already in your universal remote but is being replaced.
This lets the Titan Noir simply take over the old projector's already configured role in your universal remote. However, the old projector cannot be
used simultaneously under this strategy.

The following IR maps have working IR code sets ...

* irmap-tivo-roamio.yaml          (TiVo Roamio)
* irmap-LG-cinebeam-hu810p.yaml   (LG Cinebeam HU810P projector)
* irmap-hisense-50u6g.yaml        (Hisense 50U6G TV)
* irmap-jvc-hr-S9600u.yaml        (JVC-VCR HR-S9600U)
* irmap-jvc-projector.yaml
* irmap-sony-projector.yaml
* irmap-sony-tv.yaml
  

The following IR maps have not been vetted and may have incomplete or incorrect IR mapping...

* irmap-AWOL-projector.yaml
* irmap-benq-projector.yaml
* irmap-epson-projector.yaml
* irmap-hisense-projector.yaml
* irmap-optoma-projector.yaml


## Hardware Definition Packages
Board specific information are stored in ir-common-kuo/ as hardware.yaml files. These hardware files contain board specific information used to create the translator firmware. Four board types are supplied. You can also create your "hardware" file to support a new ESP32 board type or customize GPIO assignments.

Supplied hardware files are for ESP32 boards ...

* hardware-c3.yaml
* hardware-c6.yaml
* hardware-m5stack-atom-lite.yaml
* hardware-s3-waveshare-lcd-1.47b.yaml

Look in the hardware file for suggested GPIO pins for IR receiver and optional i2c display
  
# The Hardware
## Selecting an ESP32 Board
I recommend using an ESP32-C3 with on-board OLED display. Although it is possible to sniff tokens "blind," the OLED display gives better feedback during setup and troubleshooting IR codes. Boards that lack a display are also usable, but you will only have LED flashes for feedback during setup. 

## Boards Tested and Known to Work..

### ESP32-C3 with built in 0.42 OLD display. Many clones are available.
<img width="300" alt="esp32 c3 OLED" src="https://github.com/user-attachments/assets/f5da1f5f-e7a5-4947-a166-1334c34a57c2" />

[Here is a good quality set of 3 including breakout boards](https://www.amazon.com/dp/B0G6YT4ZQ3)

[A cheaper, usable clone with thinner PCB and often less well aligned OLD display ](https://www.amazon.com/dp/B0F59L9RMR)

<br>
<br>


### ESP32-C6-WROOM-1
The [ESP32-C6-WROOM-1](https://www.amazon.com/dp/B0H1GGL9L1?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1) does not have a display, but does include a multi-color neopixel LED that gives some feedback. 
<img width="500" alt="ESP32-C6-WROOM-1" src="https://github.com/user-attachments/assets/c580d0eb-c7f6-480c-9d7d-152ecd95c489" />


You can optionally connect a [SSD1306 128x32 0.91-inch OLED display module](https://www.amazon.com/dp/B0GX9245FD) to this board. One might even connect the OLED display only during setup and troubleshooting. Support for adding a display is already in my hardware-c6.yaml file.

<img width="636" height="222" alt="0 91-inch OLED display module" src="https://github.com/user-attachments/assets/49b91c07-67c8-4109-bf33-dbb260e0f5d2" />

<br>
<br>
<br>
### Waveshare ESP32-S3 1.47inch LCD Display Development Board (Revision B)
This ESP32-S3 board was used to verify translator functionality on S3 boards. It has a larger LCD display and dissipates quite a bit more power than the C3 and C6 boards. I would only use this as a testing and setup board. The larger screen enables easier reading of captured IR codes while editing irmap files. It is probably to bright and power hungry for home theater deployment as full time translator. There are many similar boards, but variants are not always GPIO pin matches for the ones specified in my hardware-s3-waveshare-lcd-1.47B.yaml

Be sure to get exactly [Waveshare ESP S3 LCD 1.47B board](https://www.amazon.com/dp/B0FBWPJPXN) if you want to use my hardware config without searching for correct pinouts.


<img width="550" alt="Screenshot 2026-08-23 at 13 55 31" src="https://github.com/user-attachments/assets/8d91d927-70b3-4f66-9c51-759854cdc0fb" />

<br>
<br>
<br>
### Other ESP32 Boards
If you want to use an ESP32 board than the two examples illustrated herein, be certain it has at least bluetooth version 4.2 and BLE (Bluetooth Low Energy) capability. Otherwise, power-on broadcast to Xgimi projector will not work.

Board series that should work:

*   ESP32-S3 (uses more electricity than C3 and C6)
*   ESP32-C3
*   ESP32-C6
*   ESP32 pico d4
    
<br>
Boards that will NOT work

*   Original ESP32 (WROOM-32 /DevKitC)
*   ESP32-S2 series
    
<br>
Each variation of ESP board has its own pinout and specific GPIO pins suitable for for IR signal input. It is particularly important to obtain pinout information and know which GPIO pins are actually free for use (not strapping pins or already assigned to other board functions). Because that process can be overwhelming, four board configuration "hardware" files have been supplied. These pre-define board type and pinouts. Most likely, you can simply specify one of the supplied hardware files within your make file.

Example "make" files provided have already been configured for those specific boards. Easiest way to proceed is to use one of the exact same boards as I have for this project. If you follow the same board selection and pin wiring that I made, you can directly use one of the supplied make files to create an IR to Bluetooth translator.

### Using Other ESP Boards

Skip this step if you are using one of already tested boards. To use a different board, obtain its pinout diagram and identify a free GPIO pin suitable for IR input. Beware that some GPIO pins are used for special functions such as boot strapping, timing signals, or may be allocated to devices on the PC board.

Once you know the board pinout and identify an appropriate GPIO pin, make a copy of one of my "hardware" files within ir-common-kuo directory. Name your copy uniquely and and customize it for your particular board.

Adjust values in your hardware yaml for your particular board.

```yaml
# ============= BEGIN Configuration Options KUO ==========

substitutions:
  
  pin_ir_receiver: GPIO1   # IR sensor. 
  pin_indicator_LED: GPIO8 # LED indicator (both ESP32-C3 and C6)
  pin_sniff_btn: GPIO9     # use boot button for snifff. Both ESP32-c3 and ESP32-C6 use pin 9 for boot button. 
  
  
# Core Architecture Declaration
esp32:
  board: esp32-c3-devkitm-1
  framework:
    type: esp-idf
  
# i2c Display Pins
i2c: 
  sda: GPIO5
  scl: GPIO6
  frequency: 400kHz
  scan: false # to prevent startup crashes during boot scans
  id: bus_a
  
# ============= END Configuration Options KUO ==========

```

Most users are better off obtaining one of the already tested and known working ESP32 boards.


### ESP32-C6-WROOM-1 Pinout and GPIO Pin Selection Example

ESP32-C6-WROOM-1 board has GPIO10 freely usable as input for I used as IR signal input. Pin GPIO10 has already been specified in the "make" files provided in this project. I have marked the connectors of interest on its pinout diagram.
<img width="1326" height="712" alt="pinouts esp32-c6-wroom-1-marked" src="https://github.com/user-attachments/assets/c01e511b-e68a-442d-b883-c19569586023" />

### Adafruit ESP32-C6-DevKitC-1 N8 Pinout and GPIO Pin Selection Example

Although this is also an ESP32-C6 board, its pinout differs from above C6 board. I give this as a reminder that when selecting other boards, you MUST obtain its pinout diagram to know which connectors are to be used. On this alternative board, GPIO 10, GND, and 3V are in totally different places. Also GPIO 10 is not pre-assigned and available to use as IR receiver pin. If your board already has GPIO 10 unavailable, you would need to to create a new hardware file with the correct specifications for board and pins.

<img width="1300" alt="5672_esp32-c6-devkitc-1-pin-layout" src="https://github.com/user-attachments/assets/0ffb0a8e-b76c-4adf-bfc6-18cbf68f614f" />




### ESP32-C3 SuperMini Dev Board with Integrated 0.42-inch OLED Display Example

Lonely Binary's ESP32-C3 OLED board has GPIO01 usable for IR signal input. GPIO01 is on same side as its 3V and Gnd connectos, making wiring simpler than GPIO pins on the opposing edge of the board.
<img width="1500" height="983" alt="esp32-c3 with OLED pins marked" src="https://github.com/user-attachments/assets/33e10bc0-86a0-48b4-ab33-a4aaa9b7b8ed" />


### Completed ESP32 Boards

Here are two ESP32-C6-WROOM-1 boards wired with two different style IR sensors.
<img width="1000" height="624" alt="ir to xgimi BLE completed" src="https://github.com/user-attachments/assets/f6219f27-d2a9-41a8-a603-ff9e0276e722" />


Here are ESP32-C3 OLED boards. My firmware displays inbound IR codes and equivalent Xgimi projector translation.
<img width="1300" height="620" alt="oled esp32-c3" src="https://github.com/user-attachments/assets/04c25845-2c3f-4be7-bd48-4127073c7f61" />

Such tiny boards to the job and provide good feedback via OLED display. Flexible sensor wire leads allow turning LED and OLED away from viewer while keeping IR sensor pointed in direction of IR signal. UV cured resin encapsulates IR sensor wire joints in this example.

<img width="1000" height="743" alt="ESP32 C3 with IR sensor" src="https://github.com/user-attachments/assets/9f39a4ef-bfd0-496b-b69d-b79c36f6db5b" />


## Powering Your ESP32 Board

ESP32 boards require very modest power for operation. Any stable USB-C power supply should suffice.

[These work well](https://www.amazon.com/dp/B0DZ6J62C3) and include a varied length assortment of DATA capable cables.

<img width="400" alt="usbc power supply" src="https://github.com/user-attachments/assets/fe43824f-542d-45b6-890a-67b16024e743" />



## IR Sensor

Your IR sensor module needs three connections to your ESP32 board.

1.  IR signal --> GPIO pin (actual pin varies with board)
2.  IR Gnd --> Ground
3.  IR Vcc --> 3V (Avoid connecting Vcc to 5V to avoid risk of ESP32 board damage)
    

Here are pinouts of two styles of IR sensors and the correct connection points on a ESP32-C3 with built in OLED display.
<img width="1000" height="827" alt="pinouts c3" src="https://github.com/user-attachments/assets/ed81b73e-706c-42e6-9b2c-3ecd1d09404e" />

The IR sensor style that has the small pc board includes a red LED which lights with IR presence. That feedback LED may be useful during troubleshooting, but is not required. Either style of IR sensor will work.

Here is an alternative board I have also tested, ESP32-C6-WROOM-1. This one does not have a built-in display, but this project will run on it and uses its single light to give feedback.

<img width="1000" height="827" alt="pinouts c6" src="https://github.com/user-attachments/assets/52b63bcd-5724-4e93-91a2-a50d24705c71" />




# How Bluetooth to Titan Noir Works

When the projector is awake, the ESP32 maintains a bonded Bluetooth HID connection and sends the same keyboard or consumer-control reports captured from the original remote. Settings Menu and Focus use the two distinct short/alternate reports produced by the physical remote. Immediate power-off holds the Power report for the confirmed 1500 ms duration.

When the projector is fully asleep, Power On broadcasts all 256 rolling-counter values with the original remote's stable 15-byte wake token. There is no deliberate delay between advertisements. A connection-state guard prevents a wake burst while the projector is already connected, avoiding advancement of the projector's rolling-code state at the wrong time.

The firmware contains the tested Titan Noir Max HID descriptor and button map.

Your specific token lives in the `secrets.yaml`, not in the shareable source.

Mrmachine already captured and mapped an original Titan Noir Max remote. We do not need to learn its buttons, HID descriptor, names or Home Assistant entities. That has already been done for us by mrmachine.

**_The only per-remote value we must acquire is the original remote's 15-byte BLE wake token._**


# Installing This Software on ESP32

## 1\. Clone or copy this directory to your computer.
Cloning and downloading controls are within Gitbub green "<> Code" button.

<img width="400" alt="gihub directory clone" src="https://github.com/user-attachments/assets/f7c4ce8f-e0fb-49c3-a893-f6e6023dad4e" />



<br>

### Create a virtual environment, and install the pinned tools

On macOS or Linux, cd to directory of this project. You can readily do so by typing "cd " and dragging your directory into terminal.
Once terminal is at the correct working directory, you can proceed with below scripts.

## 2\. Install Python environment

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

## 3\. OPTIONAL STEP - Manually Capture Your Remote's Token

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

## 4. Create `secrets.yaml` with optional captured token. 

(NOTE: You if you skipped prior manual capture of Remote's token, you do not need to replace the below token hex numbers. Just leave them alone
because you will be capturing your token on the translator later)

The helper generates the API key and strong OTA/fallback-access-point passwords using Python's securerandom generator:

(Optional) Replace the below script's token with the 15-byte token printed by the capture

MacOS or Linux

```sh
.venv/bin/python scripts/create_secrets.py --token "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88"
```

Windows Powershell

```powershell
.venv\Scripts\python.exe scripts\create_secrets.py --token "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88"
```

Now you have a secrets.yaml file that (optionally) contains your specific token.

The helper refuses to overwrite an existing `secrets.yaml`.

## 5. Build and Flash Firmware for Your Choice of IR Remote

For your convenience, YAML "make" files have been created to directly support the two example ESP32 boards with any of three IR code sets. The name of provided make files indicate board type, GPIO pin, and IR code set.  
  
Substitute name of specific "make" file for the one listed in below example scripts to create the version desired.  
If you create a "make" file for a different board / IR mapping combination, substitute your own make filename in the scripts.

### 5a. Example Build firmware for TIVO on ESP32-C6

Connect the new ESP32-C6 by USB, validate, compile and flash:

MacOS or Linux

```sh
.venv/bin/esphome run make-esp32-c6-wroom-1-GPIO10-IR-TiVo.yaml
```

Windows Powershell

```powershell
.venv\\Scripts\\esphome.exe run make-esp32-c6-wroom-1-GPIO10-IR-TiVo.yaml

```

### 5b. Example Build Firmware for HISENSE on ESP32-C3 with OLED display)

Connect the new ESP32-C3 by USB, validate, compile and flash:

MacOS or Linux

```sh
.venv/bin/esphome run make-esp32-c3-OLED-GPIO1-IR-Hisense.yaml

```

Windows Powershell

```powershell
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


## 7\. Learning Wake Token from Xgimi Remote.

Unless your secrets.yaml already has a valid wake token, your ESP32 translator needs to learn a wake token from your original Xgimi remote.
Sniffing for the wake token should be done with projector power disconnected because you will be pressing the remote's power button repeatedly.

- Remove AC power from Xgimi Titan Noir Projector
- ESP32 should be plugged into 5 volt USB-C power
- Point infrared remote at ESP32 IR receiver and press its "4" button to start sniffing.
      Alternatively, press the "boot" button of your ESP32 to start sniffing.
      Sniffing remains active for 20 seconds. Light comes on to indicate sniffing mode.
  
- Repeatedly press power button on Xgimi remote with it near your ESP32 board.
      Usually, just 4 to 6 presses are needed to sniff a valid token. 
      If you have an ESP32-C3 with built-in OLED display, sniff mode and capture will be shown.
      On boards lacking a display, light will flash 8 times indicating capture of new token.
      If light flashes 3 times, captured token was identical to one already stored.

- Wait at least five seconds to let the ESP32 commit the token to NVS flash storage.
     Once flashed to permanent storage, the learned token persists across power and boot cycles.
     There is also a clear token (5 on remote). To clear a learned token press 5 twice within 5 seconds.
     Normally, there is no need to clear a learned token, unless you wish to revert to the one
     specified in secret.yaml. Because sniffing a token is so simple, you likely did not manually
     capture and place one in your secrets.yaml. Easiest is to sniff with the ESP board.

What the token buttons do
  - Token Sniff - Button (4) ESP32 sniffs for an Xgimi a wake token for 20 seconds. If one is accepted, It is stored into NV storage.
  - Token Clear - Button (5) Must be pressed twice within 5 seconds. Stored token is removed from NV storage. Active wake token becomes the one supplied in secrets.yaml
  - Token Recall - Button (6) displays currently stored token (if one is present). This is useful to verifying the token that was sniffed correctly.


    

CAUTION: Do not commit your `secrets.yaml` or a personalised firmware binary: both contain device credentials, and the binary also embeds the wake token.

<br>
<br>

# Xgimi Command and IR Remote Button Tables
<br>

## Tivo to Xgimi Titan Noir Mapping
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
|token\_sniff |4 |address=0x3085, command=0xD02B |
|token\_clear |5 |address=0x3085, command=0xD02C |
|token\_recall |6 |address=0x3085, command=0xD02D |
|bluetooth\_pairing\_start |enter |address=0x3085, command=0xC033 |
|bluetooth\_pairing\_clear |clear |address=0x3085, command=0xC032 |

<br>
<br>

## Hisense to Xgimi Titan Noir Mapping
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
|token\_sniff |4 |0xEB14 |
|token\_clear |5 |0xEA15 |
|token\_recall |6 |0xE916 |
|bluetooth\_pairing\_start |prime video |0xB847 |
|bluetooth\_pairing\_clear |youtube |0xB649 |

<br>
<br>

## LG Cinebeam HU810P to Xgimi Titan Noir Mapping

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
| shortcut 1 | red | 0x4EB1 |
| shortcut 2 | green | 0x8E71 |
| shortcut 3 | yellow | 0xC639 |
| shortcut 4 | blue | 0x8679 |
| volume down | volume down | 0xC03F |
| volume up | volume up | 0x40BF |
| token sniff | 4 | 0x28D7 |
| token clear | 5 | 0xA857 |
| token recall | 6 | 0x6897 |
| BT start pairing | netflix | 0x6A95 |
| BT clear pairing | prime | 0x3AC5 |

<br>
<br>

## JVC-VCR to Xgimi Titan Noir Mapping
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
| power\_off | audio monitor | 0xC2E8 |
| power\_off | 0 | 0xC2CC |
| shortcut\_1 | Prog | 0xC283 |
| shortcut\_2 | Prog Check | 0xC2BC |
| shortcut\_3 | SP/EP | 0xC28C |
| shortcut\_4 | Skip Search | 0xC269 |
| volume\_down | Start - | 0xC293 |
| volume\_up | Start + | 0xC213 |
| token\_sniff | 4| 0xC224 |
| token\_clear | 5 | 0xC2A4 |
| token\_recall | 6| 0xC264 |
| bluetooth\_pairing\_start | 1 | 0xC284 |
| bluetooth\_pairing\_clear | 2 | 0xC244 |







# Recent Changes:
*   Further adjusted button acceptance speed to approx 10 press/sec.
*   Added support for holding down button on remote for fast repeat.
*   Added hardware file for Waveshare ESP32 S3 LCD 1.47B board.
*   Revamped IR debouncing to improve responsiveness.
*   Hardware-c6 supports SSD1306 128x32 i2c display
*   Multiple hardware and ir-map files added to project
*   Added support for LED light on boards C3 and C6 to act as visual feedback
*   Pressing BOOT button of board is alternate way to start sniff mode.
*   Light flashes give information about token presence and sniffing success
*   Sniffing a token that is identical to one already stored no longer seems to fail.
*   Sniffing of Xgimi tokens directly by ESP32 board fully working. Captures and
stores wake token. It's now simply a matter of pressing a button on the IR remote
twice to start sniffing. Four to six presses of Xgimi remote power button is usually
sufficient to get a capture. Learned token is kept on ESP non-volatile flash.

*   Supplying a wake token in secret.yaml is now optional.
  
*   ESP Board preference is now OLED display bearing ESP32-C3 SuperMini dev board.
OLED display nicely shows when board begins token sniffing and completes capture.
Non-display ESP32 will still work, but gives feedback only via log.


    

## Scope and safety

This was captured and tested on an XGIMI Titan Noir Max. Other XGIMI models or remote revisions may use a different HID map or wake format. This project is independent and is not affiliated with XGIMI, M5Stack, ESPHome or Home Assistant.

## License

This project is distributed under GPLv3 because its adapted ESPHome C++ runtime code is GPLv3. See `LICENSE` and `THIRD_PARTY_NOTICES.md`.
