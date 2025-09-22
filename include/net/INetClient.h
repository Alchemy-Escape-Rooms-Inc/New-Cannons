#pragma once
#include <stdint.h>
#include <stddef.h>

namespace net {

/** Minimal client API your app / MQTT needs. */
class INetClient {
public:
  virtual ~INetClient() = default;

  virtual bool    connect(const char* host, uint16_t port) = 0;
  virtual bool    connected() const = 0;
  virtual void    stop() = 0;

  virtual size_t  write(const uint8_t* buf, size_t len) = 0;
  virtual int     read(uint8_t* buf, size_t len) = 0;   // return bytes read or -1
  virtual int     available() const = 0;
  virtual void    setTimeout(unsigned long ms) = 0;
};

} // namespace net
