// include/board/gpio.h

#pragma once

#include <Arduino.h>
#include <cstdint>
#include <limits>

// ---------- Basic enums ----------
enum class ActivePolarity : uint8_t { ActiveHigh, ActiveLow };
enum class Pull          : uint8_t { None, Up, Down };
enum class GpioMode      : uint8_t { Input, Output, OpenDrain };

// Allow projects to override the underlying pin storage type at build time.
// Example (platformio.ini): build_flags = -DGPIO_PIN_T=uint32_t
#ifndef GPIO_PIN_T
  #define GPIO_PIN_T int16_t  // signed so we can use -1 as "NC"
#endif

using gpio_pin_t = GPIO_PIN_T;

static constexpr gpio_pin_t GPIO_NC =
  std::numeric_limits<gpio_pin_t>::is_signed
    ? static_cast<gpio_pin_t>(-1)
    : std::numeric_limits<gpio_pin_t>::max();

// Some cores don't define OUTPUT_OPEN_DRAIN; provide a fallback.
#ifndef OUTPUT_OPEN_DRAIN
  #define OUTPUT_OPEN_DRAIN OUTPUT
#endif

// ---------- Minimal, reusable pin wrapper ----------
class GpioPin {
public:
  constexpr GpioPin(gpio_pin_t pin = GPIO_NC,
                    GpioMode mode = GpioMode::Input,
                    Pull pull = Pull::None,
                    ActivePolarity pol = ActivePolarity::ActiveHigh)
  : pin_(pin), mode_(mode), pull_(pull), pol_(pol) {}

  // Basic introspection
  constexpr bool       valid() const noexcept { return pin_ != GPIO_NC; }
  constexpr gpio_pin_t num()   const noexcept { return pin_; }

  // Configure (safe to call more than once)
  void begin() const {
    if (!valid()) return;
    switch (mode_) {
      case GpioMode::Input:     applyPull_(INPUT); break;
      case GpioMode::Output:    pinMode(pin_, OUTPUT); break;
      case GpioMode::OpenDrain: pinMode(pin_, OUTPUT_OPEN_DRAIN); break;
    }
  }

  // Logical write/read (honors polarity)
  void write(bool logicalActive) const {
    if (!valid()) return;
    const bool level = (pol_ == ActivePolarity::ActiveHigh) ? logicalActive
                                                            : !logicalActive;
    digitalWrite(pin_, level ? HIGH : LOW);
  }
  bool read() const {
    if (!valid()) return false;
    const int v = digitalRead(pin_);
    return (pol_ == ActivePolarity::ActiveHigh) ? (v == HIGH) : (v == LOW);
  }

  // Raw helpers
  void writeRaw(bool high) const { if (valid()) digitalWrite(pin_, high ? HIGH : LOW); }
  void toggle() const            { write(!read()); }

  // Mutable configuration (if you want to reuse the object)
  void setMode(GpioMode m)               { mode_ = m; }
  void setPull(Pull p)                   { pull_ = p; }
  void setPolarity(ActivePolarity p)     { pol_  = p; }

private:
  void applyPull_(uint8_t base) const {
    // Map our Pull enum to Arduino pinMode variants (when available)
    switch (pull_) {
      case Pull::None: pinMode(pin_, base); break;
      case Pull::Up:   pinMode(pin_, base == INPUT ? INPUT_PULLUP : base); break;
      case Pull::Down:
      #ifdef INPUT_PULLDOWN
        pinMode(pin_, base == INPUT ? INPUT_PULLDOWN : base);
      #else
        pinMode(pin_, base); // not available on all cores
      #endif
        break;
    }
  }

  gpio_pin_t       pin_;
  GpioMode         mode_;
  Pull             pull_;
  ActivePolarity   pol_;
};

