/**
 * ============================================================================
 *  ALCHEMY ESCAPE ROOMS — FIRMWARE MANIFEST
 * ============================================================================
 *
 *  THIS FILE IS THE SINGLE SOURCE OF TRUTH.
 *
 *  It serves two masters simultaneously:
 *
 *    1. THE COMPILER — Every constant the firmware needs (pins, IPs, ports,
 *       thresholds, timing) is defined here as real C++ code. The firmware
 *       #includes this file and uses these values directly.
 *
 *    2. THE GRIMOIRE PARSER — A Python script running on M3 at 6 AM reads
 *       this file as plain text and extracts values tagged with @FIELD_NAME
 *       in the comments. Those values populate the WatchTower Grimoire
 *       device registry, wiring reference, and operations manual.
 *
 *  Because both systems read from the same lines, the documentation can
 *  never drift from the firmware. Change a pin number here, and the Grimoire
 *  updates automatically. There is no second file to keep in sync.
 *
 *  RULES:
 *    1. Every field marked [REQUIRED] must be filled in before deployment.
 *    2. Update this file FIRST when changing hardware, pins, or topics.
 *    3. The 6 AM parser looks for @TAG patterns — don't rename them.
 *    4. Descriptive-only sections (operations, quirks) are pure comments.
 *       Constants sections are real code + comment tags on the same line.
 *    5. This file replaces MqttConfig.h — do not duplicate values elsewhere.
 *
 *  LAST UPDATED: 2026-02-12
 *  MANIFEST VERSION: 2.0
 * ============================================================================
 */

#pragma once
#include <cstdint>

// ============================================================================
//  SECTION 1 — IDENTITY
// ============================================================================
//
// @MANIFEST:IDENTITY
// @PROP_NAME:        Cannon{ID}
// @PROP_PATTERN:     Cannon1, Cannon2
// @INSTANCE_COUNT:   2
//
// @DESCRIPTION:      Physical cannon prop that players rotate to aim at enemy
//                    pirate ships displayed via projectors in the Gravity Games
//                    VR world. An ALS31300 hall effect sensor detects the
//                    cannon's rotation angle and publishes it via MQTT. Players
//                    load a cannonball that rolls past a VL6180X time-of-flight
//                    sensor, triggering a Loaded status. A rope pull activates
//                    the fire button, triggering a Fired status. The Gravity
//                    Games VR software consumes these MQTT topics to render the
//                    cannonball firing on screen. The ESP32 is responsible only
//                    for the physical prop and MQTT output — all visual and
//                    audio reactions are handled by the VR system.
//
// @ROOM:             Pirate Ship
// @BOARD:            ESP32-S3-DevKitC-1
// @FRAMEWORK:        Arduino (PlatformIO)
// @REPO:             https://github.com/Alchemy-Escape-Rooms-Inc/New-Cannons
// @BUILD_STATUS:     INSTALLED
// @CODE_HEALTH:      EXCELLENT
// @WATCHTOWER:       FULLY_COMPLIANT
// @END:IDENTITY

