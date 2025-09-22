#pragma once
/**
 * @file ArduinoPubSubClientAdapter.h
 * @brief Adapter that implements IMqttClient using PubSubClient on Arduino/ESP32.
 *
 * Requires:
 *   - Arduino core
 *   - PubSubClient library
 *   - A connected TCP client (WiFiClient or WiFiClientSecure)
 */

#ifdef ARDUINO

#include <Arduino.h>
#include <PubSubClient.h>
#include <functional>
#include "protocols/mqtt/MqttClient.h"

class ArduinoPubSubClientAdapter : public mqtt::IMqttClient {
public:
  // Ctor takes a reference to an already-existing PubSubClient instance.
  // You construct PubSubClient with a connected WiFiClient/WiFiClientSecure.
  explicit ArduinoPubSubClientAdapter(PubSubClient& client)
  : client_(client) {}

  bool begin(const mqtt::Config& cfg) override {
    cfg_ = cfg;
    client_.setServer(cfg_.brokerHost, cfg_.brokerPort);
    // Bridge PubSubClient callback to our std::function
    client_.setCallback([this](char* topic, byte* payload, unsigned int length){
      if (onMessage_) onMessage_(topic, payload, static_cast<size_t>(length));
    });
    client_.setKeepAlive(cfg_.keepAliveS);
    return true;
  }

  bool connect() override {
    if (connected()) return true;
    // Note: cleanSession is default in PubSubClient; no explicit flag.
    bool ok = false;
    if (cfg_.username && cfg_.password) {
      ok = client_.connect(cfg_.clientId, cfg_.username, cfg_.password);
    } else {
      ok = client_.connect(cfg_.clientId);
    }
    return ok;
  }

  void loop() override {
    client_.loop();
  }

  bool connected() const override {
    return client_.connected();
  }

  void disconnect() override {
    client_.disconnect();
  }

  bool publish(const char* topic, const char* payload, bool retain, int /*qos*/) override {
    // PubSubClient supports QoS 0 only; qos is ignored here.
    return client_.publish(topic, payload, retain);
  }

  bool subscribe(const char* topicFilter, int /*qos*/) override {
    // PubSubClient supports QoS 0 only; qos is ignored here.
    return client_.subscribe(topicFilter);
  }

  void onMessage(mqtt::MessageHandler handler) override {
    onMessage_ = std::move(handler);
  }

private:
  mqtt::Config cfg_{};
  PubSubClient& client_;
  mqtt::MessageHandler onMessage_{};
};

#endif // ARDUINO
