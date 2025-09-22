#include <Adafruit_VL6180X.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <Wire.h>
// #include <Ethernet.h>


#include <Arduino.h>
#include "boardkit.hpp"

#include "config/MqttConfig.h"
#include "state/CannonStateView.h"
#include "state/ControllerState.h"
#include "telemetry/CannonTelemetry.h"
#include "telemetry/ControllerTelemetrySource.h"






// Pick safe pins for your ESP32-S3 board:
static constexpr int BUTTON_PIN = 35;  // example input-capable GPIO
static constexpr uint8_t ALS_ADDR = 0x65;
int counter = 0;

Adafruit_VL6180X distanceSensor = Adafruit_VL6180X();
bool vl6180xInitialized = false;
bool als31300Initialized = false;
ALS31300::Sensor als(ALS_ADDR); 
Controller ctrl(BoardPins::DevKitS3_DefaultI2C(15, 18, 100000U), BUTTON_PIN, Pull::Up, ActivePolarity::ActiveLow, 20);
ctl::State gstate;
ControllerTelemetrySource tSource(gstate);
WiFiClient wifiClient;
PubSubClient pubSubClient(wifiClient);
ArduinoPubSubClientAdapter mqttAdapter(pubSubClient);

static float getAngleDeg(const ctl::State& s){ return s.getAngleDeg(); }
static bool  getLoaded (const ctl::State& s){ return s.getLoaded();   }
static bool  getFired  (const ctl::State& s){ return s.getFired();    }

cannon::StateView<ctl::State> cView(gstate, &getAngleDeg, &getLoaded, &getFired);

telem::TelemetryConfig tcfg{
  "escape/room1/puzzleA",   // base
  "evt/state",              // stateEvt
  "evt/changes",            // deltaEvt
  true,                     // retainState
  0                         // qos
};
telem::TelemetryPublisher tPub(mqttAdapter, tSource, tcfg);

integ::CannonTelemetry cannonPub(mqttAdapter, "MermaidsTale");


// Map heading (0..359) to an 8-point compass label.
static const char* compass8(uint16_t deg) {
  // sector size = 360/8 = 45°, center sectors by adding half-sector (22.5)
  static const char* const labels[8] = {"N","NE","E","SE","S","SW","W","NW"};
  uint16_t idx = (uint16_t)((deg + 22U) / 45U) % 8U;
  return labels[idx];
}

// I2C Scanner to detect connected devices
void scanI2CDevices() {
  Serial.println("\nScanning I2C bus...");
  int deviceCount = 0;

  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    uint8_t error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("I2C device found at address 0x");
      if (address < 16) Serial.print("0");
      Serial.print(address, HEX);

      // Identify known devices
      if (address == 0x29) Serial.print(" (VL6180X default)");
      else if (address == 0x60) Serial.print(" (ALS31300)");

      Serial.println();
      deviceCount++;
    }
  }

  if (deviceCount == 0) {
    Serial.println("No I2C devices found!");
    Serial.println("Check wiring and pull-up resistors.");
  } else {
    Serial.print("Found ");
    Serial.print(deviceCount);
    Serial.println(" I2C device(s).");
  }
  Serial.println();
}


void setup() {
  Serial.begin(115200);
  delay(1000); // Wait for serial monitor
  Serial.println("Starting Cannon3 System...");

  ctrl.begin();
  delay(100); // Allow I2C bus to stabilize

  // Try to recover I2C bus if stuck
  Serial.println("Attempting I2C bus recovery...");
  if (ctrl.i2c().clearBus()) {
    Serial.println("I2C bus recovery successful");
  } else {
    Serial.println("I2C bus recovery failed - continuing anyway");
  }

  I2CBus::setActive(&ctrl.i2c());
  ALS31300::Sensor::setCallbacks(
      I2CBus::cbRegisterDevice, I2CBus::cbUnregisterDevice,
      I2CBus::cbChangeAddress,  I2CBus::cbWrite, I2CBus::cbRead);

  // Run I2C scan to diagnose connected devices
  scanI2CDevices();

  WiFi.mode(WIFI_STA);
  //WiFi.begin("AlchemyGuest", "VoodooVacation5601");
  WiFi.begin(cfg::WIFI_SSID,cfg::WIFI_PASS);

  while(WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  mqttAdapter.begin(mqtt::Config());
  mqttAdapter.connect();
  mqttAdapter.loop();
  

  if(mqttAdapter.connected()) {
    Serial.println("MQTT connected");
    /*
    mqttAdapter.publish(
      "escape/room1/puzzleA/evt/test",
      "{\"hello\":1}",
      false,
      0
    );
    */
  } else {
    Serial.println("MQTT not connected");
    Serial.printf("Broker host: %s\n", mqtt::Config().brokerHost);
    Serial.printf("Broker port: %d\n", mqtt::Config().brokerPort);
  }

  // Initialize VL6180X distance sensor with error handling
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
    Serial.println("ERROR: VL6180X not responding on I2C!");
    Serial.printf("I2C error code: %d\n", vl_error);
    Serial.println("Check wiring: SDA=15, SCL=18, 3.3V, GND");
    vl6180xInitialized = false;
  }

  // Initialize ALS31300 sensor
  Serial.println("\n=== ALS31300 Initialization ===");
  Serial.printf("Checking ALS31300 at address 0x%02X...\n", ALS_ADDR);

  Wire.beginTransmission(ALS_ADDR);
  uint8_t als_error = Wire.endTransmission();

  if (als_error == 0) {
    Serial.println("ALS31300 detected on I2C bus!");
    if (!als.update()) {
      Serial.println("ALS31300 detected but update failed!");
      als31300Initialized = false;
    } else {
      Serial.println("ALS31300 initialized successfully!");
      als31300Initialized = true;
    }
  } else {
    Serial.println("ERROR: ALS31300 not responding on I2C!");
    Serial.printf("I2C error code: %d\n", als_error);
    als31300Initialized = false;
  }

  Serial.println("Setup complete");
}

