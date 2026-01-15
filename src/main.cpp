/*
 * ============================================================================
 * Cannon Controller - ESP32
 * Alchemy Escape Rooms - "A Mermaid's Tale"
 * ============================================================================
 *
 * DESCRIPTION:
 * Controls cannon props with VL6180X distance sensor (loading detection) and
 * ALS31300 magnetic angle sensor (aiming). Publishes load/fire events and
 * angle changes via MQTT.
 *
 * WATCHTOWER PROTOCOL COMPLIANT ✓
 * - /command topic with PING/PONG, STATUS, RESET, PUZZLE_RESET
 * - /status topic for state reports
 * - /log topic for debug output
 * - Topic buffer copy fix in callback
 * - 5-minute heartbeat
 * - VERSION define
 *
 * GRAVITY GAMES INTEGRATION COMPLIANT ✓
 * - Topic format: MermaidsTale/Cannon1Loaded (no extra slash)
 * - Payload format: "triggered" for events, "pre_45" for angles
 * - Auto-clear after firing
 *
 * VERSION HISTORY:
 * v3.0.0 - 2026-01-14 - Added Watchtower Protocol compliance
 * v2.0.0 - 2025-11-14 - Gravity Games Integration spec compliance
 * v1.0.0 - Original version
 *
 * ============================================================================
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_VL6180X.h>
#include <esp_task_wdt.h>
#include <stdarg.h>

// ============================================================================
// VERSION - Watchtower Protocol Requirement
// ============================================================================
#define VERSION "3.0.0"

// ============================================================================
// CONFIGURATION - CHANGE THESE FOR EACH CANNON
// ============================================================================
namespace config {
  // ┌────────────────────────────────────────────────────────────────────────┐
  // │  CANNON IDENTITY - SET THIS FOR EACH DEVICE                            │
  // └────────────────────────────────────────────────────────────────────────┘
  constexpr uint8_t CANNON_ID = 1;  // ← CHANGE TO 1 or 2

  // WiFi
  const char* WIFI_SSID     = "AlchemyGuest";
  const char* WIFI_PASSWORD = "VoodooVacation5601";

  // MQTT Broker - Alchemy Standard
  const char* MQTT_SERVER = "10.1.10.115";
  const int   MQTT_PORT   = 1883;  // Per Watchtower docs, 1883 is acceptable

  // Filter coefficients
  constexpr float DISTANCE_FILTER_ALPHA = 0.2f;   // 20% new, 80% old
  constexpr float ANGLE_FILTER_ALPHA = 0.3f;      // 30% new, 70% old

  // Change detection thresholds
  constexpr float MAX_ANGLE_JUMP_DEG = 10.0f;     // Reject unrealistic jumps
  constexpr int MIN_ANGLE_CHANGE_DEG = 1;         // Publish threshold
  constexpr uint8_t MIN_DISTANCE_CHANGE_MM = 2;   // Publish threshold

  // Load detection
  constexpr uint8_t LOAD_DISTANCE_THRESHOLD_MM = 50;  // Object closer = loaded

  // Timing
  constexpr uint32_t STATUS_HEARTBEAT_MS = 300000;    // 5 minutes - Watchtower
  constexpr uint32_t STARTUP_SETTLE_MS = 1000;
  constexpr uint32_t MQTT_RECONNECT_MS = 5000;
  constexpr uint32_t WATCHDOG_TIMEOUT_S = 10;
  constexpr uint32_t CLEAR_STATUS_DELAY_MS = 1000;    // Auto-clear after fire

  // Hardware pins
  constexpr int BUTTON_PIN = 35;
  constexpr int I2C_SDA = 15;
  constexpr int I2C_SCL = 18;
  constexpr uint8_t ALS31300_ADDR = 0x65;
}

// ============================================================================
// MQTT TOPICS - Generated from CANNON_ID
// ============================================================================
char PROP_NAME[16];                    // "Cannon1" or "Cannon2"

// Watchtower Protocol Topics
char MQTT_TOPIC_COMMAND[64];           // MermaidsTale/Cannon1/command
char MQTT_TOPIC_STATUS[64];            // MermaidsTale/Cannon1/status
char MQTT_TOPIC_LOG[64];               // MermaidsTale/Cannon1/log

// Gravity Games Integration Topics (no extra slash!)
char MQTT_TOPIC_LOADED[64];            // MermaidsTale/Cannon1Loaded
char MQTT_TOPIC_FIRED[64];             // MermaidsTale/Cannon1Fired
char MQTT_TOPIC_HOR[64];               // MermaidsTale/Cannon1Hor
char MQTT_TOPIC_VER[64];               // MermaidsTale/Cannon1Ver

// LWT Topic
char MQTT_LWT_TOPIC[64];               // MermaidsTale/Cannon1/status

// ============================================================================
// GLOBAL STATE
// ============================================================================
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
Adafruit_VL6180X vl6180x = Adafruit_VL6180X();

// Sensor initialization flags
bool vl6180xInitialized = false;
bool als31300Initialized = false;

// State tracking
bool isLoaded = false;
bool hasFired = false;
int lastAngle = -999;
float filteredDistance = 0;
float filteredAngle = 0;

// Timing
uint32_t lastHeartbeatMs = 0;
uint32_t lastActivityMs = 0;
uint32_t firedAtMs = 0;
bool pendingClear = false;

// ============================================================================
// MQTT LOGGING - Watchtower Protocol
// ============================================================================
void mqttLogf(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  Serial.printf("[LOG] %s\n", buf);
  if (mqttClient.connected()) {
    mqttClient.publish(MQTT_TOPIC_LOG, buf);
  }
}

// ============================================================================
// TOPIC INITIALIZATION - Called in setup()
// ============================================================================
void initializeTopics() {
  // Prop name
  snprintf(PROP_NAME, sizeof(PROP_NAME), "Cannon%d", config::CANNON_ID);

  // Watchtower Protocol topics
  snprintf(MQTT_TOPIC_COMMAND, sizeof(MQTT_TOPIC_COMMAND),
           "MermaidsTale/%s/command", PROP_NAME);
  snprintf(MQTT_TOPIC_STATUS, sizeof(MQTT_TOPIC_STATUS),
           "MermaidsTale/%s/status", PROP_NAME);
  snprintf(MQTT_TOPIC_LOG, sizeof(MQTT_TOPIC_LOG),
           "MermaidsTale/%s/log", PROP_NAME);
  snprintf(MQTT_LWT_TOPIC, sizeof(MQTT_LWT_TOPIC),
           "MermaidsTale/%s/status", PROP_NAME);

  // Gravity Games Integration topics (NO slash between Cannon# and event!)
  snprintf(MQTT_TOPIC_LOADED, sizeof(MQTT_TOPIC_LOADED),
           "MermaidsTale/Cannon%dLoaded", config::CANNON_ID);
  snprintf(MQTT_TOPIC_FIRED, sizeof(MQTT_TOPIC_FIRED),
           "MermaidsTale/Cannon%dFired", config::CANNON_ID);
  snprintf(MQTT_TOPIC_HOR, sizeof(MQTT_TOPIC_HOR),
           "MermaidsTale/Cannon%dHor", config::CANNON_ID);
  snprintf(MQTT_TOPIC_VER, sizeof(MQTT_TOPIC_VER),
           "MermaidsTale/Cannon%dVer", config::CANNON_ID);
}

// ============================================================================
// STATUS REPORT - Watchtower Protocol
// ============================================================================
void sendStatusReport() {
  char status[256];
  snprintf(status, sizeof(status),
    "ONLINE | v%s | Loaded:%s | Fired:%s | Angle:%d | VL6180X:%s | ALS31300:%s | Uptime:%lums",
    VERSION,
    isLoaded ? "YES" : "NO",
    hasFired ? "YES" : "NO",
    lastAngle,
    vl6180xInitialized ? "OK" : "FAIL",
    als31300Initialized ? "OK" : "FAIL",
    millis()
  );

  mqttClient.publish(MQTT_TOPIC_STATUS, status, true);  // Retained
  Serial.printf("[STATUS] %s\n", status);
}

// ============================================================================
// MQTT CALLBACK - Watchtower Protocol Compliant
// ============================================================================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // ┌────────────────────────────────────────────────────────────────────────┐
  // │  CRITICAL: Copy topic buffer FIRST before any other stack variables   │
  // │  This prevents stack corruption from the PubSubClient buffer reuse    │
  // └────────────────────────────────────────────────────────────────────────┘
  char topicBuf[128];
  strncpy(topicBuf, topic, sizeof(topicBuf) - 1);
  topicBuf[sizeof(topicBuf) - 1] = '\0';

  // Now safe to declare other variables
  char msg[128];
  if (length >= sizeof(msg)) length = sizeof(msg) - 1;
  memcpy(msg, payload, length);
  msg[length] = '\0';

  // Update activity timestamp
  lastActivityMs = millis();

  Serial.printf("[MQTT] %s -> %s\n", topicBuf, msg);

  // ════════════════════════════════════════════════════════════════════════
  // WATCHTOWER STANDARD COMMANDS - /command topic
  // ════════════════════════════════════════════════════════════════════════
  if (strcmp(topicBuf, MQTT_TOPIC_COMMAND) == 0) {

    // PING - Health check for System Checker
    if (strcmp(msg, "PING") == 0) {
      mqttClient.publish(MQTT_TOPIC_COMMAND, "PONG");
      Serial.println("[MQTT] PING -> PONG");
      return;
    }

    // STATUS - Report current state
    if (strcmp(msg, "STATUS") == 0) {
      sendStatusReport();
      mqttClient.publish(MQTT_TOPIC_COMMAND, "OK");
      Serial.println("[MQTT] STATUS -> OK");
      return;
    }

    // RESET - Reboot the device
    if (strcmp(msg, "RESET") == 0) {
      mqttClient.publish(MQTT_TOPIC_COMMAND, "OK");
      mqttLogf("RESET command received - rebooting...");
      delay(100);
      ESP.restart();
      return;
    }

    // PUZZLE_RESET - Reset state without rebooting
    if (strcmp(msg, "PUZZLE_RESET") == 0) {
      isLoaded = false;
      hasFired = false;
      pendingClear = false;
      lastAngle = -999;

      // Clear the Gravity Games topics
      mqttClient.publish(MQTT_TOPIC_LOADED, "clear");
      mqttClient.publish(MQTT_TOPIC_FIRED, "clear");

      mqttClient.publish(MQTT_TOPIC_COMMAND, "OK");
      mqttLogf("PUZZLE_RESET -> State cleared");
      return;
    }

    // Unknown command
    mqttLogf("Unknown command: %s", msg);
    return;
  }
}

// ============================================================================
// WIFI MANAGEMENT
// ============================================================================
void setupWiFi() {
  Serial.printf("\nConnecting to %s", config::WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(config::WIFI_SSID, config::WIFI_PASSWORD);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 30) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\nWiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\nWiFi connection failed - will retry in loop");
  }
}

void checkWiFi() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Disconnected - reconnecting...");
    WiFi.disconnect();
    WiFi.begin(config::WIFI_SSID, config::WIFI_PASSWORD);
  }
}

// ============================================================================
// MQTT MANAGEMENT
// ============================================================================
void connectMQTT() {
  if (mqttClient.connected()) return;
  if (WiFi.status() != WL_CONNECTED) return;

  static uint32_t lastAttempt = 0;
  if (millis() - lastAttempt < config::MQTT_RECONNECT_MS) return;
  lastAttempt = millis();

  Serial.printf("[MQTT] Connecting to %s:%d...\n", config::MQTT_SERVER, config::MQTT_PORT);

  // Connect with LWT (Last Will Testament)
  if (mqttClient.connect(PROP_NAME, NULL, NULL, MQTT_LWT_TOPIC, 0, true, "OFFLINE")) {
    Serial.println("[MQTT] Connected!");

    // Subscribe to command topic - Watchtower Protocol
    mqttClient.subscribe(MQTT_TOPIC_COMMAND);
    Serial.printf("[MQTT] Subscribed to: %s\n", MQTT_TOPIC_COMMAND);

    // Publish online status
    mqttClient.publish(MQTT_TOPIC_STATUS, "ONLINE", true);

    // Send full status report on boot
    sendStatusReport();
    mqttLogf("Boot complete - v%s", VERSION);

  } else {
    Serial.printf("[MQTT] Failed, rc=%d\n", mqttClient.state());
  }
}

// ============================================================================
// SENSOR INITIALIZATION
// ============================================================================
void setupSensors() {
  Wire.begin(config::I2C_SDA, config::I2C_SCL);

  // VL6180X Distance Sensor
  Serial.println("\nInitializing VL6180X...");
  if (vl6180x.begin()) {
    vl6180xInitialized = true;
    Serial.println("[VL6180X] Initialized successfully!");
  } else {
    Serial.println("[VL6180X] FAILED to initialize!");
  }

  // ALS31300 Magnetic Angle Sensor
  // Note: You'll need to add your ALS31300 library initialization here
  // For now, marking as initialized for the template
  Serial.println("\nInitializing ALS31300...");
  // als31300Initialized = als.begin();
  als31300Initialized = true;  // Placeholder - add your library init
  if (als31300Initialized) {
    Serial.println("[ALS31300] Initialized successfully!");
  } else {
    Serial.println("[ALS31300] FAILED to initialize!");
  }
}

// ============================================================================
// SENSOR READING
// ============================================================================
uint8_t readDistance() {
  if (!vl6180xInitialized) return 255;

  uint8_t range = vl6180x.readRange();
  uint8_t status = vl6180x.readRangeStatus();

  if (status == VL6180X_ERROR_NONE) {
    // Apply low-pass filter
    filteredDistance = (config::DISTANCE_FILTER_ALPHA * range) +
                       ((1.0f - config::DISTANCE_FILTER_ALPHA) * filteredDistance);
    return (uint8_t)filteredDistance;
  }

  return 255;  // Error
}

int readAngle() {
  if (!als31300Initialized) return -1;

  // Placeholder - replace with your ALS31300 reading code
  // Example:
  // int rawAngle = als.readAngle();
  //
  // // Reject unrealistic jumps
  // if (lastAngle != -999 && abs(rawAngle - lastAngle) > config::MAX_ANGLE_JUMP_DEG) {
  //   return lastAngle;
  // }
  //
  // // Apply low-pass filter
  // filteredAngle = (config::ANGLE_FILTER_ALPHA * rawAngle) +
  //                 ((1.0f - config::ANGLE_FILTER_ALPHA) * filteredAngle);
  //
  // return (int)filteredAngle;

  return 45;  // Placeholder
}

// ============================================================================
// GRAVITY GAMES INTEGRATION - Event Publishing
// ============================================================================
void publishLoaded() {
  mqttClient.publish(MQTT_TOPIC_LOADED, "triggered");
  mqttLogf("Cannon LOADED");
}

void publishFired() {
  mqttClient.publish(MQTT_TOPIC_FIRED, "triggered");
  mqttLogf("Cannon FIRED");

  // Start the auto-clear timer
  firedAtMs = millis();
  pendingClear = true;
}

void publishAngle(int angle) {
  // Format per Gravity Games spec: "pre_45" not just "45"
  char payload[16];
  snprintf(payload, sizeof(payload), "pre_%d", angle);

  mqttClient.publish(MQTT_TOPIC_HOR, payload);
  // Serial.printf("[ANGLE] %s -> %s\n", MQTT_TOPIC_HOR, payload);
}

void clearLoadedAndFired() {
  mqttClient.publish(MQTT_TOPIC_LOADED, "clear");
  mqttClient.publish(MQTT_TOPIC_FIRED, "clear");

  isLoaded = false;
  hasFired = false;
  pendingClear = false;

  mqttLogf("Cleared Loaded and Fired statuses - ready to fire again");
}

// ============================================================================
// MAIN CANNON LOGIC
// ============================================================================
void processCannonState() {
  // Read distance
  uint8_t distance = readDistance();

  // Check for loading
  if (!isLoaded && distance < config::LOAD_DISTANCE_THRESHOLD_MM) {
    isLoaded = true;
    publishLoaded();
  }

  // Read button for firing (active low)
  bool buttonPressed = (digitalRead(config::BUTTON_PIN) == LOW);

  // Check for firing
  if (isLoaded && !hasFired && buttonPressed) {
    hasFired = true;
    publishFired();
  }

  // Auto-clear after firing (Gravity Games spec)
  if (pendingClear && (millis() - firedAtMs >= config::CLEAR_STATUS_DELAY_MS)) {
    clearLoadedAndFired();
  }

  // Read and publish angle changes
  int angle = readAngle();
  if (angle >= 0 && abs(angle - lastAngle) >= config::MIN_ANGLE_CHANGE_DEG) {
    lastAngle = angle;
    publishAngle(angle);
  }
}

// ============================================================================
// HEARTBEAT - Watchtower Protocol (5-minute interval)
// ============================================================================
void checkHeartbeat() {
  if (millis() - lastHeartbeatMs >= config::STATUS_HEARTBEAT_MS) {
    lastHeartbeatMs = millis();
    sendStatusReport();
  }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(100);

  Serial.println("\n========================================");
  Serial.printf("Cannon Controller - Cannon %d\n", config::CANNON_ID);
  Serial.printf("Version: %s\n", VERSION);
  Serial.println("Watchtower Protocol: ENABLED");
  Serial.println("========================================\n");

  // Initialize topic strings
  initializeTopics();

  // Hardware setup
  pinMode(config::BUTTON_PIN, INPUT_PULLUP);

  // Sensor setup
  setupSensors();

  // Network setup
  setupWiFi();
  mqttClient.setServer(config::MQTT_SERVER, config::MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  mqttClient.setBufferSize(512);

  // Watchdog
  esp_task_wdt_init(config::WATCHDOG_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);

  // Let sensors settle
  delay(config::STARTUP_SETTLE_MS);

  // Initial readings to prime filters
  filteredDistance = readDistance();
  filteredAngle = readAngle();
  lastAngle = (int)filteredAngle;

  lastHeartbeatMs = millis();

  Serial.println("\n=== Setup Complete ===");
  Serial.printf("Command topic: %s\n", MQTT_TOPIC_COMMAND);
  Serial.printf("Status topic:  %s\n", MQTT_TOPIC_STATUS);
  Serial.printf("Log topic:     %s\n", MQTT_TOPIC_LOG);
  Serial.printf("Loaded topic:  %s\n", MQTT_TOPIC_LOADED);
  Serial.printf("Fired topic:   %s\n", MQTT_TOPIC_FIRED);
  Serial.printf("Angle topic:   %s\n", MQTT_TOPIC_HOR);
  Serial.println("======================\n");
}

// ============================================================================
// LOOP
// ============================================================================
void loop() {
  esp_task_wdt_reset();

  // Network maintenance
  checkWiFi();
  connectMQTT();
  mqttClient.loop();

  // Main cannon logic
  if (mqttClient.connected()) {
    processCannonState();
    checkHeartbeat();
  }

  delay(50);  // ~20Hz update rate
}
