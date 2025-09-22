#pragma once
#include "net/INetClient.h"
#include <WiFi.h>  // or <WiFiClient.h> depending on core

namespace net {
class ArduinoWiFiClientAdapter : public INetClient {
public:
  explicit ArduinoWiFiClientAdapter(WiFiClient& c) : c_(c) {}
  bool connect(const char* host, uint16_t port) override { return c_.connect(host, port); }
  bool connected() const override { return c_.connected(); }
  void stop() override { c_.stop(); }
  size_t write(const uint8_t* b, size_t n) override { return c_.write(b, n); }
  int read(uint8_t* b, size_t n) override { return c_.read(b, n); }
  int available() const override { return c_.available(); }
  void setTimeout(unsigned long ms) override { c_.setTimeout(ms); }
private: WiFiClient& c_;
};
} // namespace net
