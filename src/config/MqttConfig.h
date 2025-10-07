#pragma once

#include <cstdint>

namespace cfg {
  // Choose the right host per build
  /*
  inline constexpr char WIFI_SSID[]   = "Rcarroll";
  inline constexpr char WIFI_PASS[]   = "GiantS01!#";
  inline constexpr char MQTT_HOST[]   = "192.168.1.145";
*/

  inline constexpr char WIFI_SSID[]   = "AlchemyGuest";
  inline constexpr char WIFI_PASS[]   = "VoodooVacation5601";
  inline constexpr char MQTT_HOST[]   = "10.1.10.115";
  
  inline constexpr uint16_t MQTT_PORT = 1883;
  inline constexpr char MQTT_USER[]   = "";
  inline constexpr char MQTT_PASSW[]  = "";
  
  // NOTE: The CLIENT_ID is now dynamically set in main.cpp based on config::CANNON_ID
  // It will be: "cannon-{id}" (e.g., "cannon-1", "cannon-2", etc.)
  // If you need a different format, modify the mqtt::Config in main.cpp setup()
  
//  inline constexpr char WIFI_SSID[]   = "EscapeRoomNet";
//  inline constexpr char WIFI_PASS[]   = "supersecret";
}
