#ifndef TUYA_LAMP_H
#define TUYA_LAMP_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <mbedtls/aes.h>

class TuyaLampController {
private:
    String devId;
    String localKey;
    IPAddress lampIp;
    bool ipDiscovered;
    WiFiUDP udp;

    uint32_t calculateCRC32(const uint8_t *data, size_t length) {
        uint32_t crc = 0xFFFFFFFF;
        for (size_t i = 0; i < length; i++) {
            crc ^= data[i];
            for (uint8_t j = 0; j < 8; j++) {
                if (crc & 1) crc = (crc >> 1) ^ 0xEDB88320;
                else crc >>= 1;
            }
        }
        return ~crc;
    }

public:
    TuyaLampController(const char* id, const char* key, IPAddress defaultIp = IPAddress(0, 0, 0, 0))
        : devId(id), localKey(key), lampIp(defaultIp), ipDiscovered(false) {}

    void beginUdp() {
        udp.begin(6666);
    }

    void setLampIp(IPAddress ip) {
        lampIp = ip;
        ipDiscovered = true;
    }

    IPAddress getLampIp() {
        return lampIp;
    }

    bool isDiscovered() {
        return ipDiscovered;
    }

    void update() {
        if (WiFi.status() != WL_CONNECTED) return;
        
        int packetSize = udp.parsePacket();
        if (packetSize > 0) {
            char buf[512];
            int len = udp.read(buf, sizeof(buf) - 1);
            if (len > 0) {
                buf[len] = '\0';
                String msg = String(buf);
                if (msg.indexOf(devId) >= 0) {
                    IPAddress newIp = udp.remoteIP();
                    if (!ipDiscovered || lampIp != newIp) {
                        lampIp = newIp;
                        ipDiscovered = true;
                        Serial.printf("[Tuya] Discovered Cleverio lamp at IP: %s\n", lampIp.toString().c_str());
                    }
                }
            }
        }
    }

    bool sendCommand(const String &dpsJson) {
        if (devId.length() == 0 || localKey.length() == 0) {
            Serial.println("[Tuya] Error: DevID or LocalKey not configured!");
            return false;
        }

        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[Tuya] Error: WiFi is not connected!");
            return false;
        }

        // If IP is not discovered yet, check UDP packet or probe subnet
        if (!ipDiscovered || lampIp == IPAddress(0, 0, 0, 0)) {
            update();
            if (!ipDiscovered || lampIp == IPAddress(0, 0, 0, 0)) {
                Serial.println("[Tuya] Lamp IP unknown. Probing local subnet for port 6668...");
                IPAddress myIp = WiFi.localIP();
                for (int i = 2; i <= 20; i++) {
                    IPAddress testIp(myIp[0], myIp[1], myIp[2], i);
                    if (testIp == myIp) continue;
                    WiFiClient testClient;
                    testClient.setTimeout(50);
                    if (testClient.connect(testIp, 6668)) {
                        testClient.stop();
                        lampIp = testIp;
                        ipDiscovered = true;
                        Serial.printf("[Tuya] Found open Tuya port 6668 at %s!\n", lampIp.toString().c_str());
                        break;
                    }
                }
            }
        }

        if (!ipDiscovered || lampIp == IPAddress(0, 0, 0, 0)) {
            Serial.println("[Tuya] Cannot send: Lamp IP unknown on current network!");
            return false;
        }

        Serial.printf("[Tuya] Sending to %s: %s\n", lampIp.toString().c_str(), dpsJson.c_str());

        WiFiClient client;
        client.setTimeout(1000);
        if (!client.connect(lampIp, 6668)) {
            Serial.printf("[Tuya] Connection to lamp at %s failed!\n", lampIp.toString().c_str());
            ipDiscovered = false; // Reset to rediscover IP
            return false;
        }

        // Build full Tuya 3.3 payload JSON
        String fullJson = "{\"devId\":\"" + devId + "\",\"uid\":\"" + devId + "\",\"t\":" + String(millis()) + ",\"dps\":" + dpsJson + "}";