void loop() {
  static unsigned long lastStatus = 0;
  static float filteredAngle = 0;
  static float filteredDistance = 0;
  static bool firstReading = true;

  mqttAdapter.loop();

  char * out = new char[128];
  ctrl.pollButton();

  // Read distance with error checking
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
        // Low-pass filter: smooth out noise
        filteredDistance = filteredDistance * 0.8 + mm * 0.2;
      }
    }

    // Only print if error status changes (but hide errors 6 and 11)
    if (stat != lastDistanceError) {
      if (stat == VL6180X_ERROR_NONE) {
        Serial.print("VL6180X OK - Distance: ");
        Serial.print((int)filteredDistance);
        Serial.println("mm");
      } else if (stat != 6 && stat != 11) {
        Serial.print("VL6180X Error ");
        Serial.print(stat);
        Serial.print(" - Distance: ");
        Serial.print(mm);
        Serial.println("mm");
      }
      lastDistanceError = stat;
    }
  }

  // Update ALS sensor with error checking
  static bool lastAlsStatus = true;
  bool currentAlsStatus = als31300Initialized ? als.update() : false;

  // Apply angle filtering
  if (currentAlsStatus) {
    float currentAngle = als.getAngle();
    if (firstReading) {
      filteredAngle = currentAngle;
      firstReading = false;
    } else {
      // Handle angle wraparound (359° -> 0°)
      float angleDiff = currentAngle - filteredAngle;
      if (angleDiff > 180) angleDiff -= 360;
      if (angleDiff < -180) angleDiff += 360;

      // Only update if change is reasonable (< 10° per reading)
      if (abs(angleDiff) < 10) {
        filteredAngle = filteredAngle + angleDiff * 0.3; // 30% new, 70% old
        if (filteredAngle < 0) filteredAngle += 360;
        if (filteredAngle >= 360) filteredAngle -= 360;
      }
    }
  }

  // Only print if ALS status changes
  if (currentAlsStatus != lastAlsStatus) {
    if (currentAlsStatus) {
      Serial.print("ALS31300 OK - Angle: ");
      Serial.print((int)filteredAngle);
      Serial.println("°");
    } else {
      Serial.println("ALS31300 read error occurred");
    }
    lastAlsStatus = currentAlsStatus;
  }

  uint16_t deg = (uint16_t)filteredAngle;

  gstate.update(millis(), deg, ctrl.button().pressed(), (uint8_t)filteredDistance, stat == VL6180X_ERROR_NONE);
  
  delete[] out;

   // gstate.toJson( out, 128);
   // Serial.println(out);

  // Only publish and print if values actually changed significantly
  static int lastPublishedAngle = -1;
  static uint8_t lastPublishedDistance = 255;
  static bool lastButtonState = false;

  uint32_t changed = cView.update();
  int currentAngle = (int)filteredAngle;
  uint8_t currentDistance = (uint8_t)filteredDistance;
  bool currentButton = ctrl.button().pressed();

  // Publish angle only if it changed by more than 1 degree
  if (changed & cannon::ChangedAngle) {
    if (abs(currentAngle - lastPublishedAngle) >= 1) {
      cannonPub.publishAngle(2, cView.angleDeg());
      Serial.print("MQTT: Published angle ");
      Serial.println(cView.angleDeg());
      lastPublishedAngle = currentAngle;
    }
  }

  // Publish distance only if it changed by more than 2mm
  if (vl6180xInitialized && stat == VL6180X_ERROR_NONE) {
    if (abs(currentDistance - lastPublishedDistance) >= 2) {
      Serial.print("Distance changed: ");
      Serial.print(currentDistance);
      Serial.println("mm");
      lastPublishedDistance = currentDistance;
    }
  }

  // Print button changes
  if (currentButton != lastButtonState) {
    if (currentButton) {
      Serial.println("*** BUTTON PRESSED ***");
    } else {
      Serial.println("*** Button Released ***");
    }
    lastButtonState = currentButton;
  }

  if (changed & cannon::ChangedLoaded) {
    if (cView.justLoaded()) {
      cannonPub.publishEvent(2, "Loaded");
      Serial.println("MQTT: Published Loaded event");
    }
  }
  if (changed & cannon::ChangedFired) {
    if (cView.justFired()) {
      cannonPub.publishEvent(2, "Fired");
      Serial.println("MQTT: Published Fired event");
    }
  }

  // Print status every 5 seconds
  if (millis() - lastStatus > 5000) {
    lastStatus = millis();
    Serial.print("Status - VL6180X: ");
    if (vl6180xInitialized && stat == VL6180X_ERROR_NONE) {
      Serial.print((int)filteredDistance);
      Serial.print("mm");
    } else {
      Serial.print("Error");
    }
    Serial.print(" | ALS31300: ");
    if (als31300Initialized && currentAlsStatus) {
      Serial.print((int)filteredAngle);
      Serial.print("°");
    } else {
      Serial.print("Error");
    }
    Serial.print(" | MQTT: ");
    Serial.println(mqttAdapter.connected() ? "Connected" : "Disconnected");
  }

  //  mqttAdapter.publish("Captain", out, true, 0);
   // Serial.println("Published");

  delay(50); // small poll interval; debouncer handles timing
}
