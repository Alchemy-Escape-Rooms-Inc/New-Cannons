#include <Adafruit_VL6180X.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_task_wdt.h>  // For watchdog timer

#include <Arduino.h>
#include "boardkit.hpp"

#include "config/MqttConfig.h"
#include "state/CannonStateView.h"
#include "state/ControllerState.h"
#include "telemetry/CannonTelemetry.h"
#include "telemetry/ControllerTelemetrySource.h"

// ============================================================================
// VERSION - Sourced from MANIFEST.h (single source of truth)
// ============================================================================
#define VERSION manifest::FIRMWARE_VERSION

// ============================================================================
// CONFIGURATION CONSTANTS — Sourced from MANIFEST.h (single source of truth)
// ============================================================================
// All values below are pulled from the manifest. To change any value,
// edit MANIFEST.h — not this file. This bridge exists so that every
// reference to config:: throughout main.cpp continues to work unchanged.
// ============================================================================
namespace config {
  // Cannon Identity — sourced from manifest
  constexpr uint8_t CANNON_ID = manifest::CANNON_ID;

  // Filter coefficients — sourced from manifest
  constexpr float DISTANCE_FILTER_ALPHA = manifest::DISTANCE_FILTER_ALPHA;
  constexpr float ANGLE_FILTER_ALPHA = manifest::ANGLE_FILTER_ALPHA;

  // Change detection thresholds — sourced from manifest
  constexpr float MAX_ANGLE_JUMP_DEG = manifest::MAX_ANGLE_JUMP_DEG;
  constexpr int MIN_ANGLE_CHANGE_DEG = manifest::MIN_ANGLE_CHANGE_DEG;
  constexpr uint8_t MIN_DISTANCE_CHANGE_MM = manifest::MIN_DISTANCE_CHANGE_MM;

  // Timing — sourced from manifest
  constexpr uint32_t STATUS_REPORT_INTERVAL_MS = manifest::STATUS_REPORT_INTERVAL_MS;
  constexpr uint32_t WATCHTOWER_HEARTBEAT_MS = manifest::WATCHTOWER_HEARTBEAT_MS;
  constexpr uint32_t STARTUP_SETTLE_MS = manifest::STARTUP_SETTLE_MS;
  constexpr uint32_t MQTT_RECONNECT_CHECK_MS = manifest::MQTT_RECONNECT_CHECK_MS;
  constexpr uint32_t WATCHDOG_TIMEOUT_S = manifest::WATCHDOG_TIMEOUT_S;

  // Hardware — sourced from manifest
  constexpr int BUTTON_PIN = manifest::BUTTON_PIN;
  constexpr int BUTTON_DEBOUNCE_MS = manifest::BUTTON_DEBOUNCE_MS;
  constexpr uint8_t ALS_FALLBACK_ADDR = manifest::ALS_FALLBACK_ADDR;
  constexpr int I2C_SDA_PIN = manifest::I2C_SDA_PIN;
  constexpr int I2C_SCL_PIN = manifest::I2C_SCL_PIN;
  constexpr uint32_t I2C_FREQUENCY = manifest::I2C_FREQUENCY;