namespace manifest {

// ── Device Identity ─────────────────────────────────────────────────────────
// The CANNON_ID determines all dynamic topic names and client IDs.
// Change ONLY this value when flashing a different cannon.
inline constexpr uint8_t CANNON_ID = 1;                           // @INSTANCE_CONFIG  ← CHANGE PER CANNON

inline constexpr char FIRMWARE_VERSION[] = "3.2.0";               // @FIRMWARE_VERSION


// ============================================================================
//  SECTION 2 — NETWORK CONFIGURATION
// ============================================================================
// @MANIFEST:NETWORK

// ── WiFi ────────────────────────────────────────────────────────────────────
inline constexpr char WIFI_SSID[]   = "AlchemyGuest";             // @WIFI_SSID
inline constexpr char WIFI_PASS[]   = "VoodooVacation5601";       // @WIFI_PASS

// ── MQTT Broker ─────────────────────────────────────────────────────────────
inline constexpr char MQTT_HOST[]   = "10.1.10.115";              // @BROKER_IP
inline constexpr uint16_t MQTT_PORT = 1883;                       // @BROKER_PORT
inline constexpr char MQTT_USER[]   = "";                         // @MQTT_USER
inline constexpr char MQTT_PASSW[]  = "";                         // @MQTT_PASS
// Client ID is built dynamically: "cannon-{CANNON_ID}" (e.g., "cannon-2")

// ── Heartbeat ───────────────────────────────────────────────────────────────
inline constexpr uint32_t WATCHTOWER_HEARTBEAT_MS = 300000;       // @HEARTBEAT_MS  (5 minutes)

//  ── TOPIC MAP ──────────────────────────────────────────────────────────────
//  Topics are built dynamically from CANNON_ID at runtime. This map documents
//  every topic the device uses. The parser reads these tags; the firmware
//  builds the actual strings in setup() using snprintf.
//
//  SUBSCRIPTIONS:
//  @SUBSCRIBE:  MermaidsTale/Cannon{ID}/command    | WatchTower protocol commands
//  @SUBSCRIBE:  MermaidsTale/Cannon{ID}/reset      | Legacy reset trigger (payload: "true")
//  @SUBSCRIBE:  MermaidsTale/Cannon{ID}/status     | Legacy status request (payload: "request")
//
//  PUBLICATIONS:
//  @PUBLISH:  MermaidsTale/Cannon{ID}/command      | PONG responses             | retain:no
//  @PUBLISH:  MermaidsTale/Cannon{ID}/status       | Startup status summary     | retain:yes
//  @PUBLISH:  MermaidsTale/Cannon{ID}/log          | WatchTower log messages    | retain:no
//  @PUBLISH:  MermaidsTale/Cannon{ID}/diagnostics  | Detailed diagnostic report | retain:yes
//  @PUBLISH:  MermaidsTale/Cannon{ID}/sensors      | Sensor reset status        | retain:no
//  @PUBLISH:  MermaidsTale/Cannon{ID}/i2c          | I2C bus scan results       | retain:no
//  @PUBLISH:  MermaidsTale/Cannon{ID}Hor           | Angle degrees (pre_{deg})  | retain:no  | LEGACY (no slash)
//  @PUBLISH:  MermaidsTale/Cannon{ID}Loaded        | "triggered" on cannonball  | retain:no  | LEGACY (no slash)
//  @PUBLISH:  MermaidsTale/Cannon{ID}Fired         | "triggered" on rope pull   | retain:no  | LEGACY (no slash)
//
//  NOTE: The last 3 topics use legacy format (Cannon2Hor, not Cannon2/Hor)
//        for Gravity Games VR compatibility. Do NOT "fix" this.
//
//  SUPPORTED COMMANDS (via /command topic):
//  @COMMAND:  PING          | Responds PONG on /command topic         | Health check
//  @COMMAND:  STATUS        | Sends full report on /status topic      | Responds OK
//  @COMMAND:  RESET         | Reboots ESP32 via ESP.restart()         | Responds OK then reboots
//  @COMMAND:  PUZZLE_RESET  | Clears loaded/fired state, no reboot   | Clears MQTT retained msgs
//
// @END:NETWORK


// ============================================================================
//  SECTION 3 — PIN CONFIGURATION
// ============================================================================
// @MANIFEST:PINS

inline constexpr int I2C_SDA_PIN = 15;                            // @PIN:SDA    | Shared bus: VL6180X + ALS31300
inline constexpr int I2C_SCL_PIN = 18;                            // @PIN:SCL    | Clock at 100kHz
inline constexpr int BUTTON_PIN  = 35;                            // @PIN:BUTTON | Pull-up, active-low, rope pull trigger

inline constexpr uint32_t I2C_FREQUENCY = 100000U;                // @I2C_FREQ   | 100kHz standard mode
inline constexpr int BUTTON_DEBOUNCE_MS = 20;                     // @DEBOUNCE   | Button debounce time

// @END:PINS


// ============================================================================
//  SECTION 4 — I2C DEVICES
// ============================================================================
// @MANIFEST:I2C

inline constexpr uint8_t ALS_FALLBACK_ADDR = 0x01;                // @I2C_DEVICE:ALS31300 | Hall effect angle sensor | Custom driver
// ALS31300 auto-detects across 0x01 and 0x60-0x6F on boot.
// Each cannon may have a different ALS address depending on sensor programming.

// VL6180X is at fixed address 0x29 (hardcoded in Adafruit library)
// @I2C_DEVICE:VL6180X | 0x29 | Time-of-flight distance sensor | Adafruit_VL6180X

// @END:I2C


// ============================================================================
//  SECTION 5 — SENSOR TUNING & THRESHOLDS
// ============================================================================
// @MANIFEST:TUNING

// ── Distance Sensor (VL6180X) ───────────────────────────────────────────────
inline constexpr float DISTANCE_FILTER_ALPHA = 0.2f;              // @FILTER:DISTANCE  | EMA: 20% new, 80% old
inline constexpr uint8_t MIN_DISTANCE_CHANGE_MM = 2;              // @THRESHOLD:DISTANCE | Publish only if ≥2mm change

// ── Angle Sensor (ALS31300) ─────────────────────────────────────────────────
inline constexpr float ANGLE_FILTER_ALPHA = 0.3f;                 // @FILTER:ANGLE    | EMA: 30% new, 70% old
inline constexpr float MAX_ANGLE_JUMP_DEG = 10.0f;                // @THRESHOLD:ANGLE_MAX | Reject unrealistic jumps
inline constexpr int MIN_ANGLE_CHANGE_DEG = 1;                    // @THRESHOLD:ANGLE_MIN | Publish only if ≥1° change

// Physical-to-virtual angle amplification: real-world cannon has limited
// pivot range, so we scale deviations from rest to make Unreal cannon
// move farther than the physical one. Output wraps via normalize360.
inline constexpr float ANGLE_REST_DEG       = 0.0f;               // @CALIB:ANGLE_REST | Center/rest angle of physical cannon (tune in field)
inline constexpr float ANGLE_AMPLIFICATION  = -3.0f;              // @CALIB:ANGLE_GAIN | 1° physical = N° virtual (Unreal); sign reverses direction

// ── Presence Detection ──────────────────────────────────────────────────────
// (Presence threshold is set in ControllerState.h: presenceThresholdMm_ = 150)
// @THRESHOLD:PRESENCE | 150mm | Cannonball must be within 150mm to register as "loaded"

// ── VL6180X Error Codes (from datasheet, used for filtering) ────────────────
inline constexpr uint8_t VL6180X_ERR_ECE_FAIL = 6;                // @SENSOR_ERR | ECE check failed (non-critical)
inline constexpr uint8_t VL6180X_ERR_VCSEL_WD = 11;               // @SENSOR_ERR | VCSEL watchdog timeout (non-critical)

// @END:TUNING


// ============================================================================
//  SECTION 6 — TIMING CONSTANTS
// ============================================================================
// @MANIFEST:TIMING

inline constexpr uint32_t STATUS_REPORT_INTERVAL_MS = 5000;       // @TIMING:STATUS    | Serial status print every 5s
inline constexpr uint32_t STARTUP_SETTLE_MS = 1000;                // @TIMING:SETTLE    | Delay on boot before init
inline constexpr uint32_t MQTT_RECONNECT_CHECK_MS = 5000;         // @TIMING:RECONNECT | Check MQTT connection every 5s
inline constexpr uint32_t WATCHDOG_TIMEOUT_S = 10;                 // @TIMING:WATCHDOG  | Reboot if loop stalls 10s

// @END:TIMING

} // namespace manifest


