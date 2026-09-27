/*************************************************************
  ESP32-C3 TV + Cleverio Smart Galaxy Lamp Controller
  Hardware:
    - ESP32-C3
    - IR Transmitter LED on GPIO 1 (BC547B transistor)
    - Built-in Wi-Fi SoftAP for Cleverio Lamp
    - Built-in Bluetooth LE (BLE) for Phone App Control
 *************************************************************/

#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "TV_Codes.h"
#include "Tuya_Lamp.h"

// Wi-Fi SoftAP Settings (Lamp connects here)
const char* AP_SSID = "GalaxyNet";
const char* AP_PASS = "superhemligt123";

// Tuya Device Credentials (retrieve via tinytuya)
const char* TUYA_DEV_ID    = "YOUR_DEV_ID";
const char* TUYA_LOCAL_KEY = "YOUR_LOCAL_KEY";

TuyaLampController lamp(TUYA_DEV_ID, TUYA_LOCAL_KEY, IPAddress(192, 168, 4, 2));

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
        lamp.setPower(true);
        sendBleResponse("Lamp: ON");
        return;
    } else if (cmdStr == "LAMP_OFF" || cmdStr == "LAMP OFF") {
        lamp.setPower(false);
        sendBleResponse("Lamp: OFF");
        return;
    } else if (cmdStr == "LASER_ON" || cmdStr == "LASER ON") {
        lamp.setLaser(true);
        sendBleResponse("Laser: ON");
        return;
    } else if (cmdStr == "LASER_OFF" || cmdStr == "LASER OFF") {
        lamp.setLaser(false);
        sendBleResponse("Laser: OFF");
        return;
    } else if (cmdStr == "LAMP_RED") {
        lamp.setColorHSV(0, 1000, 1000);
        sendBleResponse("Lamp: Red");
        return;
    } else if (cmdStr == "LAMP_GREEN") {
        lamp.setColorHSV(120, 1000, 1000);
        sendBleResponse("Lamp: Green");
        return;
    } else if (cmdStr == "LAMP_BLUE") {
        lamp.setColorHSV(240, 1000, 1000);
        sendBleResponse("Lamp: Blue");
        return;
    } else if (cmdStr == "LAMP_PURPLE") {
        lamp.setColorHSV(280, 1000, 1000);
        sendBleResponse("Lamp: Purple");
        return;
    } else if (cmdStr == "LAMP_WHITE") {
        lamp.setMode("white");
        sendBleResponse("Lamp: White");
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

    // --- TV INFRARED COMMANDS ---
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
    } else if (cmdStr == "BACK" || cmdStr == "RETURN") {
        cmd = CMD_BACK; found = true;
    } else if (cmdStr == "MENU" || cmdStr == "HOME") {
        cmd = CMD_MENU; found = true;
    }

    if (found) {
        sendTVCommand(selectedBrand, cmd);
        sendBleResponse(String("OK: Sent ") + getCommandName(cmd) + " (" + getBrandName(selectedBrand) + ")");
    } else {
        sendBleResponse(String("ERR: Unknown command '") + cmdStr + "'");
    }
}

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("[BLE] Device connected!");
    }

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("[BLE] Device disconnected!");
    }
};

class RxCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
        String rxValue = pCharacteristic->getValue();
        if (rxValue.length() > 0) {
            parseAndExecuteBleCommand(rxValue);
        }
    }
};

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- ESP32-C3 TV + Cleverio Lamp Controller Starting ---");

    // 1. Initialize IR sender on GPIO 1
    IrSender.begin(IR_SEND_PIN);
    Serial.printf("[IR] Sender initialized on GPIO %d\n", IR_SEND_PIN);

    // 2. Start Wi-Fi SoftAP for Cleverio Lamp
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASS);
    Serial.printf("[AP] SoftAP '%s' started at IP %s\n", AP_SSID, WiFi.softAPIP().toString().c_str());

    // 3. Initialize BLE for Phone App
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
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("[BLE] Advertising started as 'ESP32C3-TV-Remote'.");
}

void loop() {
    if (!deviceConnected && oldDeviceConnected) {
        delay(500);
        pServer->startAdvertising();
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        sendBleResponse(String("TV Remote Ready! (Brand: ") + getBrandName(selectedBrand) + ")");
    }
    delay(20);
}