  // VL6180X Error Codes — sourced from manifest
  constexpr uint8_t VL6180X_ERR_ECE_FAIL = manifest::VL6180X_ERR_ECE_FAIL;
  constexpr uint8_t VL6180X_ERR_VCSEL_WD = manifest::VL6180X_ERR_VCSEL_WD;
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================
enum class ResetState { IDLE, PENDING, IN_PROGRESS };
static ResetState resetState = ResetState::IDLE;
static unsigned long resetStartTime = 0;

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================
void sendStartupStatus();
void scanI2CDevices();
void handleReset();
void handleMqttReconnection();
void onMqttMessage(char *topic, byte *payload, unsigned int length);
void sendWatchtowerStatus();
void watchtowerHeartbeat();

// Watchtower Protocol Topics
char WATCHTOWER_COMMAND_TOPIC[64];  // MermaidsTale/Cannon1/command
char WATCHTOWER_STATUS_TOPIC[64];   // MermaidsTale/Cannon1/status
char WATCHTOWER_LOG_TOPIC[64];      // MermaidsTale/Cannon1/log
uint32_t lastWatchtowerHeartbeat = 0;

// Helper function to build cannon-specific topics
void buildCannonTopic(char* out, size_t cap, const char* suffix) {
  snprintf(out, cap, "MermaidsTale/Cannon%d/%s", config::CANNON_ID, suffix);
}

// ============================================================================
// GLOBAL OBJECTS
// ============================================================================
static uint8_t detectedALS_ADDR = 0x00;
static bool alsAddressDetected = false;

Adafruit_VL6180X distanceSensor = Adafruit_VL6180X();
bool vl6180xInitialized = false;
bool als31300Initialized = false;
ALS31300::Sensor als(config::ALS_FALLBACK_ADDR);

Controller ctrl(
  BoardPins::DevKitS3_DefaultI2C(config::I2C_SDA_PIN, config::I2C_SCL_PIN, config::I2C_FREQUENCY),
  config::BUTTON_PIN,
  Pull::Up,
  ActivePolarity::ActiveLow,
  config::BUTTON_DEBOUNCE_MS
);

ctl::State gstate;
ControllerTelemetrySource tSource(gstate);
WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
ArduinoPubSubClientAdapter mqttAdapter(pubSubClient);

static float getAngleDeg(const ctl::State &s) { return s.getAngleDeg(); }
static bool getLoaded(const ctl::State &s) { return s.getLoaded(); }
static bool getFired(const ctl::State &s) { return s.getFired(); }

cannon::StateView<ctl::State> cView(gstate, &getAngleDeg, &getLoaded, &getFired);

// Build base topic for this cannon - initialized in setup()
char cannonBaseTopic[64] = {0};

telem::TelemetryConfig tcfg{
    cannonBaseTopic,
    "state",
    "changes",
    true,
    0
};
telem::TelemetryPublisher tPub(mqttAdapter, tSource, tcfg);

integ::CannonTelemetry cannonPub(mqttAdapter, "MermaidsTale");

// ============================================================================
// MQTT MESSAGE HANDLER
// ============================================================================
void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  char message[128];
  size_t len = (length < sizeof(message) - 1) ? length : sizeof(message) - 1;
  memcpy(message, payload, len);
  message[len] = '\0';

  String topicStr = String(topic);

  // Build expected topics for this cannon
  char resetTopic[64], statusTopic[64];
  buildCannonTopic(resetTopic, sizeof(resetTopic), "reset");
  buildCannonTopic(statusTopic, sizeof(statusTopic), "status");

  // ════════════════════════════════════════════════════════════════════════
  // WATCHTOWER PROTOCOL COMMANDS - /command topic
  // ════════════════════════════════════════════════════════════════════════
  if (topicStr == WATCHTOWER_COMMAND_TOPIC) {
    // PING - Health check
    if (strcmp(message, "PING") == 0) {
      mqttAdapter.publish(WATCHTOWER_COMMAND_TOPIC, "PONG", false, 0);
      Serial.println("[Watchtower] PING -> PONG");
      return;
    }

    // STATUS - Report current state
    if (strcmp(message, "STATUS") == 0) {
      sendWatchtowerStatus();
      mqttAdapter.publish(WATCHTOWER_COMMAND_TOPIC, "OK", false, 0);
      Serial.println("[Watchtower] STATUS -> OK");
      return;
    }

    // RESET - Reboot the device
    if (strcmp(message, "RESET") == 0) {
      mqttAdapter.publish(WATCHTOWER_COMMAND_TOPIC, "OK", false, 0);
      mqttAdapter.publish(WATCHTOWER_LOG_TOPIC, "RESET command received - rebooting...", false, 0);
      Serial.println("[Watchtower] RESET -> Rebooting...");
      delay(100);
      ESP.restart();
      return;
    }

    // PUZZLE_RESET - Reset state without rebooting
    if (strcmp(message, "PUZZLE_RESET") == 0) {
      cView.resetLoadedAndFired();

      // Clear the Gravity Games topics
      char loadedTopic[64], firedTopic[64];
      snprintf(loadedTopic, sizeof(loadedTopic), "MermaidsTale/Cannon%dLoaded", config::CANNON_ID);
      snprintf(firedTopic, sizeof(firedTopic), "MermaidsTale/Cannon%dFired", config::CANNON_ID);
      mqttAdapter.publish(loadedTopic, "clear", false, 0);
      mqttAdapter.publish(firedTopic, "clear", false, 0);

      mqttAdapter.publish(WATCHTOWER_COMMAND_TOPIC, "OK", false, 0);
      mqttAdapter.publish(WATCHTOWER_LOG_TOPIC, "PUZZLE_RESET -> State cleared", false, 0);
      Serial.println("[Watchtower] PUZZLE_RESET -> State cleared");
      return;
    }

    // Unknown command
    char logMsg[128];
    snprintf(logMsg, sizeof(logMsg), "Unknown command: %s", message);
    mqttAdapter.publish(WATCHTOWER_LOG_TOPIC, logMsg, false, 0);
    Serial.printf("[Watchtower] %s\n", logMsg);
    return;
  }

