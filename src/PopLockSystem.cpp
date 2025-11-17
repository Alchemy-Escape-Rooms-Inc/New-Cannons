/*
 * ESP32 Pop Lock System
 *
 * This system controls multiple electronic pop locks (solenoid locks) via a web interface.
 * Players enter a code on an iPad, and the corresponding lockbox opens.
 *
 * Hardware Requirements:
 * - ESP32 (any variant)
 * - Electronic pop locks (12V solenoids recommended)
 * - MOSFETs or relay modules for switching high current
 * - 12V power supply for locks
 * - WiFi network
 *
 * Wiring:
 * - Each lock controlled by a GPIO pin through a MOSFET/relay
 * - Lock power: External 12V supply
 * - Control signal: ESP32 GPIO -> MOSFET gate -> Lock ground
 */

#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <Arduino.h>

// ============================================================================
// CONFIGURATION
// ============================================================================

namespace PopLockConfig {
  // WiFi Configuration
  const char* WIFI_SSID = "AlchemyGuest";
  const char* WIFI_PASS = "VoodooVacation5601";

  // MQTT Configuration (optional - for logging/monitoring)
  const char* MQTT_HOST = "10.1.10.115";
  const uint16_t MQTT_PORT = 1883;
  const char* MQTT_CLIENT_ID = "pop-lock-system";

  // Lock Configuration
  const int NUM_LOCKS = 6;  // Number of pop locks
  const int LOCK_ACTIVATION_TIME_MS = 2000;  // How long to energize solenoid (2 seconds)

  // GPIO Pins for locks (modify based on your wiring)
  const int LOCK_PINS[NUM_LOCKS] = {
    25,  // Lock 1 - GPIO 25
    26,  // Lock 2 - GPIO 26
    27,  // Lock 3 - GPIO 27
    32,  // Lock 4 - GPIO 32
    33,  // Lock 5 - GPIO 33
    14   // Lock 6 - GPIO 14
  };

  // Security codes mapped to locks
  // Format: {code, lock_index}
  struct CodeMapping {
    const char* code;
    int lockIndex;
    const char* description;
  };

  const CodeMapping CODE_MAP[] = {
    {"1234", 0, "Lockbox 1 - Red Box"},
    {"5678", 1, "Lockbox 2 - Blue Box"},
    {"9012", 2, "Lockbox 3 - Green Box"},
    {"3456", 3, "Lockbox 4 - Yellow Box"},
    {"7890", 4, "Lockbox 5 - Purple Box"},
    {"2468", 5, "Lockbox 6 - Orange Box"},
    // Add more codes as needed
    {"MASTER", -1, "Open All Locks"},  // Special master code
  };

  const int NUM_CODES = sizeof(CODE_MAP) / sizeof(CodeMapping);

  // Web server port
  const int WEB_SERVER_PORT = 80;
}

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================

WebServer webServer(PopLockConfig::WEB_SERVER_PORT);
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

// Lock states
struct LockState {
  bool isActive;
  unsigned long activationTime;
};

LockState lockStates[PopLockConfig::NUM_LOCKS];

// ============================================================================
// HTML WEB INTERFACE
// ============================================================================

