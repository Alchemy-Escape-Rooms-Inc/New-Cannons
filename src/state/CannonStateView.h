#pragma once
#include <cstdint>
#include <cmath>

// Change bits local to the cannon view
namespace cannon {
enum : uint32_t {
  ChangedNone   = 0,
  ChangedAngle  = 1u << 0,
  ChangedLoaded = 1u << 1,
  ChangedFired  = 1u << 2,
};

template <typename StateT>
class StateView {
public:
  using AngleGetter = float (*)(const StateT&);
  using BoolGetter  = bool  (*)(const StateT&);

  StateView(StateT& s,
            AngleGetter getAngle,
            BoolGetter  getLoaded,
            BoolGetter  getFired)
  : s_(s), getAngle_(getAngle), getLoaded_(getLoaded), getFired_(getFired) {}

  // Call once per loop after ControllerState has been updated.
  uint32_t update() {
    uint32_t changed = ChangedNone;

    // Angle: quantize to integer degrees to avoid jitter spam
    const float a = getAngle_(s_);
    const int   q = normalize360(a);
    if (q != angleDegInt_) {
      angleDegInt_ = q;
      angleDeg_    = a;
      changed |= ChangedAngle;
    }

    const bool ld = getLoaded_(s_);
    const bool fd = getFired_(s_);

    // v3.3.1: the first update after boot only syncs the fire level, so a
    // rope already held (or a stuck switch) at power-on is not a shot.
    if (!primed_) {
      primed_ = true;
      fired_  = fd;
    }

    // Loaded: rising edge
    justLoaded_ = (ld && !loaded_);
    if (justLoaded_) changed |= ChangedLoaded;
    loaded_ = ld;

    // Fired: rising edge of the REAL button level. fired_ always mirrors the
    // rope, so a held rope gives exactly one Fired; the next shot needs the
    // rope released and pulled again.
    justFired_ = (fd && !fired_);
    if (justFired_) changed |= ChangedFired;
    fired_ = fd;

    lastChange_ = changed;
    return changed;
  }

  // What the publisher needs:
  float    angleDeg()   const { return angleDeg_; }
  bool     justLoaded() const { return justLoaded_; }
  bool     justFired()  const { return justFired_; }
  uint32_t lastChangeMask() const { return lastChange_; }

  // Reset after a shot or on PUZZLE_RESET. Loaded is cleared so a ball still
  // in the barrel is announced again (unchanged behaviour). fired_ is NOT
  // cleared any more: clearing it made a held rope count as a new pull every
  // ~2.2 s (Cannon 1, 11-01 and 11-02).
  void resetLoadedAndFired() {
    loaded_ = false;
    justLoaded_ = false;
    justFired_ = false;
  }

private:
  static int normalize360(float deg) {
    float m = std::fmod(deg, 360.0f);
    if (m < 0) m += 360.0f;
    return static_cast<int>(std::lround(m));
  }

  const StateT& s_;
  AngleGetter   getAngle_;
  BoolGetter    getLoaded_;
  BoolGetter    getFired_;

  // cached view
  float    angleDeg_     = 0.0f;
  int      angleDegInt_  = 0;   // 0..359
  bool     loaded_       = false;
  bool     fired_        = false;
  bool     justLoaded_   = false;
  bool     justFired_    = false;
  bool     primed_       = false;
  uint32_t lastChange_   = ChangedNone;
};
} // namespace cannon