  // Handle reset command (legacy)
  if (topicStr == resetTopic && strcmp(message, "true") == 0) {
    Serial.printf("Reset command received for Cannon%d via MQTT\n", config::CANNON_ID);
    resetState = ResetState::PENDING;
    resetStartTime = millis();
  }

  // Handle status request (legacy)
  if (topicStr == statusTopic && strcmp(message, "request") == 0) {
    Serial.printf("Status request received for Cannon%d via MQTT\n", config::CANNON_ID);
    sendStartupStatus();
  }
}

// ============================================================================
// RESET HANDLER (Non-blocking state machine)
// ============================================================================
void handleReset() {
  if (resetState == ResetState::PENDING && millis() - resetStartTime > 100) {
    resetState = ResetState::IN_PROGRESS;

    Serial.printf("Executing sensor reset for Cannon%d...\n", config::CANNON_ID);

    // Build topics for this cannon
    char sensorsTopic[64], resetTopic[64];
    buildCannonTopic(sensorsTopic, sizeof(sensorsTopic), "sensors");
    buildCannonTopic(resetTopic, sizeof(resetTopic), "reset");

    // Reset sensor states
    vl6180xInitialized = false;
    als31300Initialized = false;

    // Reinitialize VL6180X
    if (distanceSensor.begin()) {
      vl6180xInitialized = true;
      Serial.println("VL6180X reset successful");
      mqttAdapter.publish(sensorsTopic, "VL6180X reset OK", false, 0);
    } else {
      Serial.println("VL6180X reset failed");
      mqttAdapter.publish(sensorsTopic, "VL6180X reset failed", false, 0);
    }

    // Reinitialize ALS31300
    uint8_t alsAddr = alsAddressDetected ? detectedALS_ADDR : config::ALS_FALLBACK_ADDR;
    als = ALS31300::Sensor(alsAddr);

    if (als.update()) {
      als31300Initialized = true;
      Serial.println("ALS31300 reset successful");
      mqttAdapter.publish(sensorsTopic, "ALS31300 reset OK", false, 0);
    } else {
      Serial.println("ALS31300 reset failed");
      mqttAdapter.publish(sensorsTopic, "ALS31300 reset failed", false, 0);
    }

    mqttAdapter.publish(resetTopic, "complete", false, 0);
    Serial.println("Reset complete");

    // Send updated status report after reset
    delay(100); // Brief pause to ensure MQTT messages are sent
    sendStartupStatus();

    resetState = ResetState::IDLE;
  }
}

// ============================================================================
// MQTT RECONNECTION HANDLER
// ============================================================================
void handleMqttReconnection() {
  static unsigned long lastMqttCheck = 0;

  if (millis() - lastMqttCheck > config::MQTT_RECONNECT_CHECK_MS) {
    lastMqttCheck = millis();

    if (!mqttAdapter.connected()) {
      Serial.printf("MQTT disconnected for Cannon%d, attempting reconnect...\n", config::CANNON_ID);

      if (mqttAdapter.connect()) {
        // Build topics for this cannon
        char resetTopic[64], statusTopic[64];
        buildCannonTopic(resetTopic, sizeof(resetTopic), "reset");
        buildCannonTopic(statusTopic, sizeof(statusTopic), "status");

        // Resubscribe after reconnection
        mqttAdapter.subscribe(resetTopic, 0);
        mqttAdapter.subscribe(statusTopic, 0);
        mqttAdapter.subscribe(WATCHTOWER_COMMAND_TOPIC, 0);
        Serial.printf("MQTT reconnected for Cannon%d and resubscribed\n", config::CANNON_ID);

        // Republish online status
        mqttAdapter.publish(WATCHTOWER_STATUS_TOPIC, "ONLINE", true, 0);
      } else {
        Serial.println("MQTT reconnection failed");
      }
    }
  }
}