const char* HTML_PAGE = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Lockbox Access</title>
  <style>
    * {
      margin: 0;
      padding: 0;
      box-sizing: border-box;
    }

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

    h1 {
      color: #333;
      margin-bottom: 10px;
      font-size: 32px;
    }

    .subtitle {
      color: #666;
      margin-bottom: 30px;
      font-size: 16px;
    }

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
      transition: all 0.3s ease;
    }

    .code-input:focus {
      outline: none;
      border-color: #667eea;
      box-shadow: 0 0 0 4px rgba(102, 126, 234, 0.1);
    }

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
      transition: all 0.3s ease;
      margin-bottom: 15px;
    }

    .submit-btn:hover {
      transform: translateY(-2px);
      box-shadow: 0 10px 25px rgba(102, 126, 234, 0.4);
    }

    .submit-btn:active {
      transform: translateY(0);
    }

    .clear-btn {
      width: 100%;
      padding: 14px;
      font-size: 16px;
      color: #666;
      background: #f5f5f5;
      border: 2px solid #e0e0e0;
      border-radius: 12px;
      cursor: pointer;
      transition: all 0.3s ease;
    }

    .clear-btn:hover {
      background: #e8e8e8;
      border-color: #ccc;
    }

    .message {
      margin-top: 20px;
      padding: 15px;
      border-radius: 10px;
      font-size: 16px;
      font-weight: 500;
      display: none;
    }

    .message.success {
      background: #d4edda;
      color: #155724;
      border: 2px solid #c3e6cb;
      display: block;
    }

    .message.error {
      background: #f8d7da;
      color: #721c24;
      border: 2px solid #f5c6cb;
      display: block;
    }

    .message.info {
      background: #d1ecf1;
      color: #0c5460;
      border: 2px solid #bee5eb;
      display: block;
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
      transition: all 0.2s ease;
    }

    .key:hover {
      background: #e9ecef;
      border-color: #667eea;
    }

    .key:active {
      transform: scale(0.95);
      background: #dee2e6;
    }

    @media (max-width: 480px) {
      .container {
        padding: 25px;
      }

      h1 {
        font-size: 26px;
      }

      .code-input {
        font-size: 20px;
        padding: 15px;
      }

      .submit-btn {
        font-size: 18px;
        padding: 15px;
      }
    }
  </style>
</head>
<body>
  <div class="container">
    <h1>🔒 Lockbox Access</h1>
    <p class="subtitle">Enter your access code</p>

    <input type="text"
           id="codeInput"
           class="code-input"
           placeholder="Enter Code"
           maxlength="10"
           autocomplete="off"
           readonly>

    <div class="keypad">
      <button class="key" onclick="addDigit('1')">1</button>
      <button class="key" onclick="addDigit('2')">2</button>
      <button class="key" onclick="addDigit('3')">3</button>
      <button class="key" onclick="addDigit('4')">4</button>
      <button class="key" onclick="addDigit('5')">5</button>
      <button class="key" onclick="addDigit('6')">6</button>
      <button class="key" onclick="addDigit('7')">7</button>
      <button class="key" onclick="addDigit('8')">8</button>
      <button class="key" onclick="addDigit('9')">9</button>
      <button class="key" onclick="clearCode()">C</button>
      <button class="key" onclick="addDigit('0')">0</button>
      <button class="key" onclick="deleteDigit()">←</button>
    </div>

    <button class="submit-btn" onclick="submitCode()">UNLOCK</button>
    <button class="clear-btn" onclick="clearCode()">Clear Code</button>

    <div id="message" class="message"></div>
  </div>

  <script>
    const codeInput = document.getElementById('codeInput');
    const messageDiv = document.getElementById('message');

    function addDigit(digit) {
      if (codeInput.value.length < 10) {
        codeInput.value += digit;
      }
    }

    function deleteDigit() {
      codeInput.value = codeInput.value.slice(0, -1);
    }

    function clearCode() {
      codeInput.value = '';
      hideMessage();
    }

    function showMessage(text, type) {
      messageDiv.textContent = text;
      messageDiv.className = 'message ' + type;
    }

    function hideMessage() {
      messageDiv.className = 'message';
    }

    async function submitCode() {
      const code = codeInput.value.trim();

      if (!code) {
        showMessage('Please enter a code', 'error');
        return;
      }

      try {
        showMessage('Verifying code...', 'info');

        const response = await fetch('/unlock', {
          method: 'POST',
          headers: {
            'Content-Type': 'application/x-www-form-urlencoded',
          },
          body: 'code=' + encodeURIComponent(code)
        });

        const data = await response.json();

        if (data.success) {
          showMessage('✓ ' + data.message, 'success');
          setTimeout(() => {
            clearCode();
          }, 3000);
        } else {
          showMessage('✗ ' + data.message, 'error');
          setTimeout(() => {
            clearCode();
          }, 2000);
        }
      } catch (error) {
        showMessage('Connection error. Please try again.', 'error');
        console.error('Error:', error);
      }
    }

    // Allow Enter key to submit
    codeInput.addEventListener('keypress', function(e) {
      if (e.key === 'Enter') {
        submitCode();
      }
    });

    // Auto-focus on load
    window.addEventListener('load', function() {
      codeInput.focus();
    });
  </script>
