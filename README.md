# XGIMI Titan Noir IR Remote via ESP32

<img width="1200" alt="banner for git" src="https://github.com/user-attachments/assets/7fcef2fa-453c-4113-b921-eb75118d7cbc" />



Titan Noir projectors lack IR control capability and only accept bluetooth signals. This project works around that limitation by translating infrared signals into bluetooth commands the Titan Noir projectors accept. Typical use is to add an Xgimi Titan Noir to a universal IR remote. Depending on which IR code set you choose, you can add as a completely new projector or have the XTN projector masquerade as an existing projector already in your remote. This project runs on low cost ESP32 board.

## Requirements

*   A BlueTooth BLE Capable ESP32 Board. Two boards detailed here are...
    * ESP32-C3 SuperMini dev board with integrated 0.42-inch OLED display (my preferred board)
    * ESP32-C6-WROOM-1 (also good choice)

*   IR sensor module (three pin) for ESP32
    
*   XGIMI Titan Noir Max and its original Bluetooth remote
    
*   Windows, macOS or Linux computer with Bluetooth and Python 3.14 or newer
    
*   Data capable USB cable

This fork concentrates on IR translation to Xgimi Titan Noir bluetooth. The parent fork used Home Assistant HID connectivity which is not documented here. However, _Home Assistant_ integration capability has been preserved. See mrmachine's original github for _Home Assistant_ related information.

Turning on the Xgimi Titan Noir projector requires use of the original Xgimi remote wake token. This translator can acquire that token by sniffing your original remote with the ESP32. 

Optionally, you can perform a more tedious, manual capture of your remote's wake token. Then, process that hexadecimal sequence into a secrets.yaml file. It is much easier to skip that process and let the translator ESP32 sniff and capture the wake token.
  
## What You Will Accomplish
Getting the translator to work requires you to...

* Choose which IR code set to translate
* Obtain ESP32 board and IR sensor to be this translator
* Connect ESP32 board to IR sensor (three wires)
* Create a copy of make-esp32-TEMPLATE.yaml
* Rename it something like make-my-translator.yaml
* Edit make-my-translator.yaml to specify name for your remote, which ESP32 board you are using, and which IR translation map.
* Create compiler evironment
* Compile your new ESP32 firmware and flash it to your ESP32 board.
* Set up your universal remote to use the IR mapped codes. 
       (If you chose an IR mapping which matches a projector already in your remote, 
       your remote can simply control the XTN as that existing projector)


# Layout of this Git
Project Git is arranged as below. You will find make, hardware, and IRmap files that are combined to create your own combination of IR code set translation and particular ESP32 board the translator is to run upon.
<img width="858" height="648" alt="project git layout" src="https://github.com/user-attachments/assets/fe9c50c5-b773-4f4f-9fae-a9b09bbe8d99" />







Available hardware boards are in ir-common-kuo/ subdirectory.


## IR Profiles
This translator project has several "factory" IR profiles built-in. You and can also learn up to five new IR remote profiles if none of the factory profiles are suitable.

The "factory" IR profiles, were selected to avoid conflicts in a home theater.
One should usually choose one that does not conflict with existing devices in your system.

An alternative strategy is to intentionally select a projector that is already configured in your universal remote but being replaced.
This lets the Titan Noir masquerade as the old projector without reconfiguring your universal remote. However, the old projector cannot be
simultaneously used under masquerading strategy.

These "factory" IR profiles have known working IR code sets ...


| | |
| :--- | :--- |
| **benq-w5800** | BenQ W5800 projector |
| **epson-pro-cinema-LS1200** | Epson Projectors |
| **hisense-50u6g** | Hisense 50U6G TV |
| **jvc-hr-S9600u** | JVC HR-S9600U VCR |
| **jvc-rs2-codeset-A** | JVC Projectors code set A - default | 
| **jvc-rs2-codeset-B** | JVC Projectors code set B - alt code set | 
| **LG-cinebeam-hu810p** | LG Cinebeam HU810P projector |
| **optoma-UHD50X** | Optoma UHD50X projector |
| **sony-VPL-XW600ES** | Sony VPL-XW600ES projector |
| **sony-XBR-77A9G** | Sony XBR-77A9G TV |
| **tivo-roamio-TCD846500** | TiVo Roamio |
| **AWOL-projector** | Valerion using Hisense IR codes. WIP and may have incomplete or incorrect IR mapping |
| **Panasonic** | Panasonic projectors. WIP IR mapping |


## Hardware Definition Packages
ESP32 boards are defined in ir-common-kuo/ as hardware.yaml files. These hardware files contain board specific information used to create the translator firmware. Several board types are supplied. You can also create your "hardware" file to support a new ESP32 board type or to customize GPIO assignments.

Supplied hardware files are for ESP32 boards ...

* hardware-esp32-c3-0.42-OLED.yaml
* hardware-esp32-c6-wroom-1.yaml
* hardware-c6-lafvintech-lcd-1.47.yaml
* hardware-m5stack-atom-lite.yaml
* hardware-s3-hosyond-lcd-3.5-touch.yaml
* hardware-s3-waveshare-esp32-s3-touch-lcd-2.8.yaml

Look in the hardware file for actual GPIO pins for IR receiver and optional i2c display
  
# The Hardware
## Selecting an ESP32 Board
I recommend using an ESP32-C3 with on-board OLED display. Although it is possible to sniff tokens "blind," the OLED display gives better feedback during setup and troubleshooting IR codes. Boards that lack a display are also usable, but you will only have LED flashes for feedback during setup. 

## Boards Tested and Known to Work..