// ============================================================================
// STARTUP STATUS (Fixed string concatenation issues)
// ============================================================================
void sendStartupStatus() {
  Serial.printf("=== Cannon%d Startup Status ===\n", config::CANNON_ID);

  char statusMsg[256];
  char detailedMsg[512];
  int statusLen = 0;
  int detailLen = 0;
  bool allGood = true;

  statusLen += snprintf(statusMsg + statusLen, sizeof(statusMsg) - statusLen,
                        "Cannon%d online - ", config::CANNON_ID);

  // Check WiFi
  if (WiFi.status() == WL_CONNECTED) {
    statusLen += snprintf(statusMsg + statusLen, sizeof(statusMsg) - statusLen, "WiFi ✓ ");
    detailLen += snprintf(detailedMsg + detailLen, sizeof(detailedMsg) - detailLen,
                         "WiFi: Connected to %s (IP: %s) | ",
                         WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
    Serial.printf("✓ WiFi connected to %s (IP: %s)\n",
                  WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
  } else {
    statusLen += snprintf(statusMsg + statusLen, sizeof(statusMsg) - statusLen, "WiFi ✗ ");

    const char* wifiErrorMsg = "Unknown error";
    if (WiFi.status() == WL_NO_SSID_AVAIL) wifiErrorMsg = "Network not found";
    else if (WiFi.status() == WL_CONNECT_FAILED) wifiErrorMsg = "Connection failed";
    else if (WiFi.status() == WL_CONNECTION_LOST) wifiErrorMsg = "Connection lost";

    detailLen += snprintf(detailedMsg + detailLen, sizeof(detailedMsg) - detailLen,
                         "WiFi: Failed - %s | ", wifiErrorMsg);
    Serial.printf("✗ WiFi: %s\n", wifiErrorMsg);
    allGood = false;
  }

  // Check MQTT
  if (mqttAdapter.connected()) {
    statusLen += snprintf(statusMsg + statusLen, sizeof(statusMsg) - statusLen, "MQTT ✓ ");
    detailLen += snprintf(detailedMsg + detailLen, sizeof(detailedMsg) - detailLen,
                         "MQTT: Connected and subscribed | ");
    Serial.println("✓ MQTT connected and ready");
  } else {
    statusLen += snprintf(statusMsg + statusLen, sizeof(statusMsg) - statusLen, "MQTT ✗ ");
    detailLen += snprintf(detailedMsg + detailLen, sizeof(detailedMsg) - detailLen,
                         "MQTT: Disconnected | ");
    Serial.println("✗ MQTT: Disconnected");
    allGood = false;
  }

  // Check VL6180X
  if (vl6180xInitialized) {
    statusLen += snprintf(statusMsg + statusLen, sizeof(statusMsg) - statusLen, "Distance ✓ ");
    detailLen += snprintf(detailedMsg + detailLen, sizeof(detailedMsg) - detailLen,
                         "VL6180X: Online at 0x29 | ");
    Serial.println("✓ VL6180X distance sensor ready");
  } else {
    statusLen += snprintf(statusMsg + statusLen, sizeof(statusMsg) - statusLen, "Distance ✗ ");
    Wire.beginTransmission(0x29);
    uint8_t vl_error = Wire.endTransmission();

    const char* distError = (vl_error != 0)
      ? "Not responding on I2C - Check wiring"
      : "I2C OK but init failed";

    detailLen += snprintf(detailedMsg + detailLen, sizeof(detailedMsg) - detailLen,
                         "VL6180X: %s | ", distError);
    Serial.printf("✗ VL6180X: %s\n", distError);
    allGood = false;
  }

  // Check ALS31300
  if (als31300Initialized) {
    statusLen += snprintf(statusMsg + statusLen, sizeof(statusMsg) - statusLen, "Angle ✓ ");
    uint8_t usedAddr = alsAddressDetected ? detectedALS_ADDR : config::ALS_FALLBACK_ADDR;
    detailLen += snprintf(detailedMsg + detailLen, sizeof(detailedMsg) - detailLen,
                         "ALS31300: Online at 0x%02X | ", usedAddr);
    Serial.printf("✓ ALS31300 angle sensor ready at 0x%02X\n", usedAddr);
  } else {
    statusLen += snprintf(statusMsg + statusLen, sizeof(statusMsg) - statusLen, "Angle ✗ ");
    const char* angleError = alsAddressDetected
      ? "Detected but not responding"
      : "No device detected";

    detailLen += snprintf(detailedMsg + detailLen, sizeof(detailedMsg) - detailLen,
                         "ALS31300: %s | ", angleError);
    Serial.printf("✗ ALS31300: %s\n", angleError);
    allGood = false;
  }

  // Final status
  if (allGood) {
    statusLen += snprintf(statusMsg + statusLen, sizeof(statusMsg) - statusLen,
                         "- Ready to fire! 🎯");
    Serial.println("🎯 All systems ready!");
  } else {
    statusLen += snprintf(statusMsg + statusLen, sizeof(statusMsg) - statusLen,
                         "- Issues detected");
    Serial.println("⚠️ Issues detected");
  }

  // Send to MQTT
  if (mqttAdapter.connected()) {
    char statusTopic[64], diagnosticsTopic[64];
    buildCannonTopic(statusTopic, sizeof(statusTopic), "status");
    buildCannonTopic(diagnosticsTopic, sizeof(diagnosticsTopic), "diagnostics");

    mqttAdapter.publish(statusTopic, statusMsg, true, 0);
    mqttAdapter.publish(diagnosticsTopic, detailedMsg, true, 0);
    Serial.println("Status messages sent via MQTT");
  }

  Serial.println("===============================");
}

// ============================================================================
// I2C SCANNER (Improved ALS detection)
// ============================================================================
void scanI2CDevices() {
  Serial.println("\nScanning I2C bus...");

  char i2cTopic[64];
  buildCannonTopic(i2cTopic, sizeof(i2cTopic), "i2c");
  mqttAdapter.publish(i2cTopic, "Scanning I2C bus...", false, 0);

  int deviceCount = 0;
  alsAddressDetected = false;

  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      char deviceMsg[128];
      int msgLen = snprintf(deviceMsg, sizeof(deviceMsg),
                           "I2C device found at address 0x%02X", address);

      // Identify known devices
      if (address == 0x29) {
        msgLen += snprintf(deviceMsg + msgLen, sizeof(deviceMsg) - msgLen,
                          " (VL6180X)");
      }
      // ALS31300 can be at 0x01 or 0x60-0x6F depending on programming
      else if (address == 0x01 || (address >= 0x60 && address <= 0x6F)) {
        // Try to verify this is actually an ALS31300
        Wire.beginTransmission(address);
        Wire.write(0x00);
        if (Wire.endTransmission() == 0) {
          detectedALS_ADDR = address;
          alsAddressDetected = true;
          msgLen += snprintf(deviceMsg + msgLen, sizeof(deviceMsg) - msgLen,
                            " (ALS31300 detected!)");
          Serial.printf("*** ALS31300 found at address 0x%02X ***\n", address);
        } else {
          msgLen += snprintf(deviceMsg + msgLen, sizeof(deviceMsg) - msgLen,
                            " (Possible ALS31300)");
        }
      }

      Serial.println(deviceMsg);
      mqttAdapter.publish(i2cTopic, deviceMsg, false, 0);
      deviceCount++;
    }
  }

  char resultMsg[128];
  if (deviceCount == 0) {
    snprintf(resultMsg, sizeof(resultMsg),
             "No I2C devices found! Check wiring.");
    Serial.println(resultMsg);
  } else {
    snprintf(resultMsg, sizeof(resultMsg),
             "Found %d I2C device(s)%s", deviceCount,
             alsAddressDetected ? " - ALS31300 detected" : "");
    Serial.println(resultMsg);
  }

  mqttAdapter.publish(i2cTopic, resultMsg, false, 0);
  Serial.println();
}

