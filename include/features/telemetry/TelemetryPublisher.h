#pragma once

#include <cstddef>
#include <cstdio>
#include "protocols/mqtt/MqttClient.h"          // your IMqttClient abstraction
#include "features/telemetry/TelemetrySource.h"

namespace telem {

struct TelemetryConfig {
  const char* base;      // e.g. "escape/room1/puzzleA"
  const char* stateEvt;  // e.g. "evt/state"
  const char* deltaEvt;  // e.g. "evt/changes"
  bool retainState = true;
  int  qos = 0;
};

class TelemetryPublisher {
public:
  TelemetryPublisher(mqtt::IMqttClient& client,
                     ITelemetrySource&   source,
                     const TelemetryConfig& cfg) noexcept
  : client_(client), source_(source), cfg_(cfg) {}

  bool publishDeltas() {
    char topic[128]; if (!join(topic, sizeof(topic), cfg_.base, cfg_.deltaEvt)) return false;
    char payload[256];
    if (!source_.buildDeltaJson(payload, sizeof(payload))) return false; // nothing changed
    return client_.publish(topic, payload, /*retain=*/false, cfg_.qos);
  }

  bool publishSnapshot() {
    char topic[128]; if (!join(topic, sizeof(topic), cfg_.base, cfg_.stateEvt)) return false;
    char payload[256];
    size_t n = source_.buildSnapshotJson(payload, sizeof(payload));
    safeTerm(payload, sizeof(payload), n);
    return client_.publish(topic, payload, cfg_.retainState, cfg_.qos);
  }

private:
  mqtt::IMqttClient&  client_;
  ITelemetrySource&   source_;
  TelemetryConfig     cfg_;

  static bool join(char* out, size_t cap, const char* a, const char* b) {
    int n = std::snprintf(out, cap, "%s/%s", a, b);
    return n > 0 && (size_t)n < cap;
  }
  static void safeTerm(char* out, size_t cap, size_t n) {
    if (cap) out[(n < cap) ? n : cap - 1] = '\0';
  }
};

} // namespace telem
