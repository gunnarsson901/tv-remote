/*************************************************************
  ESP32-C3 TV + Cleverio Smart Galaxy Lamp Controller
  Hardware:
    - ESP32-C3
    - IR Transmitter LED on GPIO 1 (BC547B transistor)
    - Wi-Fi Station (Multi-AP fallback) for Cleverio Lamp
    - Built-in Bluetooth LE (BLE) for Phone App Control
    - Built-in Web Server on port 80 (for Safari / all browsers)
 *************************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "TV_Codes.h"
#include "Tuya_Lamp.h"

// Wi-Fi Multi-AP Fallback list
// Maryam's iPhone is primary (where Cleverio lamp is paired)
// supdude is secondary fallback
struct KnownAP {
    const char* ssid;
    const char* pass;
};

const KnownAP knownAPs[] = {
    {"Maryams iphone", "heiiiii!"},
    {"supdude", "dudesup?"}
};
const int numKnownAPs = sizeof(knownAPs) / sizeof(knownAPs[0]);
int currentApIndex = 0;

// Tuya Device Credentials (Cleverio Smart Galaxy Lamp)
const char* TUYA_DEV_ID    = "bf8fb27deb7cd94966pntq";
const char* TUYA_LOCAL_KEY = "<q:~=&dB[i.bo=^F";

TuyaLampController lamp(TUYA_DEV_ID, TUYA_LOCAL_KEY);

WebServer server(80);

// Standard Nordic UART Service (NUS) UUIDs
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

TVBrand selectedBrand = BRAND_TOSHIBA;

void sendBleResponse(const String &msg) {
    if (deviceConnected && pTxCharacteristic != NULL) {
        String out = msg + "\r\n";
        pTxCharacteristic->setValue((uint8_t*)out.c_str(), out.length());
        pTxCharacteristic->notify();
    }
}

void parseAndExecuteBleCommand(String cmdStr) {
    cmdStr.trim();
    cmdStr.toUpperCase();

    if (cmdStr.length() == 0) return;

    Serial.printf("[CMD RX] Command: %s\n", cmdStr.c_str());

    // --- DIAGNOSTICS & WIFI SWITCHING COMMANDS ---
    if (cmdStr == "STATUS" || cmdStr == "WIFI_STATUS") {
        String st = "WiFi: ";
        if (WiFi.status() == WL_CONNECTED) {
            st += WiFi.SSID() + " (" + WiFi.localIP().toString() + ")";
            if (lamp.isDiscovered()) {
                st += " | Lamp: " + lamp.getLampIp().toString();
            } else {
                st += " | Lamp: Searching...";
            }
        } else {
            st += "Disconnected (trying " + String(knownAPs[currentApIndex].ssid) + ")";
        }
        sendBleResponse(st);
        return;
    } else if (cmdStr == "WIFI_IPHONE" || cmdStr == "WIFI_HOTSPOT") {
        currentApIndex = 0;
        WiFi.disconnect(true, true);
        delay(200);
        WiFi.begin(knownAPs[0].ssid, knownAPs[0].pass);
        sendBleResponse("Connecting to Maryams iphone...");
        return;
    } else if (cmdStr == "WIFI_SUPDUDE" || cmdStr == "WIFI_HOME") {
        currentApIndex = 1;
        WiFi.disconnect(true, true);
        delay(200);
        WiFi.begin(knownAPs[1].ssid, knownAPs[1].pass);
        sendBleResponse("Connecting to supdude...");
        return;
    }

    // --- CLEVERIO GALAXY LAMP COMMANDS ---
    if (cmdStr == "LAMP_ON" || cmdStr == "LAMP ON") {
        if (WiFi.status() != WL_CONNECTED) {
            sendBleResponse("Error: WiFi Disconnected (Turn on Hotspot)");
        } else if (!lamp.isDiscovered()) {
            sendBleResponse("Lamp: Searching IP (wait ~10s)...");
        } else {
            bool ok = lamp.setPower(true);
            sendBleResponse(ok ? "Lamp: ON" : "Lamp: Failed");
        }
        return;
    } else if (cmdStr == "LAMP_OFF" || cmdStr == "LAMP OFF") {
        if (WiFi.status() != WL_CONNECTED) {
            sendBleResponse("Error: WiFi Disconnected");
        } else if (!lamp.isDiscovered()) {
            sendBleResponse("Lamp: Searching IP (wait ~10s)...");
        } else {
            bool ok = lamp.setPower(false);
            sendBleResponse(ok ? "Lamp: OFF" : "Lamp: Failed");
        }
        return;
    } else if (cmdStr == "LASER_ON" || cmdStr == "LASER ON") {
        if (WiFi.status() != WL_CONNECTED) {
            sendBleResponse("Error: WiFi Disconnected");
        } else if (!lamp.isDiscovered()) {
            sendBleResponse("Lamp: Searching IP (wait ~10s)...");
        } else {
            bool ok = lamp.setLaser(true);
            sendBleResponse(ok ? "Laser: ON" : "Laser: Failed");
        }
        return;
    } else if (cmdStr == "LASER_OFF" || cmdStr == "LASER OFF") {
        if (WiFi.status() != WL_CONNECTED) {
            sendBleResponse("Error: WiFi Disconnected");
        } else if (!lamp.isDiscovered()) {
            sendBleResponse("Lamp: Searching IP (wait ~10s)...");
        } else {
            bool ok = lamp.setLaser(false);
            sendBleResponse(ok ? "Laser: OFF" : "Laser: Failed");
        }
        return;
    } else if (cmdStr == "LAMP_RED") {
        if (WiFi.status() != WL_CONNECTED) {
            sendBleResponse("Error: WiFi Disconnected");
        } else if (!lamp.isDiscovered()) {
            sendBleResponse("Lamp: Searching IP...");
        } else {
            lamp.setColorHSV(0, 1000, 1000);
            sendBleResponse("Lamp: Red");
        }
        return;
    } else if (cmdStr == "LAMP_GREEN") {
        if (WiFi.status() != WL_CONNECTED) {
            sendBleResponse("Error: WiFi Disconnected");
        } else if (!lamp.isDiscovered()) {
            sendBleResponse("Lamp: Searching IP...");
        } else {
            lamp.setColorHSV(120, 1000, 1000);
            sendBleResponse("Lamp: Green");
        }
        return;
    } else if (cmdStr == "LAMP_BLUE") {
        if (WiFi.status() != WL_CONNECTED) {
            sendBleResponse("Error: WiFi Disconnected");
        } else if (!lamp.isDiscovered()) {
            sendBleResponse("Lamp: Searching IP...");
        } else {
            lamp.setColorHSV(240, 1000, 1000);
            sendBleResponse("Lamp: Blue");
        }
        return;
    } else if (cmdStr == "LAMP_PURPLE") {
        if (WiFi.status() != WL_CONNECTED) {
            sendBleResponse("Error: WiFi Disconnected");
        } else if (!lamp.isDiscovered()) {
            sendBleResponse("Lamp: Searching IP...");
        } else {
            lamp.setColorHSV(280, 1000, 1000);
            sendBleResponse("Lamp: Purple");
        }
        return;
    } else if (cmdStr == "LAMP_WHITE") {
        if (WiFi.status() != WL_CONNECTED) {
            sendBleResponse("Error: WiFi Disconnected");
        } else if (!lamp.isDiscovered()) {
            sendBleResponse("Lamp: Searching IP...");
        } else {
            lamp.setMode("white");
            sendBleResponse("Lamp: White");
        }
        return;
    }

    // --- TV BRAND SELECTION ---
    if (cmdStr.startsWith("BRAND")) {
        if (cmdStr.indexOf("TOSHIBA") >= 0) {
            selectedBrand = BRAND_TOSHIBA;
        } else if (cmdStr.indexOf("SAMSUNG") >= 0) {
            selectedBrand = BRAND_SAMSUNG;
        } else if (cmdStr.indexOf("LG") >= 0) {
            selectedBrand = BRAND_LG;
        } else if (cmdStr.indexOf("SONY") >= 0) {
            selectedBrand = BRAND_SONY;
        } else if (cmdStr.indexOf("GENERIC") >= 0 || cmdStr.indexOf("NEC") >= 0) {
            selectedBrand = BRAND_GENERIC;
        }
        sendBleResponse(String("Brand switched to: ") + getBrandName(selectedBrand));
        return;
    }

    // --- TV INFRARED COMMANDS (Always 100% independent of Wi-Fi!) ---
    RemoteCommand cmd;
    bool found = false;

    if (cmdStr == "POWER" || cmdStr == "PWR") {
        cmd = CMD_POWER; found = true;
    } else if (cmdStr == "VOL+" || cmdStr == "VOLUP" || cmdStr == "VOLUME_UP") {
        cmd = CMD_VOL_UP; found = true;
    } else if (cmdStr == "VOL-" || cmdStr == "VOLDOWN" || cmdStr == "VOLUME_DOWN") {
        cmd = CMD_VOL_DOWN; found = true;
    } else if (cmdStr == "MUTE") {
        cmd = CMD_MUTE; found = true;
    } else if (cmdStr == "CH+" || cmdStr == "CHUP" || cmdStr == "CHANNEL_UP") {
        cmd = CMD_CH_UP; found = true;
    } else if (cmdStr == "CH-" || cmdStr == "CHDOWN" || cmdStr == "CHANNEL_DOWN") {
        cmd = CMD_CH_DOWN; found = true;
    } else if (cmdStr == "INPUT" || cmdStr == "SOURCE") {
        cmd = CMD_INPUT; found = true;
    } else if (cmdStr == "MENU" || cmdStr == "HOME") {
        cmd = CMD_MENU; found = true;
    } else if (cmdStr == "BACK" || cmdStr == "RETURN") {
        cmd = CMD_BACK; found = true;
    } else if (cmdStr == "UP") {
        cmd = CMD_UP; found = true;
    } else if (cmdStr == "DOWN") {
        cmd = CMD_DOWN; found = true;
    } else if (cmdStr == "LEFT") {
        cmd = CMD_LEFT; found = true;
    } else if (cmdStr == "RIGHT") {
        cmd = CMD_RIGHT; found = true;
    } else if (cmdStr == "OK" || cmdStr == "ENTER") {
        cmd = CMD_OK; found = true;
    }

    if (found) {
        Serial.printf("[IR] Sending TV command '%s' via IR pin %d...\n", cmdStr.c_str(), IR_SEND_PIN);
        sendTVCommand(selectedBrand, cmd);
        sendBleResponse(String("Sent: ") + cmdStr);
    } else {
        Serial.printf("[BLE] Unknown command: %s\n", cmdStr.c_str());
        sendBleResponse(String("Unknown: ") + cmdStr);
    }
}

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("[BLE] Phone connected!");
    }
    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("[BLE] Phone disconnected!");
    }
};

class RxCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String rxValue = pCharacteristic->getValue().c_str();
        if (rxValue.length() > 0) {
            parseAndExecuteBleCommand(rxValue);
        }
    }
};

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- ESP32-C3 TV + Cleverio Lamp Controller Starting ---");

    // 1. Initialize IR sender on GPIO 1 (Instant TV control)
    IrSender.begin(IR_SEND_PIN);
    Serial.printf("[IR] Sender initialized on GPIO %d\n", IR_SEND_PIN);

    // 2. Initialize Wi-Fi in Station mode
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);
    WiFi.setAutoReconnect(false);
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            Serial.printf("[WiFi] Connected to AP '%s'!\n", WiFi.SSID().c_str());
        } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            Serial.printf("[WiFi] Got IP: %s\n", WiFi.localIP().toString().c_str());
        } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            Serial.printf("[WiFi] Disconnected. Reason: %d\n", info.wifi_sta_disconnected.reason);
        }
    });

    // 3. Connect Wi-Fi first before starting BLE radio to avoid radio collision during 4-way handshake
    Serial.printf("[WiFi] Connecting to primary AP: '%s'...\n", knownAPs[0].ssid);
    WiFi.begin(knownAPs[0].ssid, knownAPs[0].pass);

    unsigned long wifiWait = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - wifiWait < 4000) {
        delay(100);
    }

    lamp.beginUdp();

    // Sync NTP time for authentic Tuya timestamps
    configTime(0, 0, "pool.ntp.org", "time.google.com");

    // 4. Initialize Local Web Server on port 80 (For direct Safari / iPhone control!)
    server.on("/cmd", []() {
        if (server.hasArg("c")) {
            String c = server.arg("c");
            parseAndExecuteBleCommand(c);
            server.send(200, "text/plain", "OK: " + c);
        } else {
            server.send(400, "text/plain", "Missing c");
        }
    });
    server.on("/", []() {
        String html = "<!DOCTYPE html><html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>TV & Cleverio Remote</title><style>body{background:#0f172a;color:#fff;font-family:system-ui,-apple-system,sans-serif;text-align:center;padding:12px;margin:0}button{background:#1e293b;color:#fff;border:none;border-radius:12px;padding:16px;font-size:16px;font-weight:700;cursor:pointer;-webkit-tap-highlight-color:transparent;transition:all 0.1s}button:active{background:#4f46e5;transform:scale(0.96)}.grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;max-width:340px;margin:0 auto}.btn-red{background:#dc2626}.btn-green{background:#16a34a}.btn-purple{background:#7c3aed}.btn-blue{background:#2563eb}h2{font-size:1.15rem;margin:12px 0;letter-spacing:-0.02em}h3{font-size:1rem;color:#a78bfa;margin:22px 0 10px}</style></head><body><h2>📺 Toshiba TV Fjärrkontroll</h2><div class='grid'><button style='background:#ef4444' onclick=\"fetch('/cmd?c=POWER')\">⏻ TV Power</button><button onclick=\"fetch('/cmd?c=MUTE')\">🔇 Mute</button><button onclick=\"fetch('/cmd?c=VOL+')\">VOL +</button><button onclick=\"fetch('/cmd?c=VOL-')\">VOL -</button><button onclick=\"fetch('/cmd?c=CH+')\">CH ▲</button><button onclick=\"fetch('/cmd?c=CH-')\">CH ▼</button></div><h3>🌌 Cleverio Smart Galaxy Lampa</h3><div class='grid'><button class='btn-purple' onclick=\"fetch('/cmd?c=LAMP_ON')\">💡 Lampa På</button><button style='background:#334155' onclick=\"fetch('/cmd?c=LAMP_OFF')\">Lampa Av</button><button class='btn-green' onclick=\"fetch('/cmd?c=LASER_ON')\">✨ Laser På</button><button style='background:#334155' onclick=\"fetch('/cmd?c=LASER_OFF')\">Laser Av</button><button class='btn-red' onclick=\"fetch('/cmd?c=LAMP_RED')\">Röd</button><button class='btn-green' onclick=\"fetch('/cmd?c=LAMP_GREEN')\">Grön</button><button class='btn-blue' onclick=\"fetch('/cmd?c=LAMP_BLUE')\">Blå</button><button class='btn-purple' onclick=\"fetch('/cmd?c=LAMP_PURPLE')\">Lila</button></div></body></html>";
        server.send(200, "text/html", html);
    });
    server.begin();
    Serial.println("[HTTP] Local Web Server started on port 80.");

    // 5. Initialize Bluetooth LE for Phone App
    BLEDevice::init("ESP32C3-TV-Remote");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    pRxCharacteristic->setCallbacks(new RxCallbacks());

    pService->start();

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinInterval(320); // 200ms
    pAdvertising->setMaxInterval(640); // 400ms
    BLEDevice::startAdvertising();

    Serial.println("[Setup] Ready! IR on GPIO 1, BLE advertising, HTTP Web Server on port 80.");
}

void loop() {
    // Handle HTTP Web Server requests
    server.handleClient();

    // BLE Re-advertising handling
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        sendBleResponse(String("TV Remote Ready! (Brand: ") + getBrandName(selectedBrand) + ")");
    }

    // Tuya background UDP discovery and non-blocking IP probing
    lamp.update();

    // Background Non-blocking Wi-Fi Reconnect (20 seconds per attempt)
    static unsigned long lastWifiAttempt = millis();
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - lastWifiAttempt > 20000) {
            lastWifiAttempt = millis();
            currentApIndex = (currentApIndex + 1) % numKnownAPs;
            const KnownAP &ap = knownAPs[currentApIndex];
            Serial.printf("[WiFi] Switching to AP: '%s'...\n", ap.ssid);
            
            bool wasAdv = !deviceConnected;
            if (wasAdv) pServer->getAdvertising()->stop();

            WiFi.disconnect(true, true);
            delay(200);
            WiFi.begin(ap.ssid, ap.pass);

            // Give Wi-Fi 3 seconds dedicated radio time for 4-way handshake
            unsigned long hs = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - hs < 3000) {
                delay(100);
            }

            if (wasAdv && !deviceConnected) pServer->getAdvertising()->start();
        }
    } else {
        lastWifiAttempt = millis(); // Reset timer when connected
    }

    // Serial CLI commands for testing directly from PC
    static String serialCmd = "";
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialCmd.length() > 0) {
                Serial.printf("[CLI] Executing: %s\n", serialCmd.c_str());
                parseAndExecuteBleCommand(serialCmd);
                serialCmd = "";
            }
        } else {
            serialCmd += c;
        }
    }

    // Periodic status heartbeat
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 5000) {
        lastLog = millis();
        Serial.printf("[STATUS] WiFi: %s (%s) | BLE: %s | Lamp IP: %s\n",
            WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "Disconnected",
            WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "No IP",
            deviceConnected ? "Connected" : "Advertising",
            lamp.isDiscovered() ? lamp.getLampIp().toString().c_str() : "Searching");
    }

    delay(10);
}
