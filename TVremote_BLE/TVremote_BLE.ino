/*************************************************************
  ESP32-C3 Toshiba TV Wi-Fi Controller
  Hardware:
    - ESP32-C3
    - IR Transmitter LED on GPIO 1 (BC547B transistor)
    - Wi-Fi Station connected to TP-LINK_9222
    - Local Web Server & REST API on port 80 (http://tvremote.local)
 *************************************************************/

#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include "TV_Codes.h"

// Wi-Fi Credentials for router
const char* WIFI_SSID = "TP-LINK_9222";
const char* WIFI_PASS = "66442523";

WebServer server(80);
TVBrand selectedBrand = BRAND_TOSHIBA;

void executeTvCommand(String cmdStr) {
    cmdStr.trim();
    cmdStr.toUpperCase();

    if (cmdStr.length() == 0) return;

    Serial.printf("[TV] Command received: %s\n", cmdStr.c_str());

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
    } else if (cmdStr == "EXIT") {
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
    } else if (cmdStr == "1") {
        cmd = CMD_1; found = true;
    } else if (cmdStr == "2") {
        cmd = CMD_2; found = true;
    } else if (cmdStr == "3") {
        cmd = CMD_3; found = true;
    } else if (cmdStr == "4") {
        cmd = CMD_4; found = true;
    } else if (cmdStr == "5") {
        cmd = CMD_5; found = true;
    } else if (cmdStr == "6") {
        cmd = CMD_6; found = true;
    } else if (cmdStr == "7") {
        cmd = CMD_7; found = true;
    } else if (cmdStr == "8") {
        cmd = CMD_8; found = true;
    } else if (cmdStr == "9") {
        cmd = CMD_9; found = true;
    } else if (cmdStr == "0") {
        cmd = CMD_0; found = true;
    }

    if (found) {
        Serial.printf("[IR] Sending '%s' to Toshiba TV (GPIO %d)...\n", cmdStr.c_str(), IR_SEND_PIN);
        sendTVCommand(selectedBrand, cmd);
    } else {
        Serial.printf("[TV] Unknown command: %s\n", cmdStr.c_str());
    }
}

