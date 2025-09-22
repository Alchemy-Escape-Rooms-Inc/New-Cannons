#include "hardware/controller.h"
#include <Arduino.h>  // for millis() if needed by your DebouncedButton

Controller::Controller(const BoardPins& pins,
                       gpio_pin_t buttonPin,
                       Pull pull,
                       ActivePolarity polarity,
                       uint16_t debounceMs) noexcept
: pins_(pins),
  i2c_(pins_.i2c()),                                  // build I2CBus from BoardPins
  buttonPin_(GpioPin(buttonPin, GpioMode::Input, pull, polarity)),
  button_(DebouncedButton(buttonPin_, debounceMs))     // behavior layered on the pin
{}

void Controller::begin() {
  // Bring up I2C and make it the active transport for drivers using the static thunks
  i2c_.clearBus();
  i2c_.begin();
  I2CBus::setActive(&i2c_);

    
  // Initialize the button (no-op if pin was GPIO_NC)
  if (buttonPin_.valid()) {
    button_.begin();
  }
}

void Controller::pollButton() {
  if (!buttonPin_.valid()) return;
  if (button_.update() && buttonCb_) {
    buttonCb_(button_.pressed());
  }
}
