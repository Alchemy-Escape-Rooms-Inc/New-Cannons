#pragma once
#include "net/INetClient.h"

/**
 * @brief INetClient implementation using BSD/POSIX sockets.
 *        Works on Linux/macOS and ESP-IDF (lwIP) with the same API.
 */
class PosixSocketClient : public net::INetClient {
public:
  PosixSocketClient();
  ~PosixSocketClient() override;

  bool    connect(const char* host, uint16_t port) override;
  bool    connected() const override;
  void    stop() override;

  size_t  write(const uint8_t* buf, size_t len) override;
  int     read(uint8_t* buf, size_t len) override;   // returns bytes or -1 on error
  int     available() const override;                // bytes ready to read (0 if none)
  void    setTimeout(unsigned long ms) override;     // affects connect/recv/send

private:
  int            fd_;
  unsigned long  timeout_ms_;
  bool           is_connected_;
  bool           setBlockingTimeouts_() const;
};