// Mobile-friendly TV Remote Web Interface
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="sv">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <meta name="apple-mobile-web-app-capable" content="yes">
  <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
  <title>Toshiba TV Fjärr</title>
  <style>
    :root {
      --bg: #0b0f19;
      --card: #151d2f;
      --card-inner: #101624;
      --btn-bg: #1f293d;
      --btn-active: #4f46e5;
      --accent: #6366f1;
      --danger: #ef4444;
      --danger-dark: #dc2626;
      --text: #f3f4f6;
      --text-muted: #9ca3af;
      --radius-sm: 10px;
      --radius-md: 14px;
      --radius-lg: 24px;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; -webkit-user-select: none; }
    body {
      background: var(--bg);
      color: var(--text);
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      min-height: 100vh;
      display: flex;
      justify-content: center;
      padding: 16px 12px;
    }
    .container {
      width: 100%;
      max-width: 360px;
      display: flex;
      flex-direction: column;
      gap: 14px;
    }
    .header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      padding: 4px 6px;
    }
    .header h1 {
      font-size: 1.2rem;
      font-weight: 800;
      letter-spacing: -0.02em;
      display: flex;
      align-items: center;
      gap: 8px;
    }
    .badge {
      font-size: 0.75rem;
      background: rgba(16, 185, 129, 0.15);
      color: #34d399;
      padding: 4px 10px;
      border-radius: 20px;
      font-weight: 700;
      border: 1px solid rgba(16, 185, 129, 0.3);
    }
    .remote-body {
      background: var(--card);
      border-radius: var(--radius-lg);
      padding: 20px 16px;
      display: flex;
      flex-direction: column;
      gap: 18px;
      border: 1px solid rgba(255,255,255,0.06);
      box-shadow: 0 12px 32px rgba(0,0,0,0.5);
    }
    .row {
      display: grid;
      gap: 10px;
    }
    .row-3 { grid-template-columns: 1fr 1fr 1fr; }
    .row-2 { grid-template-columns: 1fr 1fr; }
    button {
      background: var(--btn-bg);
      color: var(--text);
      border: none;
      border-radius: var(--radius-md);
      padding: 16px 8px;
      font-size: 1rem;
      font-weight: 700;
      cursor: pointer;
      display: flex;
      align-items: center;
      justify-content: center;
      transition: all 0.08s ease;
      -webkit-tap-highlight-color: transparent;
      box-shadow: 0 3px 8px rgba(0,0,0,0.25);
    }
    button:active {
      transform: scale(0.94);
      background: var(--btn-active);
      color: #fff;
    }
    .btn-power {
      background: var(--danger-dark);
      color: #fff;
      font-size: 1.25rem;
    }
    .btn-power:active { background: #991b1b; }
    .btn-secondary {
      font-size: 0.85rem;
      color: var(--text-muted);
    }
    
    /* D-PAD */
    .dpad-card {
      background: var(--card-inner);
      border-radius: 50%;
      width: 220px;
      height: 220px;
      margin: 4px auto;
      position: relative;
      border: 1px solid rgba(255,255,255,0.05);
      box-shadow: inset 0 2px 8px rgba(0,0,0,0.6);
    }
    .dpad-btn {
      position: absolute;
      background: var(--btn-bg);
      color: var(--text);
      width: 58px;
      height: 58px;
      border-radius: 50%;
      font-size: 1.1rem;
      display: flex;
      align-items: center;
      justify-content: center;
      padding: 0;
    }
    .dpad-up    { top: 8px; left: 81px; }
    .dpad-down  { bottom: 8px; left: 81px; }
    .dpad-left  { top: 81px; left: 8px; }
    .dpad-right { top: 81px; right: 8px; }
    .dpad-center {
      position: absolute;
      top: 75px;
      left: 75px;
      width: 70px;
      height: 70px;
      border-radius: 50%;
      background: var(--accent);
      color: #fff;
      font-size: 1.1rem;
      font-weight: 800;
      box-shadow: 0 4px 14px rgba(99,102,241,0.4);
    }
    .dpad-center:active {
      background: #4338ca;
      transform: scale(0.92);
    }

    /* Keypad Drawer */
    .keypad-grid {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 8px;
      margin-top: 6px;
    }
    .btn-num {
      padding: 12px;
      font-size: 1.1rem;
      font-weight: 700;
      background: rgba(31, 41, 61, 0.6);
    }

    /* Toast */
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
      font-weight: 700;
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
      <h1>📺 Toshiba TV</h1>
      <span class="badge">● Wi-Fi</span>
    </div>

    <div class="remote-body">
      <!-- Power, Input, Mute -->
      <div class="row row-3">
        <button class="btn-power" onclick="sendCmd('POWER')" title="Power">⏻</button>
        <button class="btn-secondary" onclick="sendCmd('INPUT')">KÄLLA</button>
        <button class="btn-secondary" onclick="sendCmd('MUTE')">🔇 MUTE</button>
      </div>

      <!-- Volume & Channel -->
      <div class="row row-2">
        <button onclick="sendCmd('VOL+')">🔊 VOL +</button>
        <button onclick="sendCmd('CH+')">▲ CH +</button>
      </div>
      <div class="row row-2">
        <button onclick="sendCmd('VOL-')">🔉 VOL −</button>
        <button onclick="sendCmd('CH-')">▼ CH −</button>
      </div>

      <!-- D-Pad Navigation -->
      <div class="dpad-card">
        <button class="dpad-btn dpad-up" onclick="sendCmd('UP')">▲</button>
        <button class="dpad-btn dpad-left" onclick="sendCmd('LEFT')">◀</button>
        <button class="dpad-center" onclick="sendCmd('OK')">OK</button>
        <button class="dpad-btn dpad-right" onclick="sendCmd('RIGHT')">▶</button>
        <button class="dpad-btn dpad-down" onclick="sendCmd('DOWN')">▼</button>
      </div>

      <!-- Menu / Back / Exit -->
      <div class="row row-3">
        <button class="btn-secondary" onclick="sendCmd('BACK')">↩ Bakåt</button>
        <button class="btn-secondary" onclick="sendCmd('MENU')">☰ Meny</button>
        <button class="btn-secondary" onclick="sendCmd('EXIT')">✕ Exit</button>
      </div>

      <!-- Numeric Keypad -->
      <div class="keypad-grid">
        <button class="btn-num" onclick="sendCmd('1')">1</button>
        <button class="btn-num" onclick="sendCmd('2')">2</button>
        <button class="btn-num" onclick="sendCmd('3')">3</button>
        <button class="btn-num" onclick="sendCmd('4')">4</button>
        <button class="btn-num" onclick="sendCmd('5')">5</button>
        <button class="btn-num" onclick="sendCmd('6')">6</button>
        <button class="btn-num" onclick="sendCmd('7')">7</button>
        <button class="btn-num" onclick="sendCmd('8')">8</button>
        <button class="btn-num" onclick="sendCmd('9')">9</button>
        <div></div>
        <button class="btn-num" onclick="sendCmd('0')">0</button>
        <div></div>
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
    Serial.println("\n--- ESP32-C3 Toshiba TV Wi-Fi Remote Starting ---");

    // 1. Initialize IR Sender on GPIO 1
    IrSender.begin(IR_SEND_PIN);
    Serial.printf("[IR] Sender initialized on GPIO %d\n", IR_SEND_PIN);

    // 2. Initialize Wi-Fi in Station Mode (No BLE!)
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setTxPower(WIFI_POWER_8_5dBm);

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

    // 3. Web Server Handlers
    server.on("/", HTTP_GET, []() {
        server.send_P(200, "text/html", INDEX_HTML);
    });

    // REST API Endpoint for Browser, SmartThings & Siri: /cmd?c=POWER
    server.on("/cmd", HTTP_GET, []() {
        if (server.hasArg("c")) {
            String c = server.arg("c");
            executeTvCommand(c);
            server.send(200, "text/plain", "OK: " + c);
        } else {
            server.send(400, "text/plain", "Missing c parameter");
        }
    });

    // Status endpoint for SmartThings / Healthchecks
    server.on("/status", HTTP_GET, []() {
        String json = "{";
        json += "\"status\":\"online\",";
        json += "\"device\":\"Toshiba TV Remote\",";
        json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
        json += "\"brand\":\"Toshiba\"";
        json += "}";
        server.send(200, "application/json", json);
    });

    server.begin();
    Serial.println("[HTTP] Local Web Server running on port 80");
}

void loop() {
    // Handle incoming HTTP requests
    server.handleClient();

    // Start mDNS service once connected
    static bool mdnsStarted = false;
    if (WiFi.status() == WL_CONNECTED && !mdnsStarted) {
        if (MDNS.begin("tvremote")) {
            MDNS.addService("http", "tcp", 80);
            mdnsStarted = true;
            Serial.println("[mDNS] Responder started! Access via: http://tvremote.local");
        }
    }

    // Auto-reconnect Wi-Fi every 10 seconds if dropped
    static unsigned long lastCheck = millis();
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - lastCheck > 10000) {
            lastCheck = millis();
            Serial.println("[WiFi] Reconnecting to router...");
            WiFi.disconnect(true, true);
            delay(100);
            WiFi.begin(WIFI_SSID, WIFI_PASS);
        }
    } else {
        lastCheck = millis();
    }

    // Serial CLI commands for testing from PC
    static String serialCmd = "";
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (serialCmd.length() > 0) {
                Serial.printf("[CLI] Executing: %s\n", serialCmd.c_str());
                executeTvCommand(serialCmd);
                serialCmd = "";
            }
        } else {
            serialCmd += c;
        }
    }

    delay(5);
}
