/**
 * ============================================================================
 *  MqttConfig.h — BRIDGE FILE
 * ============================================================================
 *
 *  This file is a pass-through. It re-exports values from MANIFEST.h under
 *  the cfg:: namespace so that main.cpp doesn't need any code changes.
 *
 *  Every value defined here comes from MANIFEST.h — the single source of
 *  truth. Do NOT hardcode values in this file. If you need to change the
 *  broker IP, WiFi credentials, or any other network setting, change it
 *  in MANIFEST.h and this file picks it up automatically.
 *
 *  This bridge exists so the refactor can be done safely:
 *    - main.cpp still references cfg::WIFI_SSID, cfg::MQTT_HOST, etc.
 *    - Those names now resolve to manifest:: values via this file.
 *    - Zero lines changed in main.cpp, zero risk to runtime behavior.
 *
 *  Once you're confident everything works, you can optionally remove this
 *  bridge and reference manifest:: directly in main.cpp. But there's no
 *  rush — this file costs nothing at compile time.
 * ============================================================================
 */

#pragma once

#include "MANIFEST.h"

namespace cfg {
  // All values sourced from MANIFEST.h — do not hardcode here
  inline constexpr const char* WIFI_SSID  = manifest::WIFI_SSID;   // → AlchemyGuest
  inline constexpr const char* WIFI_PASS  = manifest::WIFI_PASS;   // → VoodooVacation5601
  inline constexpr const char* MQTT_HOST  = manifest::MQTT_HOST;   // → 10.1.10.115
  inline constexpr uint16_t    MQTT_PORT  = manifest::MQTT_PORT;   // → 1883
  inline constexpr const char* MQTT_USER  = manifest::MQTT_USER;   // → ""
  inline constexpr const char* MQTT_PASSW = manifest::MQTT_PASSW;  // → ""

  // CLIENT_ID is built dynamically in main.cpp: "cannon-{manifest::CANNON_ID}"
}
