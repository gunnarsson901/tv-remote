/*************************************************************
  ESP32-C3 Direct Bluetooth (BLE) TV Remote Control
  Hardware:
    - ESP32-C3
    - IR Transmitter LED connected to GPIO 1 (BC547B NPN transistor)

  How to use:
  - This sketch allows direct Bluetooth Low Energy (BLE) control
    from your smartphone without requiring Wi-Fi or internet.
  - Install any BLE Terminal app on your phone:
      * Android: "Serial Bluetooth Terminal" or "nRF Connect"
      * iOS: "BLE Terminal HM-10" or "LightBlue"
  - Connect to device: "ESP32C3-TV-Remote"
  - Send commands:
      POWER, VOL+, VOL-, MUTE, CH+, CH-, INPUT, 
      UP, DOWN, LEFT, RIGHT, OK, BACK, MENU
      BRAND SAMSUNG, BRAND LG, BRAND SONY, BRAND GENERIC
 *************************************************************/

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "TV_Codes.h"

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

    // Check for brand switch commands
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
        sendBleResponse(String("ERR: Unknown command '") + cmdStr + "'. Type HELP");
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
    Serial.println("\n--- ESP32-C3 BLE TV Remote Starting ---");

    // Initialize IR sender on GPIO 0
    IrSender.begin(IR_SEND_PIN);
    Serial.printf("[IR] Sender initialized on GPIO %d\n", IR_SEND_PIN);

    // Initialize BLE
    BLEDevice::init("ESP32C3-TV-Remote");
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    // Create BLE Service
    BLEService *pService = pServer->createService(SERVICE_UUID);

    // Create TX Characteristic (Notifications to phone)
    pTxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_TX,
        BLECharacteristic::PROPERTY_NOTIFY
    );
    pTxCharacteristic->addDescriptor(new BLE2902());

    // Create RX Characteristic (Commands from phone)
    BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID_RX,
        BLECharacteristic::PROPERTY_WRITE
    );
    pRxCharacteristic->setCallbacks(new RxCallbacks());

    // Start service
    pService->start();

    // Start advertising
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections
    pAdvertising->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.println("[BLE] BLE advertising started! Ready to pair as 'ESP32C3-TV-Remote'.");
}

void loop() {
    // Handling reconnection advertising
    if (!deviceConnected && oldDeviceConnected) {
        delay(500); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("[BLE] Restarted advertising...");
        oldDeviceConnected = deviceConnected;
    }
    if (deviceConnected && !oldDeviceConnected) {
        oldDeviceConnected = deviceConnected;
        sendBleResponse(String("ESP32-C3 TV Remote Ready! Brand: ") + getBrandName(selectedBrand));
    }
    delay(20);
}
