#include "EthernetManager.h"

// ============================================================================
// Constructor from BoardPins structures
// ============================================================================
EthernetManager::EthernetManager(const BoardPins::SPI& spi,
                                 const BoardPins::EthDev& eth,
                                 const byte mac[6],
                                 EthStaticCfg staticCfg)
: staticCfg_(staticCfg) {
  memcpy(mac_, mac, 6);
  pins_.sclk = spi.sck;
  pins_.miso = spi.miso;
  pins_.mosi = spi.mosi;
  pins_.cs = eth.cs;
  pins_.rst = eth.rst;
}

// ============================================================================
// Private helper: Reset W5500 chip via RST pin
// ============================================================================
void EthernetManager::resetChip_() {
  if (pins_.rst < 0) return;
  pinMode(pins_.rst, OUTPUT);
  digitalWrite(pins_.rst, LOW);
  delay(5);
  digitalWrite(pins_.rst, HIGH);
  delay(50);
}

// ============================================================================
// Private helper: Start DHCP with timeout
// ============================================================================
bool EthernetManager::startDhcp_(unsigned long timeoutMs) {
  const unsigned long start = millis();
  while ((millis() - start) < timeoutMs) {
    if (Ethernet.begin(mac_) != 0) return true; // got a lease
    delay(250);
  }
  return false;
}

// ============================================================================
// Private helper: Start with static configuration
// ============================================================================
bool EthernetManager::startStatic_() {
  if (!staticCfg_.isValid()) return false;
  Ethernet.begin(mac_, staticCfg_.ip, staticCfg_.dns, staticCfg_.gw, staticCfg_.mask);
  return Ethernet.localIP() == staticCfg_.ip;
}

// ============================================================================
// Initialize Ethernet with DHCP (fallback to static if provided)
// ============================================================================
bool EthernetManager::begin(unsigned long dhcpTimeoutMs) {
  // Bind SPI to your pins and tell Ethernet which CS to use
  SPI.begin(pins_.sclk, pins_.miso, pins_.mosi, pins_.cs);
  Ethernet.init(pins_.cs);
  resetChip_();

  bool ok = startDhcp_(dhcpTimeoutMs);
  if (!ok) ok = startStatic_();

  if (ok) {
    Serial.print(F("[ETH] IP: ")); Serial.println(Ethernet.localIP());
  } else {
    Serial.println(F("[ETH] Failed to obtain IP (DHCP + static fallback)."));
  }
  lastLinkUp_ = (Ethernet.linkStatus() == LinkON);
  return ok;
}

// ============================================================================
// Maintain DHCP lease and monitor link status
// ============================================================================
void EthernetManager::loop() {
  // Maintain DHCP lease (1=renewed, 2=rebound)
  const int m = Ethernet.maintain();
  if (m == 1) {
    Serial.print(F("[ETH] DHCP renewed: ")); Serial.println(Ethernet.localIP());
  } else if (m == 2) {
    Serial.print(F("[ETH] DHCP rebound: ")); Serial.println(Ethernet.localIP());
  }

  // Edge-detect link changes
  const bool linkUp = (Ethernet.linkStatus() == LinkON);
  if (linkUp != lastLinkUp_) {
    lastLinkUp_ = linkUp;
    Serial.println(linkUp ? F("[ETH] Link UP") : F("[ETH] Link DOWN"));
  }
}
