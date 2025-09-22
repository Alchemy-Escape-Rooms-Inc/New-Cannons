#pragma once
#include <cstdint>

// Adjust these includes to match your tree:
// If your files live in include/hal/... etc, use those paths instead.
#include "board/pins.h"
#include "hal/i2c.h"
#include "hal/gpio.h"
#include "hal/input/DebouncedButton.h"   // or "hal/input/debounced_button.h"

class Controller {
public:
  using ButtonCallback = void(*)(bool pressed);  // pressed = true on rising edge

  /**
   * Construct the controller.
   * @param pins        Board pin mapping (I2C pins come from here)
   * @param buttonPin   GPIO number for the button (use GPIO_NC if none)
   * @param pull        Button pull config (default internal pull-up)
   * @param polarity    Button polarity (active-low for GND-when-pressed wiring)
   * @param debounceMs  Debounce window in ms
   */
  Controller(const BoardPins& pins,
             gpio_pin_t buttonPin,
             Pull       pull      = Pull::Up,
             ActivePolarity polarity = ActivePolarity::ActiveLow,
             uint16_t   debounceMs = 30) noexcept;

  /** Initialize hardware (I2C bus + button). Safe to call once at boot. */
  void begin();

  /** Poll the button; invoke callback on a clean press/release edge. */
  void pollButton();

  /** Register a callback invoked on button edges (optional). */
  void onButtonChange(ButtonCallback cb) { buttonCb_ = cb; }

  // Accessors
  I2CBus&           i2c()            { return i2c_; }
  DebouncedButton&  button()         { return button_; }
  const BoardPins&  board()    const { return pins_; }

private:
  BoardPins        pins_;
  I2CBus           i2c_;
  GpioPin          buttonPin_;
  DebouncedButton  button_;
  ButtonCallback   buttonCb_ = nullptr;
};