// ============================================================================
//  SECTION 7 — COMPONENTS
// ============================================================================
//
// @MANIFEST:COMPONENTS
//
// @COMPONENT:  VL6180X Time-of-Flight Sensor
//   @PURPOSE:  Detects cannonball loaded into barrel
//   @DETAIL:   Presence threshold 150mm. Ball rolls past sensor and drops onto
//              shelf for reuse (self-resetting mechanic). Uses EMA filtering
//              (alpha 0.2). Reports errors including ECE fail and VCSEL watchdog.
//
// @COMPONENT:  ALS31300 Hall Effect Sensor
//   @PURPOSE:  Detects cannon rotation angle via magnet
//   @DETAIL:   Reports 0-359 degrees. Angle changes below 1 degree are
//              suppressed to prevent MQTT spam. Auto-address detection on boot.
//
// @COMPONENT:  Momentary Push Button (Rope Pull)
//   @PURPOSE:  Fire trigger — players pull a rope which activates the button
//   @DETAIL:   Pull-up resistor, active-low logic, 20ms debounce.
//
// @COMPONENT:  ESP32 Hardware Watchdog
//   @PURPOSE:  Auto-reboot on firmware hang
//   @DETAIL:   10-second timeout. If loop() stalls, device reboots
//              automatically. Fed every loop iteration via esp_task_wdt_reset().
//
// @EXTERNAL_SYSTEMS:
//   Gravity Games VR — Consumes angle, loaded, and fired MQTT topics.
//                      Handles all visual rendering (cannonball trajectory,
//                      ship hits, explosions) and sound effects. Not controlled
//                      by this firmware.
//
// @END:COMPONENTS


