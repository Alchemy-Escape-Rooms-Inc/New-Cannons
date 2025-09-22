#pragma once
/**
 * @file ControllerState.h
 * @brief Snapshot of all relevant controller/sensor data + change tracking.
 *
 * - No Arduino deps. Pure C++.
 * - Zero heap: caller passes output buffers.
 * - Designed to publish either full snapshot or "deltas" over MQTT.
 * - You decide what "present" means for your distance sensor (threshold or valid flag).
 */

#include <cstdint>
#include <cstdio>
#include <cmath>

namespace ctl {

/** Small epsilon for float comparisons (degrees). */
constexpr float kAngleEpsDeg = 0.25f;

/** Snapshot of current readings. Extend as needed. */
struct Snapshot {
  uint32_t tsMs            = 0;    // timestamp (millis)
  float    angleDeg        = 0.0f; // ALS31300
  bool     buttonPressed   = false;// DebouncedButton
  uint16_t distanceMm      = 0;    // VL6180X (0..~200 mm typical)
  bool     targetPresent   = false;// Derived or sensor-provided "seen" flag
};

/** Bit flags for what changed between last and current snapshot. */
enum ChangeFlags : uint32_t {
  ChangedNone       = 0,
  ChangedAngle      = 1u << 0,
  ChangedButton     = 1u << 1,
  ChangedDistance   = 1u << 2,
  ChangedPresence   = 1u << 3,
  ChangedTimeOnly   = 1u << 4, // heartbeat (time progressed, data same)
};

/** State object that holds last and current snapshot + change mask. */
class State {
public:
  State() = default;

  /** Set thresholds/policies without coupling to sensors. */
  void setAngleEpsilon(float deg) { angleEpsDeg_ = deg; }
  void setPresenceDistanceThreshold(uint16_t mm) { presenceThresholdMm_ = mm; }
  void setHeartbeatMs(uint32_t ms) { heartbeatMs_ = ms; }

  /** Update from new raw readings. Returns change mask. */
  uint32_t update(uint32_t tsMs,
                  float    angleDeg,
                  bool     buttonPressed,
                  uint16_t distanceMm,
                  bool     distanceValid /* if your driver reports validity */)
  {
    last_ = now_; // keep previous snapshot
    now_.tsMs          = tsMs;
    now_.angleDeg      = angleDeg;
    now_.buttonPressed = buttonPressed;
    now_.distanceMm    = distanceMm;

    // Presence policy: either use validity, or derive from threshold.
    if (distanceValid) {
      now_.targetPresent = true;
    } else {
      // If not valid, fallback to threshold rule (you can invert this if preferred)
      now_.targetPresent = (presenceThresholdMm_ > 0) && (distanceMm > 0) && (distanceMm <= presenceThresholdMm_);
    }

    // Compute changes
    uint32_t mask = ChangedNone;
    if (std::fabs(now_.angleDeg - last_.angleDeg) > angleEpsDeg_) mask |= ChangedAngle;
    if (now_.buttonPressed != last_.buttonPressed)                 mask |= ChangedButton;
    if (now_.distanceMm    != last_.distanceMm)                    mask |= ChangedDistance;
    if (now_.targetPresent != last_.targetPresent)                 mask |= ChangedPresence;

    // Heartbeat (time changed but no data changes)
    if (mask == ChangedNone && heartbeatMs_ > 0) {
      if (now_.tsMs - lastHeartbeat_ >= heartbeatMs_) {
        lastHeartbeat_ = now_.tsMs;
        mask |= ChangedTimeOnly;
      }
    }

    lastChangeMask_ = mask;
    return mask;
  }

  const Snapshot& current() const { return now_; }
  const Snapshot& previous() const { return last_; }
  uint32_t lastChangeMask() const { return lastChangeMask_; }

  /**
   * Serialize as compact JSON (retained “state” message).
   * Example: {"t":12345,"ang":12.5,"btn":1,"dist":87,"prs":1}
   * Returns true if fully written.
   */
  bool toJson(char* out, size_t cap) const {
    if (!out || cap == 0) return false;
    int n = std::snprintf(out, cap,
      "{\"t\":%lu,\"ang\":%.2f,\"btn\":%d,\"dist\":%u,\"prs\":%d}",
      static_cast<unsigned long>(now_.tsMs),
      static_cast<double>(now_.angleDeg),
      now_.buttonPressed ? 1 : 0,
      static_cast<unsigned>(now_.distanceMm),
      now_.targetPresent ? 1 : 0);
    return n > 0 && static_cast<size_t>(n) < cap;
  }

  /**
   * Serialize only changed fields (“delta” message). Always includes ts.
   * Example (angle & button changed):
   *   {"t":12345,"ang":12.5,"btn":0}
   */
  bool toDeltaJson(char* out, size_t cap, uint32_t changeMask) const {
    if (!out || cap == 0) return false;
    int n = std::snprintf(out, cap, "{\"t\":%lu", static_cast<unsigned long>(now_.tsMs));
    if (n <= 0 || static_cast<size_t>(n) >= cap) return false;
    size_t len = static_cast<size_t>(n);

    auto addField = [&](const char* key, const char* fmt, double v)->bool {
      int m = std::snprintf(out + len, cap - len, ",\"%s\":", key);
      if (m <= 0 || len + (size_t)m >= cap) return false;
      len += (size_t)m;
      m = std::snprintf(out + len, cap - len, fmt, v);
      if (m <= 0 || len + (size_t)m >= cap) return false;
      len += (size_t)m;
      return true;
    };

    if (changeMask & ChangedAngle)    { if (!addField("ang",  "%.2f", static_cast<double>(now_.angleDeg))) return false; }
    if (changeMask & ChangedButton)   { if (!addField("btn",  "%d",   now_.buttonPressed ? 1.0 : 0.0))     return false; }
    if (changeMask & ChangedDistance) { if (!addField("dist", "%u",   static_cast<double>(now_.distanceMm))) return false; }
    if (changeMask & ChangedPresence) { if (!addField("prs",  "%d",   now_.targetPresent ? 1.0 : 0.0))     return false; }

    // finalize
    if (len + 2 >= cap) return false;
    out[len++] = '}';
    out[len]   = '\0';
    return true;
  }

  float getAngleDeg() const { return now_.angleDeg; }
  bool getLoaded() const { return now_.targetPresent; }
  bool getFired() const { return now_.buttonPressed; }

private:
  Snapshot now_{};
  Snapshot last_{};
  uint32_t lastChangeMask_ = ChangedNone;

  float    angleEpsDeg_        = kAngleEpsDeg;
  uint16_t presenceThresholdMm_ = 120;     // you can tune or set to 0 to disable
  uint32_t heartbeatMs_         = 2000;    // publish time-only heartbeat every 2s
  uint32_t lastHeartbeat_       = 0;
};

} // namespace ctl
