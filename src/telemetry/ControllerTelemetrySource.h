#pragma once
#include <cstddef>
#include "state/ControllerState.h"
#include "features/telemetry/TelemetrySource.h"

class ControllerTelemetrySource : public ITelemetrySource {
public:
  explicit ControllerTelemetrySource(ctl::State& s) : s_(s) {}

  bool buildDeltaJson(char* out, std::size_t cap) override {
    const auto changed = s_.lastChangeMask();
    if (!changed) return false;
    const auto n = s_.toDeltaJson(out, cap, changed);
    safeTerm(out, cap, n);
    return n > 0;
  }

  std::size_t buildSnapshotJson(char* out, std::size_t cap) override {
    const auto n = s_.toJson(out, cap);
    safeTerm(out, cap, n);
    return n;
  }

private:
  ctl::State& s_;
  static void safeTerm(char* out, std::size_t cap, std::size_t n) {
    if (cap) out[(n < cap) ? n : cap - 1] = '\0';
  }
};
