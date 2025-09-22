#pragma once
#include <cstddef>

// A minimal "provider" that can produce telemetry payloads as JSON.
// No project-specific types here. Implement this near your machine code.
struct ITelemetrySource {
  virtual ~ITelemetrySource() = default;

  // Write only changed fields into out[0..cap), return true if anything changed/payload written.
  virtual bool buildDeltaJson(char* out, size_t cap) = 0;

  // Write full snapshot into out[0..cap), return bytes written.
  virtual size_t buildSnapshotJson(char* out, size_t cap) = 0;
};