</body>
</html>
)rawliteral";

// ============================================================================
// LOCK CONTROL FUNCTIONS
// ============================================================================

void initializeLocks() {
  Serial.println("\n=== Initializing Pop Locks ===");

  for (int i = 0; i < PopLockConfig::NUM_LOCKS; i++) {
    pinMode(PopLockConfig::LOCK_PINS[i], OUTPUT);
    digitalWrite(PopLockConfig::LOCK_PINS[i], LOW);  // Locks OFF initially
    lockStates[i].isActive = false;
    lockStates[i].activationTime = 0;

    Serial.printf("Lock %d: GPIO %d - Ready\n", i + 1, PopLockConfig::LOCK_PINS[i]);
  }

  Serial.println("All locks initialized");
}

void activateLock(int lockIndex) {
  if (lockIndex < 0 || lockIndex >= PopLockConfig::NUM_LOCKS) {
    Serial.printf("ERROR: Invalid lock index %d\n", lockIndex);
    return;
  }

  Serial.printf("Activating Lock %d (GPIO %d)...\n",
                lockIndex + 1, PopLockConfig::LOCK_PINS[lockIndex]);

  digitalWrite(PopLockConfig::LOCK_PINS[lockIndex], HIGH);
  lockStates[lockIndex].isActive = true;
  lockStates[lockIndex].activationTime = millis();

  // Publish to MQTT if connected
  if (mqttClient.connected()) {
    char topic[64];
    char message[128];
    snprintf(topic, sizeof(topic), "PopLocks/Lock%d/status", lockIndex + 1);
    snprintf(message, sizeof(message), "activated");
    mqttClient.publish(topic, message);
  }
}

void deactivateLock(int lockIndex) {
  if (lockIndex < 0 || lockIndex >= PopLockConfig::NUM_LOCKS) {
    return;
  }

  digitalWrite(PopLockConfig::LOCK_PINS[lockIndex], LOW);
  lockStates[lockIndex].isActive = false;

  Serial.printf("Lock %d deactivated\n", lockIndex + 1);
}

void updateLocks() {
  unsigned long currentTime = millis();

  for (int i = 0; i < PopLockConfig::NUM_LOCKS; i++) {
    if (lockStates[i].isActive) {
      if (currentTime - lockStates[i].activationTime >= PopLockConfig::LOCK_ACTIVATION_TIME_MS) {
        deactivateLock(i);
      }
    }
  }
}

void activateAllLocks() {
  Serial.println("*** MASTER CODE - Activating all locks ***");

  for (int i = 0; i < PopLockConfig::NUM_LOCKS; i++) {
    activateLock(i);
  }
}

// ============================================================================
// WEB SERVER HANDLERS
// ============================================================================

void handleRoot() {
  webServer.send(200, "text/html", HTML_PAGE);
}

void handleUnlock() {
  if (!webServer.hasArg("code")) {
    webServer.send(400, "application/json", "{\"success\":false,\"message\":\"No code provided\"}");
    return;
  }

  String code = webServer.arg("code");
  code.trim();
  code.toUpperCase();  // Make comparison case-insensitive

  Serial.printf("Code received: %s\n", code.c_str());

  // Check for master code
  if (code == "MASTER") {
    activateAllLocks();
    webServer.send(200, "application/json",
                  "{\"success\":true,\"message\":\"All locks opened!\"}");
    return;
  }

  // Search for matching code
  for (int i = 0; i < PopLockConfig::NUM_CODES; i++) {
    String mapCode = String(PopLockConfig::CODE_MAP[i].code);
    mapCode.toUpperCase();

    if (code == mapCode) {
      int lockIndex = PopLockConfig::CODE_MAP[i].lockIndex;

      if (lockIndex >= 0 && lockIndex < PopLockConfig::NUM_LOCKS) {
        activateLock(lockIndex);

        char response[256];
        snprintf(response, sizeof(response),
                "{\"success\":true,\"message\":\"%s opened!\",\"lock\":%d}",
                PopLockConfig::CODE_MAP[i].description,
                lockIndex + 1);

        webServer.send(200, "application/json", response);

        // Log to MQTT
        if (mqttClient.connected()) {
          char topic[64];
          snprintf(topic, sizeof(topic), "PopLocks/access");
          mqttClient.publish(topic, response);
        }

        return;
      }
    }
  }

  // Code not found
  Serial.println("Invalid code entered");
  webServer.send(200, "application/json",
                "{\"success\":false,\"message\":\"Invalid code. Please try again.\"}");
}

