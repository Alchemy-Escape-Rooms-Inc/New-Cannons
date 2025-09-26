#pragma once
#include <cstddef>
#include <cstdio>
#include <cmath>
#include "boardkit.hpp"

namespace integ
{

  // Let you route topic assembly through your MqttTopic helper
  using TopicBuilderFn = int (*)(char *out, std::size_t cap,
                                 const char *base, const char *leaf);

  inline int defaultJoin(char *out, std::size_t cap,
                         const char *base, const char *leaf)
  {
    return std::snprintf(out, cap, "%s/%s", base, leaf);
  }

  class CannonTelemetry
  {
  public:
    CannonTelemetry(mqtt::IMqttClient &client,
                    const char *base = "MermaidsTale",
                    TopicBuilderFn builder = &defaultJoin)
        : client_(client), base_(base), build_(builder) {}

    void publishAngle(uint8_t cannonId, float angleDeg)
    {
      const int n = normalize360(angleDeg);
      char leaf[24];
      std::snprintf(leaf, sizeof(leaf), "Cannon%dHor", cannonId); // Fixed: use actual cannonId

      char topic[96];
      if (build_(topic, sizeof(topic), base_, leaf) <= 0)
        return;

      char payload[16];
      std::snprintf(payload, sizeof(payload), "pre_%d", n); // game expects "pre_<deg>"
      client_.publish(topic, payload, /*retain=*/false, /*qos=*/0);
    }

    void publishEvent(uint8_t cannonId, const char *which /* "Loaded" or "Fired" */)
    {
      char leaf[32];
      std::snprintf(leaf, sizeof(leaf), "Cannon%d%s", cannonId, which); // Fixed: use actual cannonId

      char topic[96];
      if (build_(topic, sizeof(topic), base_, leaf) <= 0)
        return;

      client_.publish(topic, "triggered", /*retain=*/false, /*qos=*/0); // game expects "triggered"
    }

  private:
    static int normalize360(float deg)
    {
      float m = std::fmod(deg, 360.0f);
      if (m < 0)
        m += 360.0f;
      return static_cast<int>(std::lround(m));
    }

    mqtt::IMqttClient &client_;
    const char *base_;
    TopicBuilderFn build_;
  };
} // namespace integ