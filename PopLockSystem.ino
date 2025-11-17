/*
 * ESP32 Pop Lock System - Arduino IDE Version
 *
 * SETUP INSTRUCTIONS:
 * 1. Install ESP32 board support in Arduino IDE
 * 2. Install "PubSubClient" library (Sketch → Include Library → Manage Libraries)
 * 3. Connect ESP32 to computer via USB
 * 4. Select Board: ESP32 Dev Module (or your specific ESP32 board)
 * 5. Select Port: The COM port where ESP32 is connected
 * 6. Edit WiFi credentials below
 * 7. Upload!
 *
 * USAGE:
 * 1. Open Serial Monitor (115200 baud)
 * 2. Note the IP address displayed
 * 3. On iPad, open browser and go to that IP address
 * 4. Enter code and unlock!
 */

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>

// ============================================================================
// CONFIGURATION - EDIT THESE VALUES
// ============================================================================

// WiFi Settings
const char* WIFI_SSID = "AlchemyGuest";          // ← Change to your WiFi name
const char* WIFI_PASS = "VoodooVacation5601";    // ← Change to your WiFi password

// MQTT Settings (Optional - for remote monitoring)
const char* MQTT_HOST = "10.1.10.115";           // ← Your MQTT broker IP
const uint16_t MQTT_PORT = 1883;

// Lock Configuration
const int NUM_LOCKS = 6;                         // Number of locks
const int LOCK_TIME_MS = 2000;                   // How long to activate (2 seconds)

// GPIO Pins - Change if using different pins
const int LOCK_PINS[NUM_LOCKS] = {
  25,  // Lock 1
  26,  // Lock 2
  27,  // Lock 3
  32,  // Lock 4
  33,  // Lock 5
  14   // Lock 6
};

// Access Codes - Edit these to your codes
struct CodeMap {
  const char* code;
  int lockNum;        // 0-5 for locks 1-6, -1 for all locks
  const char* name;
};

const CodeMap CODES[] = {
  {"1234", 0, "Red Box"},
  {"5678", 1, "Blue Box"},
  {"9012", 2, "Green Box"},
  {"3456", 3, "Yellow Box"},
  {"7890", 4, "Purple Box"},
  {"2468", 5, "Orange Box"},
  {"MASTER", -1, "All Locks"},  // Opens all locks
};

const int NUM_CODES = sizeof(CODES) / sizeof(CodeMap);

// ============================================================================
// GLOBALS
// ============================================================================

WebServer server(80);
WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

struct LockState {
  bool active;
  unsigned long startTime;
};

LockState locks[NUM_LOCKS];

// ============================================================================
// HTML WEB PAGE
// ============================================================================