void handleStatus() {
  String json = "{\"locks\":[";

  for (int i = 0; i < PopLockConfig::NUM_LOCKS; i++) {
    if (i > 0) json += ",";
    json += "{";
    json += "\"id\":" + String(i + 1) + ",";
    json += "\"pin\":" + String(PopLockConfig::LOCK_PINS[i]) + ",";
    json += "\"active\":" + String(lockStates[i].isActive ? "true" : "false");
    json += "}";
  }

  json += "]}";

  webServer.send(200, "application/json", json);
}

// ============================================================================
// MQTT FUNCTIONS
// ============================================================================

void connectMQTT() {
  if (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");

    if (mqttClient.connect(PopLockConfig::MQTT_CLIENT_ID)) {
      Serial.println(" Connected!");
      mqttClient.publish("PopLocks/status", "online");
      mqttClient.subscribe("PopLocks/command");
    } else {
      Serial.printf(" Failed (rc=%d)\n", mqttClient.state());
    }
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[256];
  if (length >= sizeof(message)) length = sizeof(message) - 1;
  memcpy(message, payload, length);
  message[length] = '\0';

  Serial.printf("MQTT: %s -> %s\n", topic, message);

  // Handle remote commands
  String topicStr = String(topic);
  String messageStr = String(message);

  if (topicStr == "PopLocks/command") {
    if (messageStr.startsWith("unlock:")) {
      int lockNum = messageStr.substring(7).toInt();
      if (lockNum > 0 && lockNum <= PopLockConfig::NUM_LOCKS) {
        activateLock(lockNum - 1);
      }
    } else if (messageStr == "unlock:all") {
      activateAllLocks();
    }
  }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("\n\n");
  Serial.println("======================================");
  Serial.println("  ESP32 Pop Lock System v1.0");
  Serial.println("======================================");

  // Initialize locks
  initializeLocks();

  // Connect to WiFi
  Serial.printf("\nConnecting to WiFi: %s\n", PopLockConfig::WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(PopLockConfig::WIFI_SSID, PopLockConfig::WIFI_PASS);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi Connected!");
    Serial.printf("IP Address: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("Open this URL on your iPad: http://%s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi connection failed!");
    Serial.println("Please check your WiFi credentials");
  }

  // Setup MQTT
  mqttClient.setServer(PopLockConfig::MQTT_HOST, PopLockConfig::MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  connectMQTT();

  // Setup web server
  webServer.on("/", handleRoot);
  webServer.on("/unlock", HTTP_POST, handleUnlock);
  webServer.on("/status", handleStatus);

  webServer.begin();
  Serial.println("Web server started");

  // Print code map
  Serial.println("\n=== Access Codes ===");
  for (int i = 0; i < PopLockConfig::NUM_CODES; i++) {
    Serial.printf("Code: %s -> %s\n",
                  PopLockConfig::CODE_MAP[i].code,
                  PopLockConfig::CODE_MAP[i].description);
  }

  Serial.println("\n======================================");
  Serial.println("System Ready!");
  Serial.println("======================================\n");
}

// ============================================================================
// MAIN LOOP
// ============================================================================

void loop() {
  // Handle web requests
  webServer.handleClient();

  // Update lock states (auto-deactivate after timeout)
  updateLocks();

  // MQTT maintenance
  if (!mqttClient.connected()) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 5000) {
      lastReconnect = millis();
      connectMQTT();
    }
  } else {
    mqttClient.loop();
  }

  // Small delay to prevent watchdog issues
  delay(10);
}
