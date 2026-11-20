#pragma once
/**
 * FireLeds - muzzle-flash light strip (v3.5.0, Cannon 1).
 *
 * WS2812B strip on FIRE_LED_PIN. Every Fired event calls trigger():
 *   0-90 ms      white-hot yellow flash, full brightness
 *   90-500 ms    bright orange glow with a fire flicker
 *   500-2600 ms  colour slides orange -> red while brightness fades to off
 * Non-blocking: tick() is called many times a second from loop(). A new shot
 * during a fade restarts the effect from the flash. The strip is dark otherwise.
 */
#include <Adafruit_NeoPixel.h>

// v3.5.4: -DFIRE_LED_RGBW=1 = 4-channel strip (SK6812 RGBW). Sending 3-byte data to a
// 4-byte strip lights only 3/4 of the pixels in the wrong colours (24 px -> 18 whitish).
#ifdef FIRE_LED_RGBW
#define FIRE_LED_TYPE (NEO_GRBW + NEO_KHZ800)
#else
#define FIRE_LED_TYPE (NEO_GRB + NEO_KHZ800)
#endif

namespace fx {

class FireLeds {
public:
  FireLeds(int pin, uint16_t count) : strip_(count, pin, FIRE_LED_TYPE), count_(count) {}

  void begin() {
    strip_.begin();
    strip_.setBrightness(255);
    strip_.clear();
    strip_.show();
  }

  void trigger() { hold_ = false; startMs_ = millis(); active_ = true; }

  // Solid orange until off() - for wiring checks (LEDS_ON / LEDS_OFF commands)
  void on() { solid(255, 90, 0); }
  // Any solid colour until off() (v3.5.6, LEDS_RGB r,g,b command)
  void solid(uint8_t r, uint8_t g, uint8_t b) {
    active_ = false; hold_ = true;
    for (uint16_t i = 0; i < count_; i++) strip_.setPixelColor(i, strip_.Color(r, g, b));
    strip_.show();
  }
  void off() { active_ = false; hold_ = false; strip_.clear(); strip_.show(); }
  bool active() const { return active_; }

  void tick() {
    if (!active_) return;
    uint32_t now = millis();
    if (now - lastFrameMs_ < FRAME_MS) return;
    lastFrameMs_ = now;
    uint32_t t = now - startMs_;

    if (t >= TOTAL_MS) {           // done: strip fully off
      strip_.clear();
      strip_.show();
      active_ = false;
      return;
    }

    for (uint16_t i = 0; i < count_; i++) {
      float r, g, b, level;
      if (t < FLASH_MS) {                         // white-hot flash
        r = 255; g = 200; b = 60; level = 1.0f;
      } else if (t < GLOW_MS) {                   // bright orange glow
        float k = float(t - FLASH_MS) / float(GLOW_MS - FLASH_MS);
        r = 255; g = 200 - 110 * k; b = 60 * (1.0f - k); // yellow-white -> orange (255,90,0)
        level = 0.85f + 0.15f * float(random(0, 100)) / 100.0f;
      } else {                                    // orange -> red, fading out
        float k = float(t - GLOW_MS) / float(TOTAL_MS - GLOW_MS);  // 0..1
        r = 255; g = 90 * (1.0f - k) * (1.0f - k); b = 0;          // green drains first = redder
        float fade = (1.0f - k);
        level = fade * fade;                                       // ease-out to dark
        level *= 0.8f + 0.2f * float(random(0, 100)) / 100.0f;     // embers flicker
      }
      strip_.setPixelColor(i, strip_.Color(uint8_t(r * level), uint8_t(g * level), uint8_t(b * level)));
    }
    strip_.show();
  }

private:
  static constexpr uint32_t FRAME_MS = 15;
  static constexpr uint32_t FLASH_MS = 90;
  static constexpr uint32_t GLOW_MS  = 500;
  static constexpr uint32_t TOTAL_MS = 2600;

  Adafruit_NeoPixel strip_;
  uint16_t count_;
  uint32_t startMs_ = 0;
  uint32_t lastFrameMs_ = 0;
  bool active_ = false;
  bool hold_ = false;
};

} // namespace fx
