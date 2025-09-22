#include <Adafruit_VL6180X.h>
#include <Wire.h>
#include <WiFi.h>

// Pin definitions
static constexpr int SDA_PIN = 15;
static constexpr int SCL_PIN = 18;
static constexpr int BUTTON_PIN = 35;
static constexpr uint8_t ALS_ADDR = 0x65;

// Global objects
Adafruit_VL6180X distanceSensor = Adafruit_VL6180X();
bool vl6180xInitialized = false;
bool als31300Initialized = false;

// WiFi credentials
const char* WIFI_SSID = "AlchemyGuest";
const char* WIFI_PASS = "VoodooVacation5601";

// Simple ALS31300 reading
bool readALS31300(uint8_t address, float &x, float &y, float &z) {
  // Read register 0x28
  Wire.beginTransmission(address);
  Wire.write(0x28);
  Wire.endTransmission(false);

  if (Wire.requestFrom((int)address, 4) != 4) return false;

  uint32_t reg28 = 0;
  for (int i = 0; i < 4; i++) {
    reg28 = (reg28 << 8) | Wire.read();
  }

  // Read register 0x29
  Wire.beginTransmission(address);
  Wire.write(0x29);
  Wire.endTransmission(false);

  if (Wire.requestFrom((int)address, 4) != 4) return false;

  uint32_t reg29 = 0;
  for (int i = 0; i < 4; i++) {
    reg29 = (reg29 << 8) | Wire.read();
  }

  // Extract X, Y, Z values
  uint16_t newX = ((reg28 >> 16) & 0xFF) << 8 | ((reg29 >> 24) & 0xFF);
  uint16_t newY = ((reg28 >> 8) & 0xFF) << 8 | ((reg29 >> 16) & 0xFF);
  uint16_t newZ = (reg28 & 0xFF) << 8 | ((reg29 >> 8) & 0xFF);

  x = (float)((int16_t)newX);
  y = (float)((int16_t)newY);
  z = (float)((int16_t)newZ);

  return true;
}

float calculateAngle(float x, float y) {
  float angle = atan2(y, x) * 180.0 / PI;
  if (angle < 0) angle += 360.0;
  return angle;
}

// I2C Scanner
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
      else if (address == 0x60) Serial.print(" (ALS31300 expected at 0x60)");
      else if (address == 0x65) Serial.print(" (ALS31300 actual)");

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
  delay(1000);
  Serial.println("Starting Cannon3 Simple System...");

  // Initialize I2C
  Wire.begin(SDA_PIN, SCL_PIN, 100000);
  delay(100);

  // Scan I2C devices
  scanI2CDevices();

  // Initialize WiFi
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int attempts = 0;
  while(WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed - continuing without network");
  }

  // Initialize VL6180X
  Serial.println("\n=== VL6180X Initialization ===");
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
    Serial.println("ERROR: VL6180X not responding!");
    vl6180xInitialized = false;
  }

  // Check ALS31300
  Serial.println("\n=== ALS31300 Check ===");
  Wire.beginTransmission(ALS_ADDR);
  uint8_t als_error = Wire.endTransmission();

  if (als_error == 0) {
    Serial.println("ALS31300 detected on I2C bus!");
    als31300Initialized = true;
  } else {
    Serial.println("ERROR: ALS31300 not responding!");
    als31300Initialized = false;
  }

  // Initialize button
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  Serial.println("\nSetup complete!\n");
  Serial.println("Data format: Distance(mm) | Angle(deg) | Button | X | Y | Z");
  Serial.println("-------------------------------------------------------------");
}

void loop() {
  static unsigned long lastPrint = 0;
  static uint8_t lastValidDistance = 0;
  static float lastValidAngle = 0;
  static bool lastButtonState = false;
  static bool lastSensorOK = false;

  // Read button
  bool buttonPressed = (digitalRead(BUTTON_PIN) == LOW);

  // Read distance sensor
  uint8_t mm = 0;
  uint8_t stat = VL6180X_ERROR_NONE;
  bool sensorOK = false;

  if (vl6180xInitialized) {
    mm = distanceSensor.readRange();
    stat = distanceSensor.readRangeStatus();
    sensorOK = (stat == VL6180X_ERROR_NONE);

    // Only notify when sensor status changes significantly
    if (sensorOK != lastSensorOK) {
      if (sensorOK) {
        Serial.println("VL6180X sensor recovered");
      } else {
        Serial.println("VL6180X sensor error - readings may be unreliable");
      }
      lastSensorOK = sensorOK;
    }
  }

  // Read ALS31300
  float x = 0, y = 0, z = 0;
  float angle = 0;

  if (als31300Initialized) {
    if (readALS31300(ALS_ADDR, x, y, z)) {
      angle = calculateAngle(x, y);
    } else {
      als31300Initialized = false;
      Serial.println("ALS31300 read failed");
    }
  }

  // Only print when values change significantly
  bool distanceChanged = false;
  bool angleChanged = false;

  if (sensorOK && abs((int)mm - (int)lastValidDistance) > 5) {
    distanceChanged = true;
    lastValidDistance = mm;
  }

  if (als31300Initialized && abs(angle - lastValidAngle) > 2.0) {
    angleChanged = true;
    lastValidAngle = angle;
  }

  // Print only when something meaningful changes
  if (distanceChanged || angleChanged || (millis() - lastPrint > 5000)) {
    lastPrint = millis();

    Serial.print("Distance: ");
    if (sensorOK) {
      Serial.print(mm);
      Serial.print("mm");
    } else {
      Serial.print("ERROR");
    }

    if (als31300Initialized) {
      Serial.print(" | Angle: ");
      Serial.print(angle, 1);
      Serial.print("°");
    }

    Serial.print(" | Button: ");
    Serial.print(buttonPressed ? "PRESSED" : "released");

    Serial.println();
  }

  // Print button state changes
  if (buttonPressed != lastButtonState) {
    if (buttonPressed) {
      Serial.println("\n*** BUTTON PRESSED! ***\n");
    } else {
      Serial.println("\n*** Button Released ***\n");
    }
    lastButtonState = buttonPressed;
  }

  delay(50);
}