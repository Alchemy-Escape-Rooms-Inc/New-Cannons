#pragma once
#include <Arduino.h>
#include <cstdint>
#include "../gpio.h"   // uses GpioPin

/**
 * DebouncedButton: software-debounced view over a GpioPin configured as an input.
 *
 * - Debounce window (default 30 ms).
 * - update() returns true once per committed edge (press or release).
 * - pressed()/released() report the current debounced logical state (honors polarity).
 */
class DebouncedButton {
public:
  DebouncedButton(GpioPin pin, uint16_t debounceMs = 30)
  : pin_(pin), debounceMs_(debounceMs) {}

  void begin() {
    pin_.begin();
    lastReading_ = pin_.read();   // logical (already honors polarity)
    stable_      = lastReading_;
    prevStable_  = stable_;
    lastChangeMs_ = millis();
  }

  // Call frequently (e.g., every loop). Returns true if a clean edge occurred.
  bool update() {
    const bool reading = pin_.read();
    const unsigned long now = millis();

    if (reading != lastReading_) {
      lastReading_ = reading;
      lastChangeMs_ = now;        // restart debounce window
    }

    if ((now - lastChangeMs_) >= debounceMs_ && reading != stable_) {
      prevStable_ = stable_;
      stable_     = reading;      // commit debounced state
      return true;                // one edge fired
    }
    return false;
  }

  // Current debounced state
  bool pressed()  const { return stable_; }
  bool released() const { return !stable_; }

  // Edge helpers (valid only immediately after update() returned true)
  bool rose() const { return (prevStable_ == false && stable_ == true); }
  bool fell() const { return (prevStable_ == true  && stable_ == false); }

  // Config
  void setDebounceMs(uint16_t ms) { debounceMs_ = ms; }

private:
  GpioPin        pin_;
  uint16_t       debounceMs_;

  bool           lastReading_  = false;  // most recent raw logical reading
  bool           stable_       = false;  // current debounced state
  bool           prevStable_   = false;  // previous debounced state (for edge helpers)
  unsigned long  lastChangeMs_ = 0;      // last time the raw reading flipped
};
