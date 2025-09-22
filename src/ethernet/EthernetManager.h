#pragma once
/**
 * EthernetManager
 * - W5500 bring-up for Arduino framework (SPI pins, CS, optional RST)
 * - DHCP with optional static fallback
 * - Link-state logging + DHCP lease maintenance
 *
 * This file is framework-specific (Arduino). Keep your portable headers in /include.
 */
#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>
#include "board/pins.h"

struct EthPins {
  int sclk;   // SPI SCLK
  int miso;   // SPI MISO
  int mosi;   // SPI MOSI
  int cs;     // W5500 CS
  int rst;    // W5500 RST (or -1 if not wired)
};

struct EthStaticCfg {
  IPAddress ip, dns, gw, mask;
  bool isValid() const {
    return ip != INADDR_NONE && gw != INADDR_NONE && mask != INADDR_NONE;
  }
};

// help me write


class EthernetManager {
public:
  EthernetManager(const EthPins& pins,
                  const byte mac[6],
                  EthStaticCfg staticCfg = {})
  : pins_(pins), staticCfg_(staticCfg) {
    memcpy(mac_, mac, 6);
  }

    // NEW: build EthPins from board SPI + EthDev and delegate
  EthernetManager(const BoardPins::SPI& spi,
                  const BoardPins::EthDev& eth,
                  const byte mac[6],
                  EthStaticCfg staticCfg = {});

  /**
   * Initialize SPI + W5500, then start DHCP (fallback to static if provided).
   * @return true if an IP was obtained.
   */
  bool begin(unsigned long dhcpTimeoutMs = 8000UL);

  /** Call periodically: maintains DHCP lease, logs link up/down. */
  void loop();

  /** True if link is up and local IP is valid. */
  bool isUp() const {
    return (Ethernet.linkStatus() == LinkON) && (Ethernet.localIP() != INADDR_NONE);
  }

  IPAddress localIP() const { return Ethernet.localIP(); }

private:
  void resetChip_();
  bool startDhcp_(unsigned long timeoutMs);
  bool startStatic_();

  EthPins pins_;
  byte mac_[6];
  EthStaticCfg staticCfg_;
  bool lastLinkUp_ = false;
};
