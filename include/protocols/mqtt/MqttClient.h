#pragma once
/**
 * @file MqttClient.h
 * @brief Portable MQTT client interface (no Arduino dependencies).
 *
 * Usage:
 *   - Implementations (adapters) wrap a concrete library (e.g., PubSubClient).
 *   - Your app only talks to IMqttClient.
 */

#include <cstdint>
#include <cstddef>
#include <functional>

namespace mqtt {

struct Config {
  const char* brokerHost = "10.1.10.115";
//  const char* brokerHost = "192.168.1.145";
  uint16_t    brokerPort = 1883;
  const char* clientId   = "controller-01";
  const char* username   = nullptr;   // optional
  const char* password   = nullptr;   // optional
  bool        useTLS     = false;     // adapter may ignore if not supported
  uint16_t    keepAliveS = 30;        // seconds
  bool        cleanSession = true;
};

using MessageHandler = std::function<void(const char* topic,
                                          const uint8_t* payload,
                                          size_t len)>;

class IMqttClient {
public:
  virtual ~IMqttClient() = default;

  /** Prepare the client; does not open the socket. */
  virtual bool begin(const Config& cfg) = 0;

  /** Open the connection to the broker and perform MQTT CONNECT. */
  virtual bool connect() = 0;

  /** Service I/O; call frequently from your main loop. */
  virtual void loop() = 0;

  /** True if the MQTT session is established. */
  virtual bool connected() const = 0;

  /** Disconnect gracefully (if supported). */
  virtual void disconnect() = 0;

  /** Publish a UTF-8 payload to topic. QoS 0/1 allowed if backend supports. */
  virtual bool publish(const char* topic,
                       const char* payload,
                       bool retain = false,
                       int qos = 0) = 0;

  /** Subscribe to a topic filter (e.g., "room/+/cmd"). QoS 0/1 allowed. */
  virtual bool subscribe(const char* topicFilter, int qos = 0) = 0;

  /** Set inbound message callback. */
  virtual void onMessage(MessageHandler handler) = 0;
};

} // namespace mqtt
