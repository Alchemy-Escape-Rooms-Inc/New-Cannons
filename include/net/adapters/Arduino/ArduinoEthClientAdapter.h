#pragma once
#include "net/INetClient.h"
#include <Ethernet.h>   // Arduino lib

namespace net {

class ArduinoEthClientAdapter : public INetClient {
public:
  explicit ArduinoEthClientAdapter(EthernetClient& c) : c_(c) {}

  bool connect(const char* host, uint16_t port) override { return c_.connect(host, port); }
  bool connected() const override { return c_.connected(); }
  void stop() override { c_.stop(); }

  size_t write(const uint8_t* buf, size_t len) override { return c_.write(buf, len); }
  int read(uint8_t* buf, size_t len) override { return c_.read(buf, len); }
  int available() const override { return c_.available(); }
  void setTimeout(unsigned long ms) override { c_.setTimeout(ms); }

private:
  EthernetClient& c_;
};

} // namespace net
