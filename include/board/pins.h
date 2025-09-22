// include/pins.h

#pragma once

#include <cstdint>
#include <array>
#include <string>

class BoardPins {
public:
  using Pin       = int16_t;      // or override via -DBOARDPINS_PIN_T=...
  using I2CFreqHz = uint32_t;
  using SPIFreqHz = uint32_t;
  using Baud      = uint32_t;

  static constexpr Pin NC = static_cast<Pin>(-1);
  static constexpr std::size_t MAX_SPI_CS = 4;
  static constexpr std::size_t MAX_PWM_CH = 8;
  static constexpr std::size_t MAX_ADC_CH = 8;

  // --- Groups (now with constexpr ctors, so SPI{} etc. always work) ---

  struct I2C {
    Pin sda, scl; I2CFreqHz hz;
    constexpr I2C(Pin sda = NC, Pin scl = NC, I2CFreqHz hz = 400000U) noexcept
      : sda(sda), scl(scl), hz(hz) {}
    constexpr bool valid() const noexcept { return sda != NC && scl != NC; }
  };

  struct SPI {
    Pin mosi, miso, sck;
    std::array<Pin, MAX_SPI_CS> cs;
    std::uint8_t csCount;
    SPIFreqHz hz;
    // default CS array filled with NC
    static constexpr std::array<Pin, MAX_SPI_CS> CS_NC = {NC, NC, NC, NC};

    constexpr SPI(Pin mosi = NC, Pin miso = NC, Pin sck = NC,
                  std::array<Pin, MAX_SPI_CS> cs = CS_NC,
                  std::uint8_t csCount = 0,
                  SPIFreqHz hz = 8000000U) noexcept
      : mosi(mosi), miso(miso), sck(sck), cs(cs), csCount(csCount), hz(hz) {}

    constexpr bool valid() const noexcept { return mosi != NC && sck != NC; }
    constexpr Pin csAt(std::size_t i) const noexcept {
      return (i < csCount) ? cs[i] : NC;
    }
  };

  struct UART {
    Pin tx, rx, cts, rts; Baud baud;
    constexpr UART(Pin tx = NC, Pin rx = NC, Pin cts = NC, Pin rts = NC,
                   Baud baud = 115200U) noexcept
      : tx(tx), rx(rx), cts(cts), rts(rts), baud(baud) {}
    constexpr bool valid() const noexcept { return tx != NC && rx != NC; }
  };

  struct PWMChannel {
    Pin pin; uint32_t freq_hz; uint8_t resolution_bits;
    constexpr PWMChannel(Pin pin = NC, uint32_t freq_hz = 1000U,
                         uint8_t resolution_bits = 10) noexcept
      : pin(pin), freq_hz(freq_hz), resolution_bits(resolution_bits) {}
    constexpr bool valid() const noexcept { return pin != NC; }
  };

  struct ADCChannel {
    Pin pin; uint8_t index; uint16_t vref_mv;
    constexpr ADCChannel(Pin pin = NC, uint8_t index = 0,
                         uint16_t vref_mv = 1100) noexcept
      : pin(pin), index(index), vref_mv(vref_mv) {}
    constexpr bool valid() const noexcept { return pin != NC; }
  };

  struct GPIO {
    Pin led, irq, user1, user2;
    constexpr GPIO(Pin led = NC, Pin irq = NC,
                   Pin user1 = NC, Pin user2 = NC) noexcept
      : led(led), irq(irq), user1(user1), user2(user2) {}
  };

    // --- Ethernet-specific groups (generic form) ---
  struct EthDev {
    Pin cs, rst, intn;                 // device pins (W5500 today)
    constexpr EthDev(Pin cs = NC, Pin rst = NC, Pin intn = NC) noexcept
      : cs(cs), rst(rst), intn(intn) {}
    constexpr bool valid() const noexcept { return cs != NC; }
  };

  // ---------- Construction (now SPI{}, UART{} etc. are valid defaults) ----------
  constexpr BoardPins(I2C i2c,
                      SPI spi = SPI{},
                      UART uart = UART{},
                      const std::array<PWMChannel, MAX_PWM_CH>& pwm = std::array<PWMChannel, MAX_PWM_CH>{},
                      std::size_t pwmCount = 0,
                      const std::array<ADCChannel, MAX_ADC_CH>& adc = std::array<ADCChannel, MAX_ADC_CH>{},
                      std::size_t adcCount = 0,
                      GPIO gpio = GPIO{},
                      EthDev eth = EthDev{}) noexcept
  : i2c_(i2c), spi_(spi), uart_(uart),
    pwm_(pwm), pwmCount_(static_cast<uint8_t>(pwmCount)),
    adc_(adc), adcCount_(static_cast<uint8_t>(adcCount)),
    gpio_(gpio), eth_(eth) {}

  // Accessors & summary
  constexpr const I2C&  i2c()  const noexcept { return i2c_; }
  constexpr const SPI&  spi()  const noexcept { return spi_; }
  constexpr const UART& uart() const noexcept { return uart_; }
  constexpr const GPIO& gpio() const noexcept { return gpio_; }
  constexpr const EthDev& eth() const noexcept { return eth_; }

  constexpr std::size_t pwmCount() const noexcept { return pwmCount_; }
  constexpr std::size_t adcCount() const noexcept { return adcCount_; }
  constexpr const PWMChannel pwmAt(std::size_t i) const noexcept {
    return (i < pwmCount_) ? pwm_[i] : PWMChannel{};
  }
  constexpr const ADCChannel adcAt(std::size_t i) const noexcept {
    return (i < adcCount_) ? adc_[i] : ADCChannel{};
  }

  std::string toString() const; // implemented in pins.cpp

  // Handy preset
  static constexpr BoardPins DevKitS3_DefaultI2C(Pin sda = 8, Pin scl = 9,
                                                  I2CFreqHz hz = 400000U) noexcept {
    return BoardPins(I2C{sda, scl, hz});
  }

private:
  I2C  i2c_;
  SPI  spi_;
  UART uart_;
  EthDev eth_;

  std::array<PWMChannel, MAX_PWM_CH> pwm_{};
  uint8_t pwmCount_ = 0;

  std::array<ADCChannel, MAX_ADC_CH> adc_{};
  uint8_t adcCount_ = 0;

  GPIO gpio_;

};