// ============================================================================
//  SECTION 8 — OPERATIONS
// ============================================================================
//
// @MANIFEST:OPERATIONS
//
//  ── RESET PROCEDURES ───────────────────────────────────────────────────────
//
// @RESET:SOFTWARE
//   Send "RESET" to MermaidsTale/Cannon{ID}/command
//   Device responds "OK", then reboots via ESP.restart()
//   Reconnects WiFi, reconnects MQTT, re-initializes all sensors
//   Publishes full startup status on /status and /diagnostics topics
//   Expected recovery time: 10-15 seconds
//
// @RESET:SENSOR
//   Send "true" to MermaidsTale/Cannon{ID}/reset
//   Re-initializes VL6180X and ALS31300 without full reboot
//   Reports per-sensor success/failure on /sensors topic
//   Use when a single sensor is misbehaving but MQTT is still connected
//
// @RESET:PUZZLE
//   Send "PUZZLE_RESET" to MermaidsTale/Cannon{ID}/command
//   Clears loaded/fired state flags and MQTT retained messages
//   No reboot, no sensor re-initialization
//   Use between game sessions to reset cannon to ready state
//
// @RESET:HARDWARE
//   The ESP32 is located inside the base of the cannon. Power is supplied
//   through the floor into the cannon base. Access requires removing screws
//   from a panel on the back of the base. Disconnect and reconnect the power
//   source to force a full power cycle. After power-on, monitor the MQTT
//   /status topic — the device should publish ONLINE within 10-15 seconds.
//   On serial monitor, look for the full startup status sequence confirming
//   WiFi, MQTT, VL6180X, and ALS31300 initialization.
//
// @RESET:WATCHDOG
//   Automatic — if loop() stalls for 10+ seconds, the ESP32 hardware
//   watchdog triggers a reboot without human intervention. Check /log topic
//   for boot messages if you suspect watchdog reboots are occurring.
//
//  ── TEST PROCEDURE ─────────────────────────────────────────────────────────
//
// @TEST:STEP1  Send PING to /command → expect PONG (confirms MQTT)
// @TEST:STEP2  Send STATUS to /command → expect full report on /status (confirms sensors)
// @TEST:STEP3  Place object within 150mm of VL6180X → expect "triggered" on Cannon{ID}Loaded
// @TEST:STEP4  Press fire button (pull rope) → expect "triggered" on Cannon{ID}Fired
// @TEST:STEP5  Wait 2 seconds after fire → expect "clear" on both Loaded and Fired topics
// @TEST:STEP6  Check /i2c topic after boot → should show both 0x29 and ALS address
// @TEST:STEP7  Rotate cannon slowly → expect angle updates on Cannon{ID}Hor (pre_{degrees})
//
//  ── KNOWN QUIRKS ───────────────────────────────────────────────────────────
//
// @QUIRK:ALS_DRIFT
//   Repeated cannon rotation may cause the ALS31300 sensor to shift from its
//   mounted position above the magnet, resulting in erratic or zero angle
//   readings. If angle data stops making sense but the device is otherwise
//   healthy (WiFi, MQTT, VL6180X all OK), check the physical alignment of
//   the ALS31300 relative to the magnet before troubleshooting firmware.
//
// @QUIRK:FIRE_DELAY
//   After firing, there is a hard 2-second delay (delay(2000) in loop) before
//   the loaded/fired MQTT states are cleared. During this window the main loop
//   is blocked. The watchdog is safe (10s timeout > 2s delay) but no sensor
//   readings or MQTT messages are processed during this pause.
//
// @QUIRK:LEGACY_TOPICS
//   Game integration topics (Cannon{ID}Hor, Cannon{ID}Loaded, Cannon{ID}Fired)
//   use a legacy format with no slash separator. This is intentional for
//   Gravity Games compatibility. Do not "fix" this formatting.
//
// @QUIRK:DEV_WIFI
//   MqttConfig.h previously contained a commented-out alternate WiFi config
//   for the "Rcarroll" development network (192.168.1.145). Now that all
//   network config lives in MANIFEST.h, ensure no stale MqttConfig.h
//   overrides exist. The active config should always point to
//   AlchemyGuest / 10.1.10.115.
//
// @END:OPERATIONS


