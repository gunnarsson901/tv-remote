/*************************************************************
  ESP32-C3 TV + Cleverio Smart Galaxy Lamp Wi-Fi Controller
  Hardware:
    - ESP32-C3
    - IR Transmitter LED on GPIO 1 (BC547B transistor)
    - Wi-Fi Station connected to TP-LINK_9222
    - Local Web Server & REST API on port 80 (http://tvremote.local)
    - Tuya LAN 3.3 protocol for Cleverio Lamp
 *************************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <time.h>
#include "TV_Codes.h"
#include "Tuya_Lamp.h"

// Wi-Fi Credentials for dedicated router
const char* WIFI_SSID = "TP-LINK_9222";
const char* WIFI_PASS = "66442523";

// Secondary fallback networks
struct KnownAP {
    const char* ssid;
    const char* pass;
};
const KnownAP fallbackAPs[] = {
    {"TP-LINK_9222", "66442523"},
    {"Maryams iphone", "heiiiii!"},
    {"supdude", "dudesup?"}
};
const int numFallbackAPs = sizeof(fallbackAPs) / sizeof(fallbackAPs[0]);
int currentApIndex = 0;

// Tuya Device Credentials (Cleverio Smart Galaxy Lamp)
const char* TUYA_DEV_ID    = "bf8fb27deb7cd94966pntq";
const char* TUYA_LOCAL_KEY = "<q:~=&dB[i.bo=^F";

TuyaLampController lamp(TUYA_DEV_ID, TUYA_LOCAL_KEY);
WebServer server(80);

TVBrand selectedBrand = BRAND_TOSHIBA;

void executeCommand(String cmdStr) {
    cmdStr.trim();
    cmdStr.toUpperCase();

    if (cmdStr.length() == 0) return;

    Serial.printf("[CMD] Executing: %s\n", cmdStr.c_str());

    // --- CLEVERIO GALAXY LAMP COMMANDS ---
    if (cmdStr == "LAMP_ON" || cmdStr == "LAMP ON") {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[Tuya] Error: WiFi Disconnected");
        } else {
            lamp.setPower(true);
        }
        return;
    } else if (cmdStr == "LAMP_OFF" || cmdStr == "LAMP OFF") {
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[Tuya] Error: WiFi Disconnected");
        } else {
            lamp.setPower(false);
        }
        return;
    } else if (cmdStr == "LASER_ON" || cmdStr == "LASER ON") {
        if (WiFi.status() == WL_CONNECTED) lamp.setLaser(true);
        return;
    } else if (cmdStr == "LASER_OFF" || cmdStr == "LASER OFF") {
        if (WiFi.status() == WL_CONNECTED) lamp.setLaser(false);
        return;
    } else if (cmdStr == "LAMP_RED") {
        if (WiFi.status() == WL_CONNECTED) lamp.setColorHSV(0, 1000, 1000);
        return;
    } else if (cmdStr == "LAMP_GREEN") {
        if (WiFi.status() == WL_CONNECTED) lamp.setColorHSV(120, 1000, 1000);
        return;
    } else if (cmdStr == "LAMP_BLUE") {
        if (WiFi.status() == WL_CONNECTED) lamp.setColorHSV(240, 1000, 1000);
        return;
    } else if (cmdStr == "LAMP_PURPLE") {
        if (WiFi.status() == WL_CONNECTED) lamp.setColorHSV(280, 1000, 1000);
        return;
    }

    // --- TV INFRARED COMMANDS (Toshiba NEC Protocol on GPIO 1) ---
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
        Serial.printf("[IR] Sending '%s' via GPIO %d...\n", cmdStr.c_str(), IR_SEND_PIN);
        sendTVCommand(selectedBrand, cmd);
    } else {
        Serial.printf("[CMD] Unknown command: %s\n", cmdStr.c_str());
    }
}

// Mobile-friendly HTML Web Remote Page
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="sv">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <title>TV & Lamp Fjärrkontroll</title>
  <style>
    :root {
      --bg: #0b0f19;
      --card: #151d2f;
      --btn-bg: #1f293d;
      --btn-active: #374151;
      --accent: #6366f1;
      --accent-glow: rgba(99, 102, 241, 0.4);
      --danger: #ef4444;
      --success: #10b981;
      --text: #f3f4f6;
      --text-muted: #9ca3af;
      --radius-sm: 8px;
      --radius-md: 14px;
      --radius-lg: 20px;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; -webkit-user-select: none; }
    body {
      background: var(--bg);
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      min-height: 100vh;
      display: flex;
      justify-content: center;
      padding: 14px 10px;
    }
    .container {
      width: 100%;
      max-width: 380px;
      display: flex;
      flex-direction: column;
      gap: 12px;
    }
    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 4px;
    }
    .header h1 { font-size: 1.15rem; font-weight: 700; }
    .status-badge {
      font-size: 0.75rem;
      background: rgba(16, 185, 129, 0.15);
      color: #34d399;
      padding: 4px 10px;
      border-radius: 20px;
      font-weight: 600;
      border: 1px solid rgba(16, 185, 129, 0.3);
    }
    .card {
      background: var(--card);
      border-radius: var(--radius-lg);
      padding: 16px;
      display: flex;
      flex-direction: column;
      gap: 12px;
      border: 1px solid rgba(255,255,255,0.06);
      box-shadow: 0 8px 24px rgba(0,0,0,0.4);
    }
    .card-title {
      font-size: 0.85rem;
      font-weight: 700;
      color: var(--text-muted);
      letter-spacing: 0.05em;
      text-transform: uppercase;
    }
    .grid-3 { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px; }
    .grid-2 { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; }
    .grid-4 { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; }
    button {
      background: var(--btn-bg);
      color: var(--text);
      border: none;
      border-radius: var(--radius-md);
      padding: 14px 8px;
      font-size: 0.95rem;
      font-weight: 700;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 6px;
      transition: all 0.08s ease;
      -webkit-tap-highlight-color: transparent;
      box-shadow: 0 2px 6px rgba(0,0,0,0.2);
    }
    button:active {
      transform: scale(0.95);
      background: var(--accent);
      color: #fff;
    }
    .btn-power { background: #dc2626; color: #fff; }
    .btn-power:active { background: #b91c1c; }
    .btn-laser { background: #059669; color: #fff; }
    .btn-lamp { background: #7c3aed; color: #fff; }
    .toast {
      position: fixed;
      bottom: 24px;
      left: 50%;
      transform: translateX(-50%) translateY(40px);
      background: #1e293b;
      color: #38bdf8;
      padding: 8px 18px;
      border-radius: 20px;
      font-size: 0.82rem;
      font-weight: 600;
      border: 1px solid rgba(56, 189, 248, 0.3);
      opacity: 0;
      transition: all 0.2s ease;
      pointer-events: none;
      z-index: 100;
    }
    .toast.show {
      opacity: 1;
      transform: translateX(-50%) translateY(0);
    }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>📺 TV & Lampa</h1>
      <span class="status-badge">● Wi-Fi Ansluten</span>
    </div>

    <!-- Toshiba TV Remote -->
    <div class="card">
      <div class="card-title">Toshiba TV</div>
      <div class="grid-3">
        <button class="btn-power" onclick="sendCmd('POWER')">⏻ Power</button>
        <button onclick="sendCmd('INPUT')">Källa</button>
        <button onclick="sendCmd('MUTE')">🔇 Mute</button>
      </div>
      <div class="grid-2">
        <button onclick="sendCmd('VOL+')">🔊 Vol +</button>
        <button onclick="sendCmd('CH+')">CH ▲</button>
      </div>
      <div class="grid-2">
        <button onclick="sendCmd('VOL-')">🔉 Vol −</button>
        <button onclick="sendCmd('CH-')">CH ▼</button>
      </div>
      <div class="grid-3">
        <button onclick="sendCmd('BACK')">↩ Bakåt</button>
        <button onclick="sendCmd('MENU')">☰ Meny</button>
        <button onclick="sendCmd('OK')">OK</button>
      </div>
    </div>

    <!-- Cleverio Smart Galaxy Lamp -->
    <div class="card">
      <div class="card-title" style="color: #c084fc;">🌌 Cleverio Galaxy Lampa</div>
      <div class="grid-2">
        <button class="btn-lamp" onclick="sendCmd('LAMP_ON')">💡 Lampa På</button>
        <button onclick="sendCmd('LAMP_OFF')">Lampa Av</button>
      </div>
      <div class="grid-2">
        <button class="btn-laser" onclick="sendCmd('LASER_ON')">✨ Laser På</button>
        <button onclick="sendCmd('LASER_OFF')">Laser Av</button>
      </div>
      <div class="grid-4">
        <button style="background:#dc2626;" onclick="sendCmd('LAMP_RED')">Röd</button>
        <button style="background:#16a34a;" onclick="sendCmd('LAMP_GREEN')">Grön</button>
        <button style="background:#2563eb;" onclick="sendCmd('LAMP_BLUE')">Blå</button>
        <button style="background:#9333ea;" onclick="sendCmd('LAMP_PURPLE')">Lila</button>
      </div>
    </div>
  </div>

  <div id="toast" class="toast">Kommando skickat</div>

  <script>
    let toastTimeout = null;
    function showToast(msg) {
      const t = document.getElementById('toast');
      t.innerText = msg;
      t.classList.add('show');
      clearTimeout(toastTimeout);
      toastTimeout = setTimeout(() => t.classList.remove('show'), 1200);
    }

    function sendCmd(c) {
      if (navigator.vibrate) navigator.vibrate(20);
      showToast(c);
      fetch('/cmd?c=' + encodeURIComponent(c)).catch(() => {});
    }
  </script>
</body>
</html>
)rawliteral";

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n--- ESP32-C3 TV + Cleverio Wi-Fi Controller Starting ---");

    // 1. Initialize IR Sender on GPIO 1 (Toshiba TV)
    IrSender.begin(IR_SEND_PIN);
    Serial.printf("[IR] Sender initialized on GPIO %d\n", IR_SEND_PIN);

    // 2. Initialize Wi-Fi in Station Mode (No BLE!)
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_8_5dBm); // Essential for ESP32-C3 radio stability and 4-way handshake

    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
            Serial.printf("[WiFi] Connected to AP '%s'!\n", WiFi.SSID().c_str());
        } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            Serial.printf("[WiFi] Got IP: %s\n", WiFi.localIP().toString().c_str());
        } else if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            Serial.printf("[WiFi] Disconnected. Reason: %d\n", info.wifi_sta_disconnected.reason);
        }
    });

    Serial.printf("[WiFi] Connecting to router '%s'...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASS);

    // 3. Initialize Tuya UDP Listener for Cleverio Lamp
    lamp.beginUdp();

    // 4. Start Local Web Server and REST API
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", INDEX_HTML);
    });

    // REST API Endpoint: /cmd?c=POWER, /cmd?c=LAMP_ON, etc. (Can be called by SmartThings/Siri)
    server.on("/cmd", HTTP_GET, []() {
        if (server.hasArg("c")) {
            String c = server.arg("c");
            executeCommand(c);
            server.send(200, "text/plain", "OK: " + c);
        } else {
            server.send(400, "text/plain", "Missing c parameter");
        }
    });

    // Device Status Endpoint: /status
    server.on("/status", HTTP_GET, []() {
        String json = "{";
        json += "\"wifi\":\"" + String(WiFi.status() == WL_CONNECTED ? "connected" : "disconnected") + "\",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"ssid\":\"" + WiFi.SSID() + "\",";
        json += "\"lamp_ip\":\"" + (lamp.isDiscovered() ? lamp.getLampIp().toString() : "searching") + "\"";
        json += "}";
        server.send(200, "application/json", json);
    });

    server.begin();
    Serial.println("[HTTP] Local Web Server running on port 80");
}

void loop() {
    // Handle incoming HTTP requests
    server.handleClient();

    // Handle mDNS service
    static bool mdnsStarted = false;
    if (WiFi.status() == WL_CONNECTED && !mdnsStarted) {
        if (MDNS.begin("tvremote")) {
            MDNS.addService("http", "tcp", 80);
            mdnsStarted = true;
            Serial.println("[mDNS] Responder started! Access via: http://tvremote.local");
        }
        // Start NTP time sync once connected
        configTime(0, 0, "pool.ntp.org", "time.google.com");
    }

    // Tuya background UDP discovery and non-blocking IP probing
    lamp.update();

    // Background Wi-Fi Reconnect (Every 15 seconds if disconnected)
    static unsigned long lastWifiAttempt = millis();
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - lastWifiAttempt > 15000) {
            lastWifiAttempt = millis();
            currentApIndex = (currentApIndex + 1) % numFallbackAPs;
            const KnownAP &ap = fallbackAPs[currentApIndex];
            Serial.printf("[WiFi] Reconnecting to AP: '%s'...\n", ap.ssid);
            WiFi.disconnect(true, true);
            delay(100);
            WiFi.begin(ap.ssid, ap.pass);
        }
    } else {
        lastWifiAttempt = millis();
    }

    // Serial CLI commands for testing directly from PC
    static String serialCmd = "";
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialCmd.length() > 0) {
                Serial.printf("[CLI] Executing: %s\n", serialCmd.c_str());
                executeCommand(serialCmd);
                serialCmd = "";
            }
        } else {
            serialCmd += c;
        }
    }

    // Status Heartbeat
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 5000) {
        lastLog = millis();
        Serial.printf("[STATUS] WiFi: %s (%s) | Lamp IP: %s\n",
            WiFi.status() == WL_CONNECTED ? WiFi.SSID().c_str() : "Disconnected",
            WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString().c_str() : "No IP",
            lamp.isDiscovered() ? lamp.getLampIp().toString().c_str() : "Searching");
    }

    delay(5);
}
