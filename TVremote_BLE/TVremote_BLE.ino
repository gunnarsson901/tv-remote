/*************************************************************
  ESP32-C3 TV + Cleverio Smart Galaxy Lamp Controller
  Hardware:
    - ESP32-C3
    - IR Transmitter LED on GPIO 1 (BC547B transistor)
    - Wi-Fi Station (Multi-AP fallback) for Cleverio Lamp
    - Built-in Bluetooth LE (BLE) for Phone App Control
 *************************************************************/

#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "TV_Codes.h"
#include "Tuya_Lamp.h"

// Wi-Fi Multi-AP Fallback list
struct KnownAP {
    const char* ssid;
    const char* pass;
};

const KnownAP knownAPs[] = {
    {"Maryams iphone", "heiiiii!"},
    {"supdude", "dudesup?"},
    {"GalaxyNet", "superhemligt123"}
};
const int numKnownAPs = sizeof(knownAPs) / sizeof(knownAPs[0]);
int currentApIndex = 0;

// Tuya Device Credentials (Cleverio Smart Galaxy Lamp)
const char* TUYA_DEV_ID    = "bf8fb27deb7cd94966pntq";
const char* TUYA_LOCAL_KEY = "<q:~=&dB[i.bo=^F";

TuyaLampController lamp(TUYA_DEV_ID, TUYA_LOCAL_KEY);

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

    Serial.printf("[BLE RX] Command: %s\n", cmdStr.c_str());

    // --- CLEVERIO GALAXY LAMP COMMANDS ---
    if (cmdStr == "LAMP_ON" || cmdStr == "LAMP ON") {
        if (WiFi.status() != WL_CONNECTED) {
            sendBleResponse("Error: WiFi Disconnected (Turn on Hotspot)");
        } else {
            bool ok = lamp.setPower(true);
            sendBleResponse(ok ? "Lamp: ON" : "Lamp: Failed");
        }
        return;
    } else if (cmdStr == "LAMP_OFF" || cmdStr == "LAMP OFF") {
        if (WiFi.status() != WL_CONNECTED) {
            sendBleResponse("Error: WiFi Disconnected");
        } else {
            bool ok = lamp.setPower(false);
            sendBleResponse(ok ? "Lamp: OFF" : "Lamp: Failed");
        }
        return;
    } else if (cmdStr == "LASER_ON" || cmdStr == "LASER ON") {
        if (WiFi.status() != WL_CONNECTED) {
            sendBleResponse("Error: WiFi Disconnected");
        } else {
            bool ok = lamp.setLaser(true);
            sendBleResponse(ok ? "Laser: ON" : "Laser: Failed");
        }
        return;
    } else if (cmdStr == "LASER_OFF" || cmdStr == "LASER OFF") {
        if (WiFi.status() != WL_CONNECTED) {
            sendBleResponse("Error: WiFi Disconnected");
        } else {
            bool ok = lamp.setLaser(false);
            sendBleResponse(ok ? "Laser: OFF" : "Laser: Failed");
        }
        return;
    } else if (cmdStr == "LAMP_RED") {
        if (WiFi.status() == WL_CONNECTED) {
            lamp.setColorHSV(0, 1000, 1000);
            sendBleResponse("Lamp: Red");
        } else {
            sendBleResponse("Error: WiFi Disconnected");
        }
        return;
    } else if (cmdStr == "LAMP_GREEN") {
        if (WiFi.status() == WL_CONNECTED) {
            lamp.setColorHSV(120, 1000, 1000);
            sendBleResponse("Lamp: Green");
        } else {
            sendBleResponse("Error: WiFi Disconnected");
        }
        return;
    } else if (cmdStr == "LAMP_BLUE") {
        if (WiFi.status() == WL_CONNECTED) {
            lamp.setColorHSV(240, 1000, 1000);
            sendBleResponse("Lamp: Blue");
        } else {
            sendBleResponse("Error: WiFi Disconnected");
        }
        return;
    } else if (cmdStr == "LAMP_PURPLE") {
        if (WiFi.status() == WL_CONNECTED) {
            lamp.setColorHSV(280, 1000, 1000);
            sendBleResponse("Lamp: Purple");
        } else {
            sendBleResponse("Error: WiFi Disconnected");
        }
        return;
    } else if (cmdStr == "LAMP_WHITE") {
        if (WiFi.status() == WL_CONNECTED) {
            lamp.setMode("white");
            sendBleResponse("Lamp: White");
        } else {
            sendBleResponse("Error: WiFi Disconnected");
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

    // 2. Initialize Bluetooth LE for Phone App (Instant connection)
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
    pAdvertising->setMinInterval(160); // 100ms
    pAdvertising->setMaxInterval(320); // 200ms
    BLEDevice::startAdvertising();

    Serial.println("[BLE] Advertising started as 'ESP32C3-TV-Remote'.");

    // 3. Initialize Wi-Fi in Station mode (Multi-AP background fallback)
    WiFi.mode(WIFI_STA);
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            Serial.printf("[WiFi] Connected to AP '%s'!\n", WiFi.SSID().c_str());
        } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            Serial.printf("[WiFi] Got IP: %s\n", WiFi.localIP().toString().c_str());
        } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            Serial.println("[WiFi] Disconnected from AP.");
        }
    });

    lamp.beginUdp();
    Serial.println("[WiFi] Multi-AP client mode initialized. Ready!");
}

void loop() {
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

    // Tuya background UDP discovery check
    lamp.update();

    // Background Non-blocking Wi-Fi Reconnect (Every 7 seconds, never blocks BLE or IR)
    static unsigned long lastWifiAttempt = 0;
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - lastWifiAttempt > 7000) {
            lastWifiAttempt = millis();
            const KnownAP &ap = knownAPs[currentApIndex];
            Serial.printf("[WiFi] Trying connection to '%s'...\n", ap.ssid);
            WiFi.disconnect();
            WiFi.begin(ap.ssid, ap.pass);
            currentApIndex = (currentApIndex + 1) % numKnownAPs;
        }
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

    delay(20);
}