// ============================================================================
//  SECTION 9 — DEPENDENCIES
// ============================================================================
//
// @MANIFEST:DEPENDENCIES
//
// @LIB:  Adafruit_VL6180X        | VL6180X driver         | GitHub (adafruit)
// @LIB:  Adafruit BusIO          | I2C abstraction         | v1.16.1+
// @LIB:  Adafruit Unified Sensor | Sensor framework        | v1.1.14+
// @LIB:  PubSubClient            | MQTT client             | v2.8+
// @LIB:  VL53L0X (pololu)        | Included but unused     | May be removable
// @LIB:  Ethernet                | Included but unused     | v2.0.2+
//
// @INTERNAL:  boardkit.hpp        | Alchemy board abstraction layer
// @INTERNAL:  als31300.cpp        | Custom ALS31300 driver (src/sensors/allegro/)
//
// @END:DEPENDENCIES


// ============================================================================
//  SECTION 10 — WIRING SUMMARY
// ============================================================================
//
// @MANIFEST:WIRING
//
//   ESP32-S3 Pin 15 (SDA) ──┬── VL6180X SDA
//                            └── ALS31300 SDA
//
//   ESP32-S3 Pin 18 (SCL) ──┬── VL6180X SCL
//                            └── ALS31300 SCL
//
//   ESP32-S3 Pin 35 ─────────── Fire Button (rope pull, to GND)
//
//   ESP32-S3 3.3V ───────────┬── VL6180X VCC
//                             └── ALS31300 VCC
//
//   ESP32-S3 GND ────────────┬── VL6180X GND
//                             ├── ALS31300 GND
//                             └── Fire Button (other terminal)
//
//   Power: USB through floor into cannon base
//   Access: Screw panel on back of cannon base
//
// @END:WIRING
