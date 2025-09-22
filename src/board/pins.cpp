#include "boardkit.hpp"
#include <cstdio>

std::string BoardPins::toString() const {
  char buf[256];
  // print first CS/PWM/ADC only to keep it short; expand as you like
  const char* cs0  = (spi_.csCount > 0) ? "" : "NC";
  int cs0v = (spi_.csCount > 0) ? static_cast<int>(spi_.cs[0]) : -1;

  std::snprintf(buf, sizeof(buf),
      "I2C: SDA=%d SCL=%d %luHz | "
      "SPI: MOSI=%d MISO=%d SCK=%d CS0=%d %luHz | "
      "UART: TX=%d RX=%d CTS=%d RTS=%d %lu | "
      "PWM[%u]: pin0=%d | ADC[%u]: pin0=%d | "
      "LED=%d IRQ=%d",
      // I2C
      static_cast<int>(i2c_.sda), static_cast<int>(i2c_.scl),
      static_cast<unsigned long>(i2c_.hz),
      // SPI
      static_cast<int>(spi_.mosi), static_cast<int>(spi_.miso), static_cast<int>(spi_.sck),
      (spi_.csCount ? static_cast<int>(spi_.cs[0]) : -1),
      static_cast<unsigned long>(spi_.hz),
      // UART
      static_cast<int>(uart_.tx), static_cast<int>(uart_.rx),
      static_cast<int>(uart_.cts), static_cast<int>(uart_.rts),
      static_cast<unsigned long>(uart_.baud),
      // PWM/ADC counts
      static_cast<unsigned>(pwmCount_),
      (pwmCount_ ? static_cast<int>(pwm_[0].pin) : -1),
      static_cast<unsigned>(adcCount_),
      (adcCount_ ? static_cast<int>(adc_[0].pin) : -1),
      // GPIO
      static_cast<int>(gpio_.led), static_cast<int>(gpio_.irq)
  );
  (void)cs0; (void)cs0v; // silence unused warnings if you trim printing
  return std::string(buf);
}