### ESP32-C3 with built in 0.42 OLED display. Many clones are available.
<img width="300" alt="esp32 c3 OLED" src="https://github.com/user-attachments/assets/f5da1f5f-e7a5-4947-a166-1334c34a57c2" />

[Here is a good quality set of 3 including breakout boards](https://www.amazon.com/dp/B0G6YT4ZQ3)

[A cheaper, usable clone with thinner PCB and often less well aligned OLED display ](https://www.amazon.com/dp/B0F59L9RMR)

<br>
<br>


### ESP32-C6-WROOM-1
The [ESP32-C6-WROOM-1](https://www.amazon.com/dp/B0H1GGL9L1?ref=ppx_yo2ov_dt_b_fed_asin_title&th=1) does not have a display, but does have a multi-color neopixel LED that gives some feedback. 
<img width="500" alt="ESP32-C6-WROOM-1" src="https://github.com/user-attachments/assets/c580d0eb-c7f6-480c-9d7d-152ecd95c489" />


You can optionally connect a [SSD1306 128x32 0.91-inch OLED display module](https://www.amazon.com/dp/B0GX9245FD) to this board. One might even connect the OLED display only during setup and troubleshooting. Support for adding a display is already in the hardware-c6.yaml file. See that file to learn which pins need to be connected for the display.

<img width="636" height="222" alt="0 91-inch OLED display module" src="https://github.com/user-attachments/assets/49b91c07-67c8-4109-bf33-dbb260e0f5d2" />

<br>
<img width="800" height="692" alt="esp32-c6-with-LCD" src="https://github.com/user-attachments/assets/fadb88ea-8d0b-4cbd-857c-5a6d7f854924" />

<br>

## Other ESP32 Boards

If you want to use an ESP32 board other than the ones herein, be certain it has at least bluetooth version 4.2 and BLE (Bluetooth Low Energy) capability. Otherwise, power-on broadcast to Xgimi projector will not work.

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

Hardware files provided have already been configured for those specific boards. Easiest way to proceed is to use one of the exact same boards as I have for this project. If you follow the same board selection and pin wiring that I made, you can directly use one of the supplied hardwared files when creating an IR to Bluetooth translator.

### Using Other ESP Boards

Skip this step if you are using one of already tested boards. To use a different board, obtain its pinout diagram and identify a free GPIO pin suitable for IR input. Beware that some GPIO pins are used for special functions such as boot strapping, timing signals, or may be allocated to devices on the PC board.

Once you know the board pinout and identify an appropriate GPIO pin, make a copy of one of my "hardware" files within ir-common-kuo directory. Name your copy uniquely and and customize it for your particular board.

Adjust values in your hardware yaml for your particular board.

```yaml
# ============= BEGIN Configuration Options KUO ==========

substitutions:
  pin_ir_receiver: GPIO1   # IR sensor. 
  pin_indicator_LED: GPIO8 # LED indicator (both ESP32-C3 and C6)
  pin_action_btn: GPIO9     # use boot button for actions. Both ESP32-c3 and ESP32-C6 use pin 9 for boot button. 
  wifi_tx_power: "12dB"     # wifi power for this board.

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

You also must edit the make file to include your new hardware file.
```yaml
packages: 
  # --- ESP32 board, Uncomment one (and only one) for your board
  hardware_package: !include ir-common-kuo/hardware-esp32-c3-0.42-OLED.yaml  # supports built-in 0.42 inch OLED display
  #hardware_package: !include ir-common-kuo/hardware-esp32-c6-wroom-1.yaml   # supports external 0.9 inch i2c OLED display
  #hardware_package: !include ir-common-kuo/hardware-c6-lafvintech-lcd-1.47.yaml
  #hardware_package: !include ir-common-kuo/hardware-m5stack-atom-lite.yaml  # supports external 0.9 inch i2c OLED display
  #hardware_package: !include ir-common-kuo/hardware-s3-hosyond-lcd-3.5-touch.yaml
  #hardware_package: !include ir-common-kuo/hardware-s3-waveshare-esp32-s3-touch-lcd-2.8.yaml
  #hardware_package: !include ir-common-kuo/hardware-s3-waveshare-lcd-1.47B.yaml # This board MUST use IR Sensor WITHOUT LED
                                                                             # Like the Vishay TSOP series.  Otherwise 3Vcc load causes crash durng boot.
                                                                                
  #hardware_package: !include ir-common-kuo/HARDWARE.YAML # Or uncomment this line and replace HARDWARE.YAML with your file.

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

Such tiny boards do the job and provide good feedback via OLED display. Flexible sensor wire leads allow turning LED and OLED away from viewer while keeping IR sensor pointed in direction of IR signal. UV cured resin encapsulates IR sensor wire joints in this example.

<img width="1000" height="743" alt="ESP32 C3 with IR sensor" src="https://github.com/user-attachments/assets/9f39a4ef-bfd0-496b-b69d-b79c36f6db5b" />


## Powering Your ESP32 Board

ESP32 boards require very modest power for operation. Any stable USB-C power supply should suffice.

[These work well](https://www.amazon.com/dp/B0DZ6J62C3) and include a varied length assortment of **data capable** cables.

<img width="400" alt="usbc power supply" src="https://github.com/user-attachments/assets/fe43824f-542d-45b6-890a-67b16024e743" />



## IR Sensor

Your IR sensor module needs three connections to your ESP32 board.

1.  IR signal --> GPIO pin (actual pin varies with board)
2.  IR Gnd --> Ground
3.  IR Vcc --> 3V (Avoid connecting Vcc to 5V to avoid risk of ESP32 board damage)
    

Here are pinouts of two styles of IR sensors and the correct connection points on a ESP32-C3 with built in OLED display.
<img width="1000" height="827" alt="pinouts c3" src="https://github.com/user-attachments/assets/ed81b73e-706c-42e6-9b2c-3ecd1d09404e" />

The IR sensor style that has the small pc board includes a red LED which lights with IR presence. That feedback LED may be useful during troubleshooting, but is not required and can cause issues with some ESP32 boards. Either style of IR sensor will work with ESP32 boards that have no or a small OLED display. If board has larger (> 1 inch) LCD display, IR sensors, like VS1838B,  WITHOUT an indicator LED are recommended. _Some_ boards with large, built-in displays cannot tolerate load of a IR sensor with indicator LED on 3Vcc. Brownout of 3Vcc can cause bootup failure. A plain VS1838B IR sensor draws less current and is less likely to cause 3Vcc brownouts.

Here is an alternative board I have also tested, ESP32-C6-WROOM-1. This one does not have a built-in display, but this project will run on it and uses its single light to give feedback.

<img width="1000" height="827" alt="pinouts c6" src="https://github.com/user-attachments/assets/52b63bcd-5724-4e93-91a2-a50d24705c71" />




# How Bluetooth to Titan Noir Works

When the projector is awake, the ESP32 maintains a bonded Bluetooth HID connection and sends the same keyboard or consumer-control reports captured from the original remote. Settings Menu and Focus use the two distinct short/alternate reports produced by the physical remote. Immediate power-off holds the Power report for the confirmed 1500 ms duration.

When the projector is fully asleep, Power On broadcasts all 256 rolling-counter values with the original remote's stable 15-byte wake token. There is no deliberate delay between advertisements. A connection-state guard prevents a wake burst while the projector is already connected, avoiding advancement of the projector's rolling-code state at the wrong time.

The firmware contains the tested Titan Noir Max HID descriptor and button map.

In the translator project, your wake token is usually sniffed and stored directly on your ESP32 board. It can optionally be in the `secrets.yaml`, but is not required to be in `secrets.yaml`.

Mrmachine already captured and mapped an original Titan Noir Max remote. We do not need to learn its buttons, HID descriptor, names or Home Assistant entities. That has already been done for us by mrmachine.



# Installing This Software on ESP32

[![Watch the video](https://img.youtube.com/vi/Q5G-mbGrfUs/0.jpg)](https://www.youtube.com/watch?v=Q5G-mbGrfUs)



## 1\. Download this Git to your computer.
Cloning and downloading controls are within Github green "<> Code" button.

<img width="400" alt="gihub directory clone" src="https://github.com/user-attachments/assets/f7c4ce8f-e0fb-49c3-a893-f6e6023dad4e" />

Expand the downloaded archive.

<br>

## 2\. Create a virtual environment, and install needed tools
### Change working directory of Powershell or Terminal to be the folder containing this Git

Launch PowerShell or Terminal (which depends on your OS).

Type cd -space-

Drag directory of this project into terminal window. Dragging directory into terminal window enters the filepath for you.

Type a return

That should set your shell window working directory to be the one containing this Git.

### Install Python environment
Type in two commands to create python compile and required elements in the Git folder. This readies things for compiling with Python

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


## 3\. OPTIONAL STEP - Manually Capture Your Remote's Token
<details>
<summary>💡 <b>Manual wake token capture</b> (Click to expand)</summary>

You do not have to do this step. The only reason to do so is to have a "default" backup copy of your Xgimi wake token built into the firmware in case you delete the learned token. Generally, including a wake token in secrete.yaml can be skipped because it is so easy to sniff the token with the board running my firmware.

Physically unplug the projector so it cannot reconnect to the original remote. 
Turn Bluetooth on and grant the terminal Bluetooth access if the operating system asks.

Run the bluetooth capture script:

On MacOS or Linux

```sh
.venv/bin/python scripts/capture_wake_token.py --duration 30
```

On Windows Powershell

```powershell
.venv\Scripts\python.exe scripts\capture_wake_token.py --duration 30
```

Keep the remote close to the computer and press its Power button several times during the scan. A trustworthy capture has a stable 15-byte tail and at least two distinct first-byte rolling-counter values.
</details>

## 4\. Create `secrets.yaml` 

You must create a secrets.yaml file using the below script .<b>

If you did the optional step of manually capturing a wake token, substitute the AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88 in the script with your manually captured token.
Otherwise, just leave AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88 as a placeholder.<br>
<br>
<br>
This script generates the API key and strong OTA/fallback-access-point passwords using Python's secure random generator:

MacOS or Linux

```sh
.venv/bin/python scripts/create_secrets.py --token "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88"
```

Windows Powershell

```powershell
.venv\Scripts\python.exe scripts\create_secrets.py --token "AA:BB:CC:DD:EE:FF:00:11:22:33:44:55:66:77:88"
```

Now you should have a secrets.yaml file that (optionally) contains your wake token. 
If you skipped manually capturing a wake token, you will later sniff the token usihg the ESP32 board. Sniffing is easier than manual capture and editing of hex numbers.

The above script refuses to overwrite an existing `secrets.yaml`.
CAUTION: Do not commit your `secrets.yaml` or a personalised firmware binary to GitHub. Both contain device credentials, and the binary also embeds your wake token.

## 5\. Build and Flash Firmware for Your Choice of IR Remote
### Make Files
"Make" files are where you specify desired combination of board type, IR profile, and name your new IR translator. 
A template make files is provided. ***make-esp32-TEMPLATE.yaml***

1 Duplicate make-esp32-TEMPLATE.yaml to create your custom make file<br>
2 Name your custom make file<br>
3 Edit your custom make file

We will name our make file ***makemine.yaml***

Your custom make file should be left in the same directory as the original ***make-esp32-TEMPLATE.yaml*** file.
Also, never move or rename files in ir-common-kuo/ subdirectory<br>
<br><br>
Open ***makemine.yaml*** in a text editor to customize three things within Configuration Options section<br>
You can specify remote name, esp32 board, and default IR profile.<br>
<br><br>
Name of remote is specified by changing name within quotes of line. Here it is *IR to Xgimi Kuo*<br>
* ble_remote_name: "IR to Xgimi Kuo"<br>
<br><br>
Default IR Profile is set by uncommenting one and only one line. Here it is *epson-pro-cinema-LS12000*<br>
You can later override this via profile setting on the ESP32 board.<br>
<br><br>
ESP32 board is set by uncommenting one and only one line. Here it is *hardware-c3.yaml*<br>
<br><br>
```YAML
# ============= BEGIN Configuration Options KUO ======================================================================

# Only three things to configure here
#   ble_remote_name
#   Default IR profile
#   hardware_package

substitutions:

  ble_remote_name: "IR Xgimi Kuo"  # <===== Name your IR remote. (20 char max. No special characters)


  # --- Default IR profile. Uncomment one (and only one)
  #def_profile: "0"  # AWOL-projector (untested, Hisense IR codes)
  #def_profile: "1"  # benq-w5800
  def_profile: "2"  # epson-pro-cinema-LS12000
  #def_profile: "3"  # hisense-50u6g
  #def_profile: "4"  # jvc-hr-S9600u
  #def_profile: "5"  # jvc-rs2-codeset-A 
  #def_profile: "6"  # jvc-rs2-codeset-B 
  #def_profile: "7"  # LG-cinebeam-hu810p 
  #def_profile: "8"  # optoma-UHD50X
  #def_profile: "9"  # sony-VPL-XW600ES 
  #def_profile: "10" # sony-XBR-77A9G 
  #def_profile: "11" # tivo-roamio-TCD846500 
  #def_profile: "12" # panasonic projector


packages: 
  # --- ESP32 board, Uncomment one (and only one) for your board
  hardware_package: !include ir-common-kuo/hardware-esp32-c3-0.42-OLED.yaml  # supports built-in 0.42 inch OLED display
  #hardware_package: !include ir-common-kuo/hardware-esp32-c6-wroom-1.yaml   # supports external 0.9 inch i2c OLED display
  #hardware_package: !include ir-common-kuo/hardware-c6-lafvintech-lcd-1.47.yaml
  #hardware_package: !include ir-common-kuo/hardware-m5stack-atom-lite.yaml  # supports external 0.9 inch i2c OLED display
  #hardware_package: !include ir-common-kuo/hardware-s3-hosyond-lcd-3.5-touch.yaml
  #hardware_package: !include ir-common-kuo/hardware-s3-waveshare-esp32-s3-touch-lcd-2.8.yaml
  #hardware_package: !include ir-common-kuo/hardware-s3-waveshare-lcd-1.47B.yaml # This board MUST use IR Sensor WITHOUT LED
                                                                             # Like the Vishay TSOP series.  Otherwise 3Vcc load causes crash durng boot.
                                                                                
  #hardware_package: !include ir-common-kuo/HARDWARE.YAML # Or uncomment this line and replace HARDWARE.YAML with your file.



# ========== END Configuration Options KUO ==============================================================================


```  
### 5a Build and Flash the Firmware
Once our make file has been created and customized, you are ready to compile and flash firmware to ESP32 board.
Connect your ESP32 board with USB. <br>

Validate, compile and flash:

MacOS or Linux

```sh
.venv/bin/esphome run makemine.yaml
```

Windows Powershell

```powershell
.venv\Scripts\esphome.exe run makemine.yaml

```

Once firmware is compiled, you should be presented with a choice of how to flash your ESP32 board.
Select the serial upload function. Over the air upload or update will NOT work with this project.
Even though the build environment may offer a wireless upload option, always use USB serial upload.

Select the ESP32's USB serial port when prompted `COM…` on Windows or `/dev/…` on macOS/Linux). 
If no port appears, install the USB serial driver required by the ESP32 USB interface and try again.


## 6\. Learning Wake Token from Xgimi Remote.
[![IMAGE ALT TEXT HERE](https://img.youtube.com/vi/Twvc1mUYd0o/0.jpg)](https://www.youtube.com/watch?v=Twvc1mUYd0o)



Unless your secrets.yaml already has a valid wake token, your ESP32 translator needs to learn a wake token from your original Xgimi remote.
Sniffing for the wake token should be done with projector power disconnected because you will be pressing the remote's power button repeatedly.

- Remove AC power from Xgimi Titan Noir Projector
- ESP32 should be plugged into 5 volt USB-C power

- There are two ways to start Sniffing for wake tokens. Easiest is to single click the BOOT button of your ESP32 board. If your translator is already on the profile for your IR remote, the other option is to press the correct button on your IR remote that starts token sniffing. 

A light will come on to indicate sniffing mode. Sniffing remains active for 20 seconds. 
    
- Repeatedly press power button on Xgimi remote with it near your ESP32 board.
      Usually, just 1 to 2 presses are needed to sniff a valid token. 
      If you have an ESP32-C3 with built-in OLED display, sniff mode and capture will be shown.
      On boards lacking a display, light will flash 8 times indicating capture of new token.
      If light flashes 3 times, captured token was identical to one already stored.

- Wait at least five seconds to let the ESP32 commit the token to NVS flash storage.
     Once flashed to permanent storage, the learned token persists across power and boot cycles.
     Normally, there is no need to clear a learned token, unless you wish to revert to the one
     specified in secret.yaml. Because sniffing a token is so simple, you likely did not manually
     capture and place one in your secrets.yaml. Easiest is to sniff with the ESP board.


### What the Token Buttons on Your IR Remote Do
  - Token Sniff - ESP32 sniffs for an Xgimi a wake token for 20 seconds. If one is captured, It is stored into NV storage.
  - Token Clear - Must be pressed and confirmed with OK button. Stored token is removed from NV storage. Active wake token becomes the one supplied in secrets.yaml
  - Token Recall - Makes currently stored token the active token. Shows on display, if one is present). This is useful to verify token was sniffed correctly.

Three buttons on your remote are assigned to control wake token. The actual buttons depend on which IR profile you are using.
|IR Profile | Token Action | Button |
| ---- | ---- | ---- |
| BenQ W5800 | Sniff | info |
| BenQ W5800 | Clear | invert |
| BenQ W5800 | Recall | 3D |
| ---- | ---- | ---- |
| Epson Pro Cinema | Sniff | Frame Interp |
| Epson Pro Cinema | Clear | RGMCMY |
| Epson Pro Cinema | Recall | Pattern |
| ---- | ---- | ---- |
| Hisense 50U6G | Sniff | 4 |
| Hisense 50U6G | Clear | 5 |
| Hisense 50U6G | Recall | 6 |
| ---- | ---- | ---- |
| JVC Projectors | Sniff | cinema |
| JVC Projectors | Clear | natural |
| JVC Projectors | Recall | gamma or HDR |
| ---- | ---- | ---- |
| LG Cinebeam | Sniff | 4 | 
| LG Cinebeam | Clear | 5 |
| LG Cinebeam | Recall | 6 |
| ---- | ---- | ---- |
| Optoma UHD50X | Sniff | user 1 | 
| Optoma UHD50X | Clear | user 2 |
| Optoma UHD50X | Recall | user 3 |    
| ---- | ---- | ---- |
| Sony VPL-XW600ES | Sniff | Brt Cinema | 
| Sony VPL-XW600ES | Clear | Brt TV |
| Sony VPL-XW600ES | Recall | User |
| ---- | ---- | ---- |
| Sony XBR-77A9G | Sniff | 4 | 
| Sony XBR-77A9G | Clear | 5 |
| Sony XBR-77A9G | Recall | 6 |
| ---- | ---- | ---- |
| TiVo Roamio | Sniff | 4 | 
| TiVo Roamio | Clear | 5 |
| TiVo Roamio | Recall | 6 |

## 7\. Selecting and Learning Profiles
### Selecting Profile
* Double-click boot button to put translator into profile setting mode. <br><br>

* Press a button on your desired IR remote and the translator should switch to that remote's profile.<br>
<br><br>
The translator listens for an IR signal. Identifies the projector by matching that signal against signals it knows. User learned remotes have higher precedence over "factory" profiles. Once a profile is set, the ESP32 will listen only for that profile and translate them to Xgmi Titan Noir bluetooth commands.<br>

### Learning a new Profile from IR Remote
In case none of the "factory" profiles are suitable for you system, you can also teach the translator a new IR profile. Learning mode requires a display on your ESP32 board or having the ESP32 board connected to a serial logging session. (Technically, learning will work without a display or log, but that would entail blindly pressing a sequence of 27 IR buttons perfectly.

* Triple-click boot button to enter learning mode.
* As each button namem appears on screen, press the corresponding button on your IR remote
* Once done with all buttons, you select which memory slot to store your new profile via the remote.
<br><br>
  OK = memory 0<br>
  Up = memory 1<br>
  Rt = memory 2<br>
  Down = memory 3<br>
  Left = memory 4<br>

### Learning IR Profile without Display or Log
It is possible, but more prone to error, to press the 27 button sequence of learing an IR profile. LED flashes help users keep track of where they are in sequence.

Triple Click boot button. LED flashes 3 times indicating IR learn mode

Diagram of button sequence and LED flashes. You can interpet the flashes as happening before a button (indicating which button to press next) or you can interpret as the flash confirming the button you just pressed. First column shows flash before button press. 3rd column shows flash after button press.
<img width="700" height="925" alt="IR learn profile LED flash sequence" src="https://github.com/user-attachments/assets/f1b95d5b-0937-49a0-b9fc-011d91130de1" />



## 8\. Add Your ESP32 Remote as to Projector as Additional Bluetooth Device
Turn on your projector with its original remote control. Within settings add your new IR translator as another Bluetooth remote.
<br><br>
Keep the original remote paired as well.
<br>
<br>


# Xgimi Command and IR Remote Button Tables
## Tivo Roamio to Xgimi Titan Noir Mapping
|Xgimi Remote Command | Tivo Remote | (0x3085) + Command |
| :--- | :--- | :--- |
| power_on | tv power (as discrete on) | 0xE010 |
| power_off | live TV (as discrete off) | 0xE011 |
| power_off_alt | 0 | 0xC031 |
| cursor_up | arrow up | 0xE014 |
| cursor_down | arrow down | 0xE016 |
| cursor_left | arrow left | 0xE017 |
| cursor_right | arrow right | 0xE015 |
| cursor_enter | select | 0xE019 |
| back | zoom | 0xB044 |
| settings_menu | tivo | 0xF00C |
| settings_menu | tivo (myHarmony version) | 0xF00D |
| game_menu | guide | 0xC036 |
| home | channel up | 0xE01E |
| input | input | 0xC034 |
| picture | into | 0xE013 |
| focus_auto | 7 | 0xD02E |
| focus_manual | 8 | 0xD02F |
| shortcut_1 | A yellow | 0x9060 |
| shortcut_2 | B blue | 0x9061 |
| shortcut_3 | C red | 0x9062 |
| shortcut_4 | D green | 0x9063 |
| volume_up | volume up | 0xE01C |
| volume_down | volume down | 0xE01D |
| mute | mute | 0xE01B |
| token_sniff | 4 | 0xD02B |
| token_clear | 5 | 0xD02C |
| token_recall | 6 | 0xD02D |
| BT_start_pair | enter | 0xC033 |
| BT_clear_pair | clear | 0xC032 |

## Hisense 50U6G to Xgimi Titan Noir Mapping
|Xgimi Remote Command | Hisense Remote Button | (0xEA15) + Command |
| :--- | :--- | :--- |
| power_on | discrete power on | 0xF708 |
| power_on_alt | discrete power on (variant) | 0x8E71 |
| power_off | discrete power off | 0xEF10 |
| power_off_alt | 0 | 0x8D72 |
| cursor_up | arrow up | 0xA956 |
| cursor_down | arrow down | 0xA857 |
| cursor_left | arrow left | 0xA758 |
| cursor_right | arrow right | 0xA659 |
| cursor_enter | select | 0xA55A |
| back | back | 0xFB04 |
| home | home | 0xBC43 |
| input | input | 0xF40B |
| settings_menu | menu | 0xB54A |
| settings_menu_alt | menu (variant) | 0x718E |
| game_menu | apps | 0x35CA |
| picture | channel up | 0xFF00 |
| focus_manual | 7 | 0xE817 |
| focus_auto | 8 | 0xE718 |
| shortcut_1 | yellow | 0xAB54 |
| shortcut_2 | blue | 0xAA55 |
| shortcut_3 | red | 0xAD52 |
| shortcut_4 | green | 0xAC53 |
| volume_up | volume up | 0xFD02 |
| volume_down | volume down | 0xFC03 |
| mute | mute | 0xF609 |
| token_sniff | 4 | 0xEB14 |
| token_clear | 5 | 0xEA15 |
| token_recall | 6 | 0xE916 |
| BT_start_pair | prime video | 0xB847 |
| BT_clear_pair | youtube | 0xB649 |

## LG Cinebeam HU810P to Xgimi Titan Noir Mapping
(Some items duplicated to accept alternative buttons)
| Xgimi Remote Command | LG Cinebeam Button | (0xFB04) + Command  |
| :--- | :--- | :--- |
| power_on | power toggle | 0xF708 |
| power_on-alt | Discrete Power On | 0x23DC |
| power_off | Discrete Power Off | 0x2CC3 |
| power_off_alt | 0 | 0xEF10 |
| back | Back / Return | 0xD728 |
| cursor_left | cursor left | 0x7807 |
| cursor_right | cursor right | 0xF906 |
| cursor_up | cursor up | 0xBF40 |
| cursor_down | cursor down | 0xBE41 |
| cursor_enter | OK / Select | 0xBB44 |
| settings_menu | menu | 0xBC43 |
| home | home | 0x837C |
| input | Input Toggle | 0xF40B |
| game_menu | channel down | 0xFE01 |
| picture | Picture Mode | 0xB24D |
| focus_manual | aspect ratio | 0x8679 |
| focus_auto | play | 0x4FB0 |
| shortcut_1 | red | 0x8d72 |
| shortcut_2 | green | 0x8e71 |
| shortcut_3 | yellow | 0x9C63 |
| shortcut_4 | blue | 0x9E61 |
| volume_up | volume up | 0xFD02 |
| volume_down | volume down | 0xFC03 |
| mute | mute | 0xF609 |
| token_sniff | 4 | 0xEB14 |
| token_clear | 5 | 0xEA15 |
| token_recall | 6 | 0xE916 |
| BT_start_pair | netflix | 0xA956 |

## JVC-VCR to Xgimi Titan Noir Mapping
| Xgimi Remote Command | JVC VCR Btn | (0x03C2) + Command |
| :--- | :--- | :--- |
| power_on | power (as discrete) | 0xC2D0 |
| power_on_alt | power (alternate) | 0xC2B8 |
| power_off | discrete off | 0xC258 |
| power_off_alt1 | audio monitor | 0xC2E8 |
| power_off_alt2 | 0 | 0xC2CC |
| back | review | 0xC2C3 |
| cursor_up | cursor up | 0xC241 |
| cursor_up_alt | cursor up (harmony) | 0xC298 |
| cursor_down | cursor down | 0xC218 |
| cursor_down_alt | cursor down (harmony) | 0xC261 |
| cursor_left | cursor left | 0xC2A8 |
| cursor_right | cursor right (harmony) | 0xC228 |
| cursor_enter | OK | 0xC23C |
| settings_menu | menu | 0xC2EC |
| settings_menu_alt | memu (alternate) | 0xC207 |
| home | cancel | 0xC26C |
| game_menu | game menu | 0xC230 |
| input | tv/vcr | 0xC2C8 |
| picture | fast forward | 0xC260 |
| focus_manual | 8 | 0xC214 |
| focus_auto | 7 | 0xC2E4 |
| shortcut_1 | prog | 0xC283 |
| shortcut_2 | prog check | 0xC2BC |
| shortcut_3 | SP/EP | 0xC28C |
| shortcut_4 | skip search | 0xC269 |
| volume_up | start down | 0xC213 |
| volume_down | start up | 0xC293 |
| mute | pause | 0xC2B0 |
| token_sniff | 4 | 0xC224 |
| token_clear | 5 | 0xC2A4 |
| token_recall | 6 | 0xC264 |
| BT_start_pair | 1 | 0xC284 |
| BT_clear_pair | 2 | 0xC244 |

## JVC Projector to Xgimi Titan Noir Mapping
The JVC IRmap accomodates several generations of JVC projectors. Buttons on their respective IR remotes have changed over the generations. Some buttons may no longer exist and must be replaced by ones for newer remotes. For this reason, you will see several _alt button mappings for some functions. These let the mapping adapt between older and more current projector remotes.

JVC projectors by default respond to code set A. The can be changed to code set B to avoid control conflicts.
Be aware that you need to use physical buttons on projector (or IP / serial control) to change projector code set if a remote with current codes set is not available.

| Xgimi Command | JVC Projector Btn | (0xCE) + Command |
| :--- | :--- | :--- |
| power_on |  power on | 0xA0 |
| power_off |  power off | 0x60 |
| cursor_up |  up arrow | 0x80 |
| cursor_down |  down arrow | 0x40 |
| cursor_left |  left arrow | 0x6C |
| cursor_right |  right arrow | 0x2C |
| cursor_enter |  enter/ok | 0xF4 |
| settings_menu |  menu | 0x74 |
| back |  exit | 0xC0 |
| home |  hide | 0xB8 |
| game_menu |  dynamic | 0xD6 |
| game_menu_alt |  advanced menu_ | 0xCE |
| input |  input HDMI 1 | 0x0E |
| picture |  input HDMI 2 | 0x8E |
| picture_alt |  picture mode_ | 0x2F |
| focus_manual |  focus - | 0xCC |
| focus_auto |  focus + | 0x8C |
| focus_manual_alt |  color profile_ | 0x11 |
| focus_auto_alt |  gamma settings_ | 0xAF |
| shortcut_1 |  user 1 | 0x36 |
| shortcut_2 |  user 2 | 0xB6 |
| shortcut_3 |  user 3 | 0x76 |
| shortcut_4 |  aspect | 0xEE |
| shortcut_1_alt |  mode 1_ | 0x1B |
| shortcut_2_alt |  mode 2_ | 0x9B |
| shortcut_3_alt |  mode 3_ | 0x5B |
| shortcut_4_alt |  info | 0x2E |
| volume_up |  brightness up | 0x5E |
| volume_down |  brightness down | 0xDE |
| mute |  color temp | 0x6E |
| volume_up_alt |  lens AP_ | 0x04 |
| volume_down_alt |  lens control_ | 0x0C |
| mute_alt |  anamorphic_ | 0xA3 |
| token_sniff |  cinema_ | 0x16 |
| token_sniff_alt |  cinema | 0x96 |
| token_clear |  natural | 0x56 |
| token_recall |  gamma | 0xAE |
| token_recall_alt |  HDR_ | 0xB7 |
| BT_start_pair |  sharp down | 0xFE |
| BT_clear_pair |  sharp up | 0x9A |
| BT_start_pair_alt |  CMD_ | 0x51 |
| BT_clear_pair_alt |  mpc_ | 0x0F |




## Sony Bravia XBR-77A9G TV to Xgimi Titan Noir Mapping
| Xgimi Command | Sony TV Remote | Hex Code |
| :--- | :--- | :--- |
| power_on | power on discrete | 0x000750 |
| power_on_alt | power (toggle) | 0x000A90 |
| power_off | power off discrete | 0x000F50 |
| power_off_alt | 0 | 0x000910 |
| cursor_left | Arrow Left | 0x0002D0 |
| cursor_right | Arrow Right | 0x000CD0 |
| cursor_up | Arrow Up | 0x0002F0 |
| cursor_down | Arrow Down | 0x000AF0 |
| cursor_enter | Arrow Select | 0x000A70 |
| settings_menu | Action Menu | 0x006923 |
| back | Back | 0x0062E9 |
| home | Home | 0x000070 |
| game_menu | Google Play | 0x003123 |
| input | Input | 0x000A50 |
| picture | TV | 0x000250 |
| focus_auto | subtitle | 0x000AE9 |
| focus_manual | audio | 0x000E90 |
| shortcut_1 | yellow | 0x0072E9 |
| shortcut_2 | blue | 0x0012E9 |
| shortcut_3 | red | 0x0052E9 |
| shortcut_4 | green | 0x0032E9 |
| volume_up | volume up | 0x000490 |
| volume_down | volume down | 0x000C90 |
| mute | mute | 0x000290 |
| token_sniff | 4 | 0x000C10 |
| token_clear | 5 | 0x000210 |
| token_recall | 6 | 0x000A10 |
| BT_start_pair | play | 0x002CE9 |
| BT_clear_pair | fast forward | 0x001CE9 |

## Sony VPL-XW600ES Projector to Xgimi Titan Noir Mapping
| Xgimi Command | Sony Projector Remote | Hex Code |
| :--- | :--- | :--- |
| power_on | Power On (discrete) | 0x00003A2A |
| power_off | Power Off (discrete) | 0x00007A2A |
| power_on_alt | Power Toggle (alt discrete On) | 0x0000542A |
| cursor_up | up arrow | 0x0000562A |
| cursor_down | down arrow | 0x0000362A |
| cursor_left | left arrow | 0x0000162A |
| cursor_right | right arrow | 0x0000662A |
| cursor_enter | OK / Enter | 0x00002D2A |
| settings_menu | Menu | 0x00004A2A |
| home | Reset | 0x00006F2A |
| back | Position | 0x00018BE4 |
| game_menu | Game | 0x0006AB54 |
| picture | Photo | 0x000EAB54 |
| input | Input | 0x0000752A |
| focus_manual | Focus | 0x00026B54 |
| focus_auto | Zoom | 0x00046B54 |
| shortcut_1 | Aspect Ratio | 0x00076B54 |
| shortcut_2 | Motion Flow | 0x0000502A |
| shortcut_3 | 3D | 0x000DCB54 |
| shortcut_4 | Color Space | 0x000D2B54 |
| volume_up | CONTRAST | 0x00000C2A |
| volume_down | CONTRAST DOWN | 0x00004C2A |
| mute | Advanced Iris | 0x000FAB54 |
| token_sniff | BRT Cinema | 0x0009AB54 |
| token_clear | BRT TV | 0x0008AB54 |
| token_recall | User | 0x0002AB54 |
| BT_start_pair | BRIGHTNESS DOWN | 0x00007C2A |
| BT_clear_pair | BRIGHTNESS UP | 0x00003C2A |

## Epson Pro Cinema LS1200 Projector to Xgimi Titan Noir Mapping
This IRmap also works with other Epson projectors such as the 5050UB
| Xgimi Command | Epson Projector Btn | (0x5583) + Command |
| :--- | :--- | :--- |
| power_on | Power On | 0x6F90 |
| power_off | Power Off | 0x6E91 |
| cursor_up | up arrow | 0x4FB0 |
| cursor_down | down arrow | 0x4DB2 |
| cursor_left | left arrow | 0x4CB3 |
| cursor_right | right arrow | 0x4EB1 |
| cursor_enter | enter | 0x7A85 |
| settings_menu | menu | 0x659A |
| back | ESC | 0x7B84 |
| home | default | 0xC639 |
| game_menu | color mode | 0x708F |
| input | HDMI link | 0xA956 |
| picture | image enhance | 0x55AA |
| focus_manual | skip back | 0xA25D |
| focus_manual_alt | lens (not in MyHarmony) | 0x728D |
| focus_auto | pause | 0xA45B |
| shortcut_1 | reverse | 0xA55A |
| shortcut_2 | play | 0xA15E |
| shortcut_3 | FF | 0xA35C |
| shortcut_4 | skip forward | 0xA05F |
| shortcut_1_alt | HDMI 1 | 0x8C73 |
| shortcut_2_alt | HDMI 2 | 0x8877 |
| shortcut_3_alt | P-in-P (not in myHarmony) | 0x7D82 |
| shortcut_4_alt | PC (not in myHarmony) | 0x629D |
| volume_up | volume up | 0x6798 |
| volume_down | volume down | 0x6699 |
| mute | mute | 0x52AD |
| token_sniff | frame interp | 0x7C83 |
| token_clear | RGBCMY | 0xC23D |
| token_recall | pattern | 0x6996 |
| BT_start_pair | 3D format | 0xC43B |
| BT_clear_pair | Aspect | 0x758A |

## BenQ W5800 Projector to Xgimi Titan Noir Mapping
| Xgimi Command | BenQ Projector Btn | (0x3000) + Command |
| :--- | :--- | :--- |
| power_on | power on | 0xB04F |
| power_off | power off | 0xB14E |
| cursor_up | up arrow | 0xF40B |
| cursor_down | down arrow | 0xF30C |
| cursor_left | left arrow | 0xF20D |
| cursor_right | right arrow | 0xF10E |
| cursor_enter | ok | 0xEA15 |
| settings_menu | menu | 0xF00F |
| back | back | 0x7A85 |
| home | default | 0x8778 |
| game_menu | guide | 0xEA15 |
| input | source | 0xFB04 |
| picture | picure mode | 0xEF10 |
| focus_manual | aspect | 0xEC13 |
| focus_auto | auto | 0xF708 |
| shortcut_1 | brightness | 0xE916 |
| shortcut_2 | contrast | 0xEE11 |
| shortcut_3 | dynamic iris | 0x837C |
| shortcut_4 | light mode | 0xCF30 |
| volume_up | gamma | 0xA15E |
| volume_down | sharp | 0x817E |
| mute | eco blank | 0xF807 |
| token_sniff | info | 0xF30C |
| token_clear | invert | 0x629D |
| token_recall | 3D | 0x639C |
| BT_start_pair | color temp | 0xA05F |
| BT_clear_pair | color manage | 0xA45B |

## Optoma UHD50X Projector to Xgimi Titan Noir Mapping
| Xgimi Command | Optoma Projector Btn | (0xCD32) + Command |
| :--- | :--- | :--- |
| power_on | power on | 0xFD02 |
| power_off | power off | 0xD12E |
| cursor_left | left arrow | 0xEF10 |
| cursor_right | right arrow | 0xED12 |
| cursor_up | up arrow | 0xEE11 |
| cursor_down | down arrow | 0xEB14 |
| cursor_enter | ok | 0xF00F |
| settings_menu | menu | 0xF10E |
| back | sleep | 0x9C63 |
| input | input HDMI 1 | 0xE916 |
| game_menu | input HDMI 2 | 0xCF30 |
| picture | mode | 0xFA05 |
| focus_manual | aspect | 0x9B64 |
| focus_auto | DB | 0xBB44 |
| shortcut_1 | input VGA 1 | 0xE41B |
| shortcut_2 | input VGA 2 | 0xE11E |
| shortcut_3 | input video | 0xE31C |
| shortcut_4 | input YPbPr | 0xE817 |
| volume_up | 3D | 0x7689 |
| volume_down | keystone | 0xF807 |
| mute | mute | 0xAD52 |
| token_sniff | user 1 | 0xC936 |
| token_clear | user 2 | 0x9A65 |
| token_recall | user 3 | 0x9966 |
| BT_start_pair | brightness | 0xBE41 |
| BT_clear_pair | contrast | 0xBD42 |


# Recent Changes:
*   Added original _Xgimi Titan_ IR mapping. That would be _Titan_ IR to _Titan Noir_ BLuetooth 
*   HOLDING boot button down for 5 seconds toggles display on/off. Setting is retained between boots. 
       Click of boot button still starts sniffing for tokens.

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