// ============================================================================
// WATCHTOWER PROTOCOL - Status Report
// ============================================================================
void sendWatchtowerStatus() {
  char status[256];
  snprintf(status, sizeof(status),
    "ONLINE | v%s | VL6180X:%s | ALS31300:%s | Uptime:%lums",
    VERSION,
    vl6180xInitialized ? "OK" : "FAIL",
    als31300Initialized ? "OK" : "FAIL",
    millis()
  );

  mqttAdapter.publish(WATCHTOWER_STATUS_TOPIC, status, true, 0);
  Serial.printf("[Watchtower] %s\n", status);
}

// ============================================================================
// WATCHTOWER PROTOCOL - 5-Minute Heartbeat
// ============================================================================
void watchtowerHeartbeat() {
  if (millis() - lastWatchtowerHeartbeat >= config::WATCHTOWER_HEARTBEAT_MS) {
    lastWatchtowerHeartbeat = millis();
    sendWatchtowerStatus();
  }
}

// ============================================================================
// SETUP
// ============================================================================
void setup() {
  Serial.begin(115200);
  delay(config::STARTUP_SETTLE_MS);
  Serial.printf("Starting Cannon%d System...\n", config::CANNON_ID);

  // Initialize cannonBaseTopic string
  snprintf(cannonBaseTopic, sizeof(cannonBaseTopic), "MermaidsTale/Cannon%d", config::CANNON_ID);

  // Initialize Watchtower Protocol topics
  snprintf(WATCHTOWER_COMMAND_TOPIC, sizeof(WATCHTOWER_COMMAND_TOPIC),
           "MermaidsTale/Cannon%d/command", config::CANNON_ID);
  snprintf(WATCHTOWER_STATUS_TOPIC, sizeof(WATCHTOWER_STATUS_TOPIC),
           "MermaidsTale/Cannon%d/status", config::CANNON_ID);
  snprintf(WATCHTOWER_LOG_TOPIC, sizeof(WATCHTOWER_LOG_TOPIC),
           "MermaidsTale/Cannon%d/log", config::CANNON_ID);

  // Enable watchdog timer
  esp_task_wdt_init(config::WATCHDOG_TIMEOUT_S, true);
  esp_task_wdt_add(NULL);
  Serial.printf("Watchdog timer enabled (%ds timeout)\n", config::WATCHDOG_TIMEOUT_S);

  ctrl.begin();
  delay(100);

  // I2C bus recovery
  Serial.println("Attempting I2C bus recovery...");
  if (ctrl.i2c().clearBus()) {
    Serial.println("I2C bus recovery successful");
  } else {
    Serial.println("I2C bus recovery failed - continuing anyway");
  }

  I2CBus::setActive(&ctrl.i2c());
  ALS31300::Sensor::setCallbacks(
      I2CBus::cbRegisterDevice, I2CBus::cbUnregisterDevice,
      I2CBus::cbChangeAddress, I2CBus::cbWrite, I2CBus::cbRead);

  // Scan I2C bus
  scanI2CDevices();

  // Connect to WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(cfg::WIFI_SSID, cfg::WIFI_PASS);
  Serial.print("Connecting to WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println(" Connected!");

  // Connect to MQTT
  mqtt::Config mqttConfig;
  mqttConfig.brokerHost = cfg::MQTT_HOST;
  mqttConfig.brokerPort = cfg::MQTT_PORT;

  // Generate dynamic client ID: "cannon-1", "cannon-2", etc.
  static char clientId[32];
  snprintf(clientId, sizeof(clientId), "cannon-%d", config::CANNON_ID);
  mqttConfig.clientId = clientId;

  mqttAdapter.begin(mqttConfig);
  mqttAdapter.connect();
  mqttAdapter.loop();

  if (mqttAdapter.connected()) {
    Serial.println("MQTT connected");

    // Build topics for this cannon
    char resetTopic[64], statusTopic[64];
    buildCannonTopic(resetTopic, sizeof(resetTopic), "reset");
    buildCannonTopic(statusTopic, sizeof(statusTopic), "status");

    pubSubClient.setCallback(onMqttMessage);
    mqttAdapter.subscribe(resetTopic, 0);
    mqttAdapter.subscribe(statusTopic, 0);
    mqttAdapter.subscribe(WATCHTOWER_COMMAND_TOPIC, 0);
    Serial.printf("Subscribed to Cannon%d reset, status, and command topics\n", config::CANNON_ID);

    // Publish ONLINE status
    mqttAdapter.publish(WATCHTOWER_STATUS_TOPIC, "ONLINE", true, 0);
  } else {
    Serial.println("MQTT not connected");
    Serial.printf("Broker: %s:%d\n", mqtt::Config().brokerHost, mqtt::Config().brokerPort);
  }

  // Initialize VL6180X
  Serial.println("\n=== VL6180X Initialization ===");
  Serial.println("Checking for VL6180X at address 0x29...");

  Wire.beginTransmission(0x29);
  uint8_t vl_error = Wire.endTransmission();

  if (vl_error == 0) {
    Serial.println("VL6180X detected on I2C bus!");
    if (!distanceSensor.begin()) {
      Serial.println("VL6180X detected but initialization failed!");
      vl6180xInitialized = false;
    } else {
      Serial.println("VL6180X initialized successfully!");
      vl6180xInitialized = true;
    }
  } else {
    Serial.printf("ERROR: VL6180X not responding (I2C error: %d)\n", vl_error);
    Serial.printf("Check wiring: SDA=%d, SCL=%d, 3.3V, GND\n",
                  config::I2C_SDA_PIN, config::I2C_SCL_PIN);
    vl6180xInitialized = false;
  }

  // Initialize ALS31300
  Serial.println("\n=== ALS31300 Initialization ===");

  if (alsAddressDetected) {
    Serial.printf("Using detected ALS31300 at address 0x%02X\n", detectedALS_ADDR);
    als = ALS31300::Sensor(detectedALS_ADDR);

    if (als.update()) {
      Serial.println("ALS31300 initialized successfully!");
      als31300Initialized = true;
    } else {
      Serial.println("ALS31300 detected but update failed!");
      als31300Initialized = false;
    }
  } else {
    Serial.printf("No ALS31300 detected. Trying fallback address 0x%02X\n",
                  config::ALS_FALLBACK_ADDR);

    if (als.update()) {
      Serial.println("ALS31300 initialized with fallback address!");
      als31300Initialized = true;
      detectedALS_ADDR = config::ALS_FALLBACK_ADDR;
      alsAddressDetected = true;
    } else {
      Serial.println("ERROR: No ALS31300 found at any address!");
      als31300Initialized = false;
    }
  }

  Serial.println("Setup complete");

  delay(config::STARTUP_SETTLE_MS);
  sendStartupStatus();

  // Initialize Watchtower heartbeat timer and send initial status
  lastWatchtowerHeartbeat = millis();
  sendWatchtowerStatus();
  mqttAdapter.publish(WATCHTOWER_LOG_TOPIC, "Boot complete", false, 0);
  Serial.printf("[Watchtower] Cannon%d ready - v%s\n", config::CANNON_ID, VERSION);
}

// ============================================================================
// MAIN LOOP
// ============================================================================
void loop() {
  // Feed the watchdog
  esp_task_wdt_reset();

  static unsigned long lastStatus = 0;
  static float filteredDistance = 0;
  static bool firstReading = true;

  // MQTT maintenance
  mqttAdapter.loop();
  handleMqttReconnection();
  handleReset();
  watchtowerHeartbeat();

  ctrl.pollButton();

  // Read distance sensor
  uint8_t mm = 0;
  uint8_t stat = VL6180X_ERROR_NONE;
  static uint8_t lastDistanceError = VL6180X_ERROR_NONE;

  if (vl6180xInitialized) {
    mm = distanceSensor.readRange();
    stat = distanceSensor.readRangeStatus();

    // Apply distance filtering
    if (stat == VL6180X_ERROR_NONE) {
      if (firstReading) {
        filteredDistance = mm;
      } else {
        filteredDistance = filteredDistance * (1.0f - config::DISTANCE_FILTER_ALPHA)
                         + mm * config::DISTANCE_FILTER_ALPHA;
      }
    }

    // Log error status changes (ignore known non-critical errors)
    if (stat != lastDistanceError) {
      if (stat == VL6180X_ERROR_NONE) {
        Serial.printf("VL6180X OK - Distance: %dmm\n", (int)filteredDistance);
      } else if (stat != config::VL6180X_ERR_ECE_FAIL &&
                 stat != config::VL6180X_ERR_VCSEL_WD) {
        Serial.printf("VL6180X Error %d - Distance: %dmm\n", stat, mm);
      }
      lastDistanceError = stat;
    }
  }

  // Update ALS sensor - use raw angle (no filtering)
  static bool lastAlsStatus = true;
  bool currentAlsStatus = als31300Initialized ? als.update() : false;

  float rawAngle = 0;
  if (currentAlsStatus) {
    rawAngle = als.getAngle();  // Raw angle, no filtering
    if (firstReading) {
      firstReading = false;
    }
  }

  // Log ALS status changes
  if (currentAlsStatus != lastAlsStatus) {
    if (currentAlsStatus) {
      Serial.printf("ALS31300 OK - Angle: %d°\n", (int)rawAngle);
    } else {
      Serial.println("ALS31300 read error occurred");
    }
    lastAlsStatus = currentAlsStatus;
  }

  uint16_t deg = (uint16_t)rawAngle;
  gstate.update(millis(), deg, ctrl.button().pressed(),
                (uint8_t)filteredDistance, stat == VL6180X_ERROR_NONE);

  // Publish only significant changes
  static int lastPublishedAngle = -1;
  static uint8_t lastPublishedDistance = 255;
  static bool lastButtonState = false;

  uint32_t changed = cView.update();
  int currentAngle = (int)rawAngle;
  uint8_t currentDistance = (uint8_t)filteredDistance;
  bool currentButton = ctrl.button().pressed();

  // Publish angle changes
  if (changed & cannon::ChangedAngle) {
    if (abs(currentAngle - lastPublishedAngle) >= config::MIN_ANGLE_CHANGE_DEG) {
      cannonPub.publishAngle(config::CANNON_ID, cView.angleDeg());
      Serial.printf("MQTT: Published angle %d° for Cannon%d\n", (int)cView.angleDeg(), config::CANNON_ID);
      lastPublishedAngle = currentAngle;
    }
  }

  // Log distance changes
  if (vl6180xInitialized && stat == VL6180X_ERROR_NONE) {
    if (abs(currentDistance - lastPublishedDistance) >= config::MIN_DISTANCE_CHANGE_MM) {
      Serial.printf("Distance changed: %dmm\n", currentDistance);
      lastPublishedDistance = currentDistance;
    }
  }

  // Log button changes
  if (currentButton != lastButtonState) {
    Serial.println(currentButton ? "*** BUTTON PRESSED ***" : "*** Button Released ***");
    lastButtonState = currentButton;
  }

  // Publish events
  if (changed & cannon::ChangedLoaded) {
    if (cView.justLoaded()) {
      cannonPub.publishEvent(config::CANNON_ID, "Loaded");
      Serial.printf("MQTT: Published Loaded event for Cannon%d (distance: %dmm)\n",
                    config::CANNON_ID, currentDistance);
    }
  }
  if (changed & cannon::ChangedFired) {
    if (cView.justFired()) {
      cannonPub.publishEvent(config::CANNON_ID, "Fired");
      Serial.printf("MQTT: Published Fired event for Cannon%d\n", config::CANNON_ID);
    }
    // Reset loaded and fired flags after firing to allow new cycle
    cView.resetLoadedAndFired();

    // Wait 2 seconds before clearing MQTT to give game time to process
    delay(2000);

    // Clear the Loaded and Fired states in MQTT by publishing empty/clear messages
    char loadedTopic[64], firedTopic[64];
    snprintf(loadedTopic, sizeof(loadedTopic), "MermaidsTale/Cannon%dLoaded", config::CANNON_ID);
    snprintf(firedTopic, sizeof(firedTopic), "MermaidsTale/Cannon%dFired", config::CANNON_ID);
    mqttAdapter.publish(loadedTopic, "clear", false, 0);
    mqttAdapter.publish(firedTopic, "clear", false, 0);

    Serial.printf("Cannon%d: Reset loaded/fired flags and cleared MQTT\n", config::CANNON_ID);
  }

  // Periodic status report
  if (millis() - lastStatus > config::STATUS_REPORT_INTERVAL_MS) {
    lastStatus = millis();
    Serial.printf("Status - VL6180X: %s | ALS31300: %s | MQTT: %s | Angle: %d° | Distance: %dmm\n",
                  (vl6180xInitialized && stat == VL6180X_ERROR_NONE) ? "OK" : "Error",
                  (als31300Initialized && currentAlsStatus) ? "OK" : "Error",
                  mqttAdapter.connected() ? "Connected" : "Disconnected",
                  (int)rawAngle,
                  (int)filteredDistance);
  }

  delay(50);
}
