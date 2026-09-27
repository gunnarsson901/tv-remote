# ESP32-C3 TV Remote Controller

This project turns an **ESP32-C3** into a smart TV remote control using an Infrared (IR) LED connected to **GPIO 0**.

---

## ⚠️ Important Note: Blynk IoT vs. Bluetooth

* **Blynk IoT (v2.0)**: The modern Blynk platform controls hardware over **Wi-Fi / Blynk Cloud** (the legacy Bluetooth widget from Blynk 1.0 was retired).
* **ESP32-C3**: Has both built-in **Wi-Fi** and **Bluetooth 5.0 (BLE)**.

To provide you with the best experience, two complete, pre-tested sketches are included:

1. [**`TVremote_Blynk/`**](file:///home/felix/Projects/TVremote/TVremote_Blynk/TVremote_Blynk.ino): Controls your TV using the official **Blynk IoT Mobile App** (via Wi-Fi).
2. [**`TVremote_BLE/`**](file:///home/felix/Projects/TVremote/TVremote_BLE/TVremote_BLE.ino): Controls your TV directly over **Bluetooth Low Energy (BLE)** without needing Wi-Fi or a router (works with any BLE terminal or custom app).

Both sketches use the unified [**`TV_Codes.h`**](file:///home/felix/Projects/TVremote/TVremote_Blynk/TV_Codes.h) library supporting **Samsung**, **LG**, **Sony**, and **Generic NEC** TVs.

---

## Hardware Setup & Wiring (GPIO 1 + BC547B)

### BC547B Transistor Driver Circuit (Pinout: Flat face facing you = C - B - E)

```
                  +3.3V or 5V (VIN)
                        │
                       [ ] 33Ω - 47Ω Resistor (1/4W)
                        │
                      ───── (Anode: Long leg)
                      \   / IR LED (940nm)
                       \ /
                      ───── (Cathode: Short leg / Flat notch)
                        │
                        └───► Collector (Pin 1 - Left)
                              │
  ESP32-C3 GPIO 1 ───[ 600Ω ]─┤ Base (Pin 2 - Center)    [BC547B NPN]
                              │
                        ┌───► Emitter (Pin 3 - Right)
                        │
                       GND
```

### Option B: Direct Connection (Short Range ~1m Testing)

* **ESP32-C3 GPIO 0** ➔ **100Ω Resistor** ➔ **IR LED Anode (+)**
* **IR LED Cathode (-)** ➔ **ESP32-C3 GND**

> **Note on ESP32-C3 GPIO 0**: Unlike the original ESP32 where GPIO 0 was a strapping pin, on the ESP32-C3 strapping pins are GPIO 2, 8, and 9. GPIO 0 is a standard RTC-capable GPIO and safe for general use.

---

## 1. Blynk IoT Setup (`TVremote_Blynk`)

### Step 1: Create a Template on Blynk.Cloud
1. Log in to [Blynk.Console](https://blynk.cloud/).
2. Navigate to **Templates** ➔ **+ New Template**:
   - **Name**: `ESP32C3 TV Remote`
   - **Hardware**: `ESP32`
   - **Connection Type**: `WiFi`
3. Copy the 3 template header lines displayed in the dashboard:
   ```cpp
   #define BLYNK_TEMPLATE_ID   "TMPLxxxxxx"
   #define BLYNK_TEMPLATE_NAME "ESP32C3 TV Remote"
   #define BLYNK_AUTH_TOKEN    "YourAuthToken"
   ```

### Step 2: Configure Datastreams
Under your template, go to the **Datastreams** tab and add the following:

| Virtual Pin | Name | Data Type | Min | Max | Mode / Details |
|:---:|:---|:---:|:---:|:---:|:---|
| **V0** | Power | Integer | 0 | 1 | Push button |
| **V1** | Volume Up | Integer | 0 | 1 | Push button |
| **V2** | Volume Down | Integer | 0 | 1 | Push button |
| **V3** | Mute | Integer | 0 | 1 | Push button |
| **V4** | Channel Up | Integer | 0 | 1 | Push button |
| **V5** | Channel Down | Integer | 0 | 1 | Push button |
| **V6** | Input / Source | Integer | 0 | 1 | Push button |
| **V7** | Nav Up | Integer | 0 | 1 | Push button |
| **V8** | Nav Down | Integer | 0 | 1 | Push button |
| **V9** | Nav Left | Integer | 0 | 1 | Push button |
| **V10** | Nav Right | Integer | 0 | 1 | Push button |
| **V11** | OK / Select | Integer | 0 | 1 | Push button |
| **V12** | Back / Return | Integer | 0 | 1 | Push button |
| **V13** | Menu / Home | Integer | 0 | 1 | Push button |
| **V14** | Brand Selector | Integer | 0 | 3 | Menu: 0=Samsung, 1=LG, 2=Sony, 3=Generic |
| **V20** | Activity LED | Integer | 0 | 255 | LED widget (pulses on IR transmit) |
| **V21** | Terminal | String | - | - | Terminal widget |

### Step 3: Configure the Mobile App Dashboard
1. Open the **Blynk IoT** app on your phone (iOS / Android).
2. Open your Device / Template dashboard and enter Edit Mode (wrench icon).
3. Drag and place **Button Widgets** for Power, Vol+/-, Ch+/-, etc.
   > **CRITICAL**: In each Button's settings, set **Mode** to **PUSH** (not Switch).
4. (Optional) Add an **LED widget** assigned to `V20` and a **Terminal widget** assigned to `V21` for real-time feedback.

### Step 4: Flash the Code
1. Open [**`TVremote_Blynk/TVremote_Blynk.ino`**](file:///home/felix/Projects/TVremote/TVremote_Blynk/TVremote_Blynk.ino).
2. Enter your `BLYNK_TEMPLATE_ID`, `BLYNK_AUTH_TOKEN`, `ssid`, and `pass`.
3. Select board `ESP32C3 Dev Module` and flash:
   ```bash
   arduino-cli compile --fqbn esp32:esp32:esp32c3 TVremote_Blynk --upload -p /dev/ttyUSB0
   ```

---

## 2. Direct Bluetooth (BLE) Setup (`TVremote_BLE`)

If you want an offline remote without connecting to a Wi-Fi network:

1. Open and flash [**`TVremote_BLE/TVremote_BLE.ino`**](file:///home/felix/Projects/TVremote/TVremote_BLE/TVremote_BLE.ino).
2. Install any standard BLE Terminal app on your phone:
   - **Android**: *Serial Bluetooth Terminal* (by Kai Morich) or *nRF Connect*.
   - **iOS**: *BLE Terminal HM-10* or *LightBlue*.
3. Scan and connect to **`ESP32C3-TV-Remote`**.
4. In the app, configure shortcut buttons or type commands:

| Command | Action |
|:---|:---|
| `POWER` | Toggle TV power |
| `VOL+` / `VOL-` | Volume up / down |
| `MUTE` | Mute toggle |
| `CH+` / `CH-` | Channel up / down |
| `INPUT` | Change input source |
| `UP` / `DOWN` / `LEFT` / `RIGHT` / `OK` | Directional navigation |
| `BACK` / `MENU` | Return / Menu |
| `BRAND SAMSUNG` | Switch active protocol to Samsung |
| `BRAND LG` | Switch active protocol to LG |
| `BRAND SONY` | Switch active protocol to Sony |
| `BRAND GENERIC` | Switch active protocol to Generic NEC |

---

## 3. Supported TV Brands & Protocol Customization

Supported brands and commands are centralized in [**`TV_Codes.h`**](file:///home/felix/Projects/TVremote/TVremote_Blynk/TV_Codes.h):

* **Samsung**: 32-bit Samsung protocol (`sendSamsung`)
* **LG**: 32-bit LG/NEC protocol (`sendLG`)
* **Sony**: 12-bit SIRC protocol (`sendSony`)
* **Generic**: Standard NEC protocol used by many affordable TVs (TCL, Insignia, Hisense, etc.)

To add custom codes or support a different brand, simply edit `sendTVCommand()` in [**`TV_Codes.h`**](file:///home/felix/Projects/TVremote/TVremote_Blynk/TV_Codes.h).
