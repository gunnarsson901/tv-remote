/*************************************************************
  ESP32-C3 TV Remote Control via Blynk IoT App
  Hardware: 
    - ESP32-C3
    - IR Transmitter LED connected to GPIO 0
  
  Notes on Blynk IoT vs Bluetooth:
  - Blynk IoT (v2.0) communicates with the ESP32-C3 via Wi-Fi 
    and the Blynk Cloud.
  - Buttons on your Blynk IoT mobile app trigger virtual pins (V0-V14),
    which send IR codes out of GPIO 0 to control your TV.
 *************************************************************/

// 1. Fill in your Blynk Template credentials from Blynk.Console
#define BLYNK_TEMPLATE_ID   "TMPLxxxxxx"
#define BLYNK_TEMPLATE_NAME "ESP32C3 TV Remote"
#define BLYNK_AUTH_TOKEN    "YourAuthTokenHere"

// Comment this out to disable Serial prints and save memory
#define BLYNK_PRINT Serial

#include <WiFi.h>
#include <WiFiClient.h>
#include <BlynkSimpleEsp32.h>
#include "TV_Codes.h"

// 2. Wi-Fi credentials
char ssid[] = "YOUR_WIFI_SSID";
char pass[] = "YOUR_WIFI_PASSWORD";

// Default TV Brand (can also be switched dynamically via Blynk V14)
// Options: BRAND_SAMSUNG, BRAND_LG, BRAND_SONY, BRAND_GENERIC
TVBrand selectedBrand = BRAND_SAMSUNG;

// Virtual pin for Terminal & Activity LED
WidgetTerminal blynkTerminal(V21);
WidgetLED activityLed(V20);
BlynkTimer timer;

// Helper function to handle button triggers
void handleRemoteButton(RemoteCommand cmd) {
    // Pulse the activity LED in Blynk app
    activityLed.on();
    timer.setTimeout(150L, []() {
        activityLed.off();
    });

    // Send the infrared signal
    sendTVCommand(selectedBrand, cmd);

    // Feedback to Blynk terminal
    String msg = String("[IR] Sent: ") + getCommandName(cmd) + " (" + getBrandName(selectedBrand) + ")";
    blynkTerminal.println(msg);
    blynkTerminal.flush();
}

// ==================== BLYNK VIRTUAL PIN HANDLERS ====================
// Note: Configure buttons in the Blynk app with Mode = "PUSH" (1 on press, 0 on release)

BLYNK_WRITE(V0) { // POWER
    if (param.asInt() == 1) handleRemoteButton(CMD_POWER);
}

BLYNK_WRITE(V1) { // VOLUME UP
    if (param.asInt() == 1) handleRemoteButton(CMD_VOL_UP);
}

BLYNK_WRITE(V2) { // VOLUME DOWN
    if (param.asInt() == 1) handleRemoteButton(CMD_VOL_DOWN);
}

BLYNK_WRITE(V3) { // MUTE
    if (param.asInt() == 1) handleRemoteButton(CMD_MUTE);
}

BLYNK_WRITE(V4) { // CHANNEL UP
    if (param.asInt() == 1) handleRemoteButton(CMD_CH_UP);
}

BLYNK_WRITE(V5) { // CHANNEL DOWN
    if (param.asInt() == 1) handleRemoteButton(CMD_CH_DOWN);
}

BLYNK_WRITE(V6) { // INPUT / SOURCE
    if (param.asInt() == 1) handleRemoteButton(CMD_INPUT);
}

BLYNK_WRITE(V7) { // D-PAD UP
    if (param.asInt() == 1) handleRemoteButton(CMD_UP);
}

BLYNK_WRITE(V8) { // D-PAD DOWN
    if (param.asInt() == 1) handleRemoteButton(CMD_DOWN);
}

BLYNK_WRITE(V9) { // D-PAD LEFT
    if (param.asInt() == 1) handleRemoteButton(CMD_LEFT);
}

BLYNK_WRITE(V10) { // D-PAD RIGHT
    if (param.asInt() == 1) handleRemoteButton(CMD_RIGHT);
}

BLYNK_WRITE(V11) { // OK / ENTER
    if (param.asInt() == 1) handleRemoteButton(CMD_OK);
}

BLYNK_WRITE(V12) { // BACK / RETURN
    if (param.asInt() == 1) handleRemoteButton(CMD_BACK);
}

BLYNK_WRITE(V13) { // MENU / HOME
    if (param.asInt() == 1) handleRemoteButton(CMD_MENU);
}

BLYNK_WRITE(V14) { // TV BRAND SELECTOR (Menu Widget: 0=Samsung, 1=LG, 2=Sony, 3=Generic NEC)
    int brandIdx = param.asInt();
    if (brandIdx >= 0 && brandIdx <= 3) {
        selectedBrand = static_cast<TVBrand>(brandIdx);
        Serial.printf("[Blynk] Selected TV brand changed to: %s\n", getBrandName(selectedBrand));
        blynkTerminal.printf("[Config] TV Brand set to: %s\n", getBrandName(selectedBrand));
        blynkTerminal.flush();
    }
}

// Sync settings when device connects to Blynk Cloud
BLYNK_CONNECTED() {
    Serial.println("[Blynk] Connected to Blynk Cloud!");
    blynkTerminal.println("=== ESP32-C3 TV Remote Online ===");
    blynkTerminal.printf("Active Brand: %s\n", getBrandName(selectedBrand));
    blynkTerminal.flush();
    Blynk.syncVirtual(V14); // Sync TV brand menu selection
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- ESP32-C3 TV Remote Starting ---");

    // Initialize IR transmitter on GPIO 0
    IrSender.begin(IR_SEND_PIN);
    Serial.printf("[IR] Sender initialized on GPIO %d\n", IR_SEND_PIN);

    // Connect to Wi-Fi and Blynk
    Serial.println("[Blynk] Connecting to Wi-Fi & Blynk...");
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
}

void loop() {
    Blynk.run();
    timer.run();
}
