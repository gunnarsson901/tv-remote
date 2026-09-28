#ifndef TUYA_LAMP_H
#define TUYA_LAMP_H

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#include <mbedtls/aes.h>

class TuyaLampController {
private:
    String devId;
    String localKey;
    IPAddress lampIp;
    bool ipDiscovered;
    WiFiUDP udp6666;
    WiFiUDP udp6667;
    unsigned long lastProbeTime;
    int probeHostIndex;
    uint32_t seqno;

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

    uint32_t getUnixTimestamp() {
        time_t now;
        time(&now);
        if (now > 1700000000) {
            return (uint32_t)now;
        }
        // Fallback realistic 10-digit epoch timestamp if NTP not yet synced
        return 1727525000 + (millis() / 1000);
    }

    void probeNextIp() {
        if (WiFi.status() != WL_CONNECTED || ipDiscovered) return;
        
        unsigned long now = millis();
        if (now - lastProbeTime < 800) return;
        lastProbeTime = now;

        IPAddress myIp = WiFi.localIP();
        if (myIp == IPAddress(0, 0, 0, 0)) return;

        if (probeHostIndex > 15) probeHostIndex = 2;

        IPAddress testIp(myIp[0], myIp[1], myIp[2], probeHostIndex);
        probeHostIndex++;

        if (testIp == myIp) return;

        WiFiClient testClient;
        if (testClient.connect(testIp, 6668, 150)) {
            testClient.stop();
            lampIp = testIp;
            ipDiscovered = true;
            Serial.printf("[Tuya] Discovered Cleverio lamp at IP: %s (via port 6668 probe)\n", lampIp.toString().c_str());
        }
    }

public:
    TuyaLampController(const char* id, const char* key, IPAddress defaultIp = IPAddress(0, 0, 0, 0))
        : devId(id), localKey(key), lampIp(defaultIp), ipDiscovered(false), lastProbeTime(0), probeHostIndex(2), seqno(1) {}

    void beginUdp() {
        udp6666.begin(6666);
        udp6667.begin(6667);
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
        
        // 1. Check UDP 6666
        int packetSize66 = udp6666.parsePacket();
        if (packetSize66 > 0) {
            char buf[512];
            int len = udp6666.read(buf, sizeof(buf) - 1);
            if (len > 0) {
                buf[len] = '\0';
                String msg = String(buf);
                if (msg.indexOf(devId) >= 0) {
                    IPAddress newIp = udp6666.remoteIP();
                    if (!ipDiscovered || lampIp != newIp) {
                        lampIp = newIp;
                        ipDiscovered = true;
                        Serial.printf("[Tuya] Discovered Cleverio lamp at IP: %s (via UDP 6666)\n", lampIp.toString().c_str());
                    }
                }
            }
        }

        // 2. Check UDP 6667
        int packetSize67 = udp6667.parsePacket();
        if (packetSize67 > 0) {
            char buf[512];
            int len = udp6667.read(buf, sizeof(buf) - 1);
            if (len > 0) {
                IPAddress newIp = udp6667.remoteIP();
                if (!ipDiscovered || lampIp != newIp) {
                    lampIp = newIp;
                    ipDiscovered = true;
                    Serial.printf("[Tuya] Discovered Cleverio lamp at IP: %s (via UDP 6667)\n", lampIp.toString().c_str());
                }
            }
        }

        // 3. Background IP probe
        if (!ipDiscovered) {
            probeNextIp();
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

        if (!ipDiscovered || lampIp == IPAddress(0, 0, 0, 0)) {
            Serial.println("[Tuya] Cannot send: Lamp IP unknown on current network! Probing in progress...");
            return false;
        }

        Serial.printf("[Tuya] Connecting to %s:6668 to send %s...\n", lampIp.toString().c_str(), dpsJson.c_str());

        WiFiClient client;
        if (!client.connect(lampIp, 6668, 1000)) {
            Serial.printf("[Tuya] Connection to lamp at %s failed!\n", lampIp.toString().c_str());
            ipDiscovered = false;
            return false;
        }

        // Build full Tuya 3.3 payload JSON with string epoch timestamp exactly like tinytuya
        uint32_t t_stamp = getUnixTimestamp();
        String fullJson = "{\"devId\":\"" + devId + "\",\"uid\":\"" + devId + "\",\"t\":\"" + String(t_stamp) + "\",\"dps\":" + dpsJson + "}";

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

        seqno++;

        // Header: prefix (0x000055AA), seq (seqno), cmd (0x07), length
        uint8_t header[16];
        header[0] = 0x00; header[1] = 0x00; header[2] = 0x55; header[3] = 0xAA;
        header[4] = (seqno >> 24) & 0xFF;
        header[5] = (seqno >> 16) & 0xFF;
        header[6] = (seqno >> 8) & 0xFF;
        header[7] = seqno & 0xFF;
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
        client.flush();

        // Wait for and read the lamp's ACK response so connection is cleanly completed
        unsigned long waitStart = millis();
        bool gotResponse = false;
        while (client.connected() && millis() - waitStart < 800) {
            if (client.available()) {
                uint8_t respBuf[128];
                int r = client.read(respBuf, sizeof(respBuf));
                Serial.printf("[Tuya] Lamp acknowledged! Received %d bytes (prefix 0x%02X%02X)\n", r, respBuf[2], respBuf[3]);
                gotResponse = true;
                break;
            }
            delay(10);
        }

        if (!gotResponse) {
            Serial.println("[Tuya] Notice: No response packet before timeout (command was sent)");
        }

        client.stop();
        return true;
    }

    // Helper functions for Cleverio Galaxy Lamp DPs
    bool setPower(bool on) {
        if (!on) {
            // Turn off main LED (20), laser (102), and nebula (103)
            return sendCommand("{\"20\":false,\"102\":false,\"103\":false}");
        } else {
            // Turn on main LED (20), laser (102), and nebula (103)
            return sendCommand("{\"20\":true,\"102\":true,\"103\":true}");
        }
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
        return sendCommand("{\"20\":true,\"21\":\"colour\",\"24\":\"" + String(buf) + "\"}");
    }
};

#endif // TUYA_LAMP_H