const char HTML[] PROGMEM = R"(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lockbox Access</title>
  <style>
    * { margin: 0; padding: 0; box-sizing: border-box; }
    body {
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Arial, sans-serif;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      min-height: 100vh;
      display: flex;
      justify-content: center;
      align-items: center;
      padding: 20px;
    }
    .container {
      background: white;
      border-radius: 20px;
      box-shadow: 0 20px 60px rgba(0,0,0,0.3);
      padding: 40px;
      max-width: 500px;
      width: 100%;
      text-align: center;
    }
    h1 { color: #333; margin-bottom: 10px; font-size: 32px; }
    .subtitle { color: #666; margin-bottom: 30px; font-size: 16px; }
    .code-input {
      width: 100%;
      padding: 20px;
      font-size: 24px;
      text-align: center;
      border: 3px solid #e0e0e0;
      border-radius: 12px;
      margin-bottom: 20px;
      letter-spacing: 4px;
      font-weight: bold;
    }
    .code-input:focus {
      outline: none;
      border-color: #667eea;
      box-shadow: 0 0 0 4px rgba(102, 126, 234, 0.1);
    }
    .keypad {
      display: grid;
      grid-template-columns: repeat(3, 1fr);
      gap: 10px;
      margin-bottom: 20px;
    }
    .key {
      padding: 20px;
      font-size: 24px;
      font-weight: bold;
      background: #f8f9fa;
      border: 2px solid #e0e0e0;
      border-radius: 12px;
      cursor: pointer;
    }
    .key:active { transform: scale(0.95); background: #dee2e6; }
    .submit-btn {
      width: 100%;
      padding: 18px;
      font-size: 20px;
      font-weight: bold;
      color: white;
      background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
      border: none;
      border-radius: 12px;
      cursor: pointer;
      margin-bottom: 15px;
    }
    .submit-btn:active { transform: translateY(2px); }
    .clear-btn {
      width: 100%;
      padding: 14px;
      font-size: 16px;
      color: #666;
      background: #f5f5f5;
      border: 2px solid #e0e0e0;
      border-radius: 12px;
      cursor: pointer;
    }
    .message {
      margin-top: 20px;
      padding: 15px;
      border-radius: 10px;
      font-size: 16px;
      font-weight: 500;
      display: none;
    }
    .message.success { background: #d4edda; color: #155724; border: 2px solid #c3e6cb; display: block; }
    .message.error { background: #f8d7da; color: #721c24; border: 2px solid #f5c6cb; display: block; }
  </style>
</head>
<body>
  <div class="container">
    <h1>🔒 Lockbox Access</h1>
    <p class="subtitle">Enter your access code</p>
    <input type="text" id="code" class="code-input" placeholder="Enter Code" maxlength="10" readonly>
    <div class="keypad">
      <button class="key" onclick="add('1')">1</button>
      <button class="key" onclick="add('2')">2</button>
      <button class="key" onclick="add('3')">3</button>
      <button class="key" onclick="add('4')">4</button>
      <button class="key" onclick="add('5')">5</button>
      <button class="key" onclick="add('6')">6</button>
      <button class="key" onclick="add('7')">7</button>
      <button class="key" onclick="add('8')">8</button>
      <button class="key" onclick="add('9')">9</button>
      <button class="key" onclick="clear()">C</button>
      <button class="key" onclick="add('0')">0</button>
      <button class="key" onclick="del()">←</button>
    </div>
    <button class="submit-btn" onclick="submit()">UNLOCK</button>
    <button class="clear-btn" onclick="clear()">Clear Code</button>
    <div id="msg" class="message"></div>
  </div>
  <script>
    const inp = document.getElementById('code');
    const msg = document.getElementById('msg');
    function add(d) { if(inp.value.length < 10) inp.value += d; }
    function del() { inp.value = inp.value.slice(0, -1); }
    function clear() { inp.value = ''; msg.className = 'message'; }
    function show(txt, type) { msg.textContent = txt; msg.className = 'message ' + type; }
    async function submit() {
      const code = inp.value.trim();
      if (!code) { show('Please enter a code', 'error'); return; }
      try {
        const res = await fetch('/unlock', {
          method: 'POST',
          headers: {'Content-Type': 'application/x-www-form-urlencoded'},
          body: 'code=' + code
        });
        const data = await res.json();
        show(data.success ? '✓ ' + data.message : '✗ ' + data.message,
             data.success ? 'success' : 'error');
        setTimeout(clear, 2500);
      } catch(e) {
        show('Connection error', 'error');
      }
    }
  </script>
</body>
</html>
)";

// ============================================================================
// FUNCTIONS
// ============================================================================

void setupLocks() {
  Serial.println("\n=== Initializing Locks ===");
  for (int i = 0; i < NUM_LOCKS; i++) {
    pinMode(LOCK_PINS[i], OUTPUT);
    digitalWrite(LOCK_PINS[i], LOW);
    locks[i].active = false;
    locks[i].startTime = 0;
    Serial.printf("Lock %d: GPIO %d - Ready\n", i + 1, LOCK_PINS[i]);
  }
}

void activateLock(int lockNum) {
  if (lockNum < 0 || lockNum >= NUM_LOCKS) return;

  Serial.printf("Activating Lock %d\n", lockNum + 1);
  digitalWrite(LOCK_PINS[lockNum], HIGH);
  locks[lockNum].active = true;
  locks[lockNum].startTime = millis();

  if (mqtt.connected()) {
    char topic[50];
    snprintf(topic, sizeof(topic), "PopLocks/Lock%d", lockNum + 1);
    mqtt.publish(topic, "activated");
  }
}

void activateAll() {
  Serial.println("*** MASTER CODE - Opening all locks ***");
  for (int i = 0; i < NUM_LOCKS; i++) {
    activateLock(i);
  }
}

void updateLocks() {
  for (int i = 0; i < NUM_LOCKS; i++) {
    if (locks[i].active) {
      if (millis() - locks[i].startTime >= LOCK_TIME_MS) {
        digitalWrite(LOCK_PINS[i], LOW);
        locks[i].active = false;
        Serial.printf("Lock %d deactivated\n", i + 1);
      }
    }
  }
}

void handleRoot() {
  server.send(200, "text/html", HTML);
}

void handleUnlock() {
  if (!server.hasArg("code")) {
    server.send(400, "application/json", "{\"success\":false,\"message\":\"No code\"}");
    return;
  }

  String code = server.arg("code");
  code.trim();
  code.toUpperCase();

  Serial.printf("Code entered: %s\n", code.c_str());

  // Check codes
  for (int i = 0; i < NUM_CODES; i++) {
    String mapCode = String(CODES[i].code);
    mapCode.toUpperCase();

    if (code == mapCode) {
      if (CODES[i].lockNum == -1) {
        activateAll();
        server.send(200, "application/json", "{\"success\":true,\"message\":\"All locks opened!\"}");
      } else {
        activateLock(CODES[i].lockNum);
        char json[128];
        snprintf(json, sizeof(json),
                "{\"success\":true,\"message\":\"%s opened!\",\"lock\":%d}",
                CODES[i].name, CODES[i].lockNum + 1);
        server.send(200, "application/json", json);
      }
      return;
    }
  }

  Serial.println("Invalid code");
  server.send(200, "application/json", "{\"success\":false,\"message\":\"Invalid code\"}");
}

void connectMQTT() {
  if (!mqtt.connected()) {
    if (mqtt.connect("pop-lock-system")) {
      Serial.println("MQTT connected");
      mqtt.publish("PopLocks/status", "online");
    }
  }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n======================================");
  Serial.println("  ESP32 Pop Lock System");
  Serial.println("======================================\n");

  setupLocks();

  // WiFi
  Serial.printf("Connecting to WiFi: %s\n", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✓ WiFi Connected!");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("\n>>> Open on iPad: http://%s <<<\n\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n✗ WiFi Failed!");
  }

  // MQTT (optional)
  mqtt.setServer(MQTT_HOST, MQTT_PORT);
  connectMQTT();

  // Web server
  server.on("/", handleRoot);
  server.on("/unlock", HTTP_POST, handleUnlock);
  server.begin();

  Serial.println("=== Access Codes ===");
  for (int i = 0; i < NUM_CODES; i++) {
    Serial.printf("%s → %s\n", CODES[i].code, CODES[i].name);
  }

  Serial.println("\n======================================");
  Serial.println("System Ready!");
  Serial.println("======================================\n");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
  server.handleClient();
  updateLocks();

  // MQTT
  if (!mqtt.connected()) {
    static unsigned long lastAttempt = 0;
    if (millis() - lastAttempt > 5000) {
      lastAttempt = millis();
      connectMQTT();
    }
  } else {
    mqtt.loop();
  }

  delay(10);
}