        // AES-128-ECB PKCS7 Padding
        int plainLen = fullJson.length();
        int padLen = 16 - (plainLen % 16);
        int encLen = plainLen + padLen;
        uint8_t padded[encLen];
        memcpy(padded, fullJson.c_str(), plainLen);
        memset(padded + plainLen, padLen, padLen);

        uint8_t encrypted[encLen];
        mbedtls_aes_context aes;
        mbedtls_aes_init(&aes);
        mbedtls_aes_setkey_enc(&aes, (const unsigned char*)localKey.c_str(), 128);
        for (int i = 0; i < encLen; i += 16) {
            mbedtls_aes_crypt_ecb(&aes, MBEDTLS_AES_ENCRYPT, padded + i, encrypted + i);
        }
        mbedtls_aes_free(&aes);

        // Protocol 3.3 format: "3.3" (3 bytes) + 12 zero padding bytes + encrypted data
        int payloadLen = 3 + 12 + encLen;
        uint8_t payload[payloadLen];
        memcpy(payload, "3.3", 3);
        memset(payload + 3, 0, 12);
        memcpy(payload + 15, encrypted, encLen);

        // Frame length = payloadLen + 8 (CRC 4B + Suffix 4B)
        uint32_t frameLen = payloadLen + 8;

        // Header: prefix (0x000055AA), seq (0), cmd (0x07), length
        uint8_t header[16];
        header[0] = 0x00; header[1] = 0x00; header[2] = 0x55; header[3] = 0xAA;
        header[4] = 0x00; header[5] = 0x00; header[6] = 0x00; header[7] = 0x00;
        header[8] = 0x00; header[9] = 0x00; header[10] = 0x00; header[11] = 0x07;
        header[12] = (frameLen >> 24) & 0xFF;
        header[13] = (frameLen >> 16) & 0xFF;
        header[14] = (frameLen >> 8) & 0xFF;
        header[15] = frameLen & 0xFF;

        int totalPreCrc = 16 + payloadLen;
        uint8_t packet[totalPreCrc + 8];
        memcpy(packet, header, 16);
        memcpy(packet + 16, payload, payloadLen);

        // CRC32
        uint32_t crc = calculateCRC32(packet, totalPreCrc);
        packet[totalPreCrc]     = (crc >> 24) & 0xFF;
        packet[totalPreCrc + 1] = (crc >> 16) & 0xFF;
        packet[totalPreCrc + 2] = (crc >> 8) & 0xFF;
        packet[totalPreCrc + 3] = crc & 0xFF;

        // Suffix: 0x0000AA55
        packet[totalPreCrc + 4] = 0x00;
        packet[totalPreCrc + 5] = 0x00;
        packet[totalPreCrc + 6] = 0xAA;
        packet[totalPreCrc + 7] = 0x55;

        client.write(packet, sizeof(packet));
        delay(50);
        client.stop();

        Serial.println("[Tuya] Command successfully sent over LAN!");
        return true;
    }

    // Helper functions for Cleverio Galaxy Lamp DPs
    bool setPower(bool on) {
        return sendCommand("{\"20\":" + String(on ? "true" : "false") + "}");
    }

    bool setLaser(bool on) {
        return sendCommand("{\"102\":" + String(on ? "true" : "false") + "}");
    }

    bool setNebula(bool on) {
        return sendCommand("{\"103\":" + String(on ? "true" : "false") + "}");
    }

    bool setSpeed(int speed) { // 0 - 1000
        return sendCommand("{\"101\":" + String(speed) + "}");
    }

    bool setMode(const char* mode) { // "colour", "white", "scene", "music"
        return sendCommand("{\"21\":\"" + String(mode) + "\"}");
    }

    bool setColorHSV(uint16_t h, uint16_t s, uint16_t v) { // h: 0-360, s: 0-1000, v: 0-1000
        char buf[16];
        snprintf(buf, sizeof(buf), "%04x%04x%04x", h, s, v);
        return sendCommand("{\"21\":\"colour\",\"24\":\"" + String(buf) + "\"}");
    }
};

#endif // TUYA_LAMP_H
