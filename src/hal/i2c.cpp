// src/peripherals/i2c.cpp
#include "boardkit.hpp"
#include <Wire.h>

// Fallback for platforms without explicit open-drain mode
#ifndef OUTPUT_OPEN_DRAIN
#define OUTPUT_OPEN_DRAIN OUTPUT
#endif

I2CBus* I2CBus::active_ = nullptr;

void I2CBus::begin() {
  if (inited_) return;
  Wire.begin(static_cast<int>(sda_), static_cast<int>(scl_), hz_);
  Wire.setTimeOut(timeout_ms_);
  inited_ = true;
}

bool I2CBus::devicePresent(Addr address) {
  begin();
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

bool I2CBus::write(Addr address, const std::uint8_t* payload, std::size_t n) {
  begin();
  Wire.beginTransmission(address);
  if (n) Wire.write(payload, n);
  return Wire.endTransmission(true) == 0; // send STOP
}

bool I2CBus::read(Addr address,
                  const std::uint8_t* index, std::size_t index_len,
                  std::uint8_t* out, std::size_t out_len) {
  begin();

  // Stage 1: write index/register (no STOP → keep bus for repeated START)
  Wire.beginTransmission(address);
  if (index_len) Wire.write(index, index_len);
  if (Wire.endTransmission(false) != 0) return false; // no STOP

  // Stage 2: read bytes
  std::size_t got = Wire.requestFrom(static_cast<int>(address),
                                     static_cast<int>(out_len));
  if (got != out_len) return false;

  for (std::size_t i = 0; i < out_len; ++i) {
    int b = Wire.read();
    if (b < 0) return false;
    out[i] = static_cast<std::uint8_t>(b);
  }
  return true;
}

// ---- "active" routing for driver callbacks ----
void I2CBus::setActive(I2CBus* bus) {
  active_ = bus;
  if (active_) active_->begin();
}

bool I2CBus::cbRegisterDevice(std::uint8_t) { return active_ != nullptr; }
bool I2CBus::cbUnregisterDevice(std::uint8_t){ return active_ != nullptr; }
bool I2CBus::cbChangeAddress(std::uint8_t, std::uint8_t){ return active_ != nullptr; }

bool I2CBus::cbWrite(std::uint8_t addr, std::uint8_t* payload, std::size_t n) {
  return active_ ? active_->write(addr, payload, n) : false;
}

bool I2CBus::cbRead(std::uint8_t addr,
                    std::uint8_t* send, std::size_t send_n,
                    std::uint8_t* recv, std::size_t recv_n) {
  return active_ ? active_->read(addr, send, send_n, recv, recv_n) : false;
}

bool I2CBus::clearBus(std::uint8_t pulses, bool slow_recover) {
  // 1) Detach Wire so we can manipulate the pins directly
  if (inited_) {
    Wire.end();
    inited_ = false;
  }

  const int sdaPin = static_cast<int>(sda_);
  const int sclPin = static_cast<int>(scl_);

  // 2) Prepare lines: SCL as open-drain (released high), SDA as input with pull-up
  pinMode(sclPin, OUTPUT_OPEN_DRAIN);
  digitalWrite(sclPin, HIGH);            // release (let pull-up raise SCL)
  pinMode(sdaPin, INPUT_PULLUP);         // sample SDA

  delayMicroseconds(5);

  // If SCL is physically held low by a device, we can't recover here.
  // (Optional) you could read SCL and bail early; many MCUs don't allow reading OD state reliably.
  // Try clocking while SDA is low.
  for (std::uint8_t i = 0; i < pulses && digitalRead(sdaPin) == LOW; ++i) {
    // Clock low
    digitalWrite(sclPin, LOW);
    delayMicroseconds(5);
    // Clock high (release)
    digitalWrite(sclPin, HIGH);
    delayMicroseconds(5);
  }

  // 3) Generate a STOP condition: SDA low -> SDA high while SCL is high
  // Take control of SDA as open-drain
  pinMode(sdaPin, OUTPUT_OPEN_DRAIN);

  // Ensure SCL high before STOP
  digitalWrite(sclPin, HIGH);
  delayMicroseconds(5);

  // SDA low then release to high while SCL high -> STOP
  digitalWrite(sdaPin, LOW);
  delayMicroseconds(5);
  digitalWrite(sdaPin, HIGH);
  delayMicroseconds(5);

  // Check if SDA is free now
  pinMode(sdaPin, INPUT_PULLUP);
  bool freed = (digitalRead(sdaPin) == HIGH);

  // 4) Reattach Wire (optionally step in slow, then restore target clock)
  const auto initial_hz = hz_;
  const auto step_hz    = slow_recover ? 100000U : initial_hz;

  Wire.begin(static_cast<int>(sda_), static_cast<int>(scl_), step_hz);
  Wire.setTimeOut(timeout_ms_);
  inited_ = true;

  if (slow_recover && initial_hz != step_hz) {
    // ESP32 Arduino supports setClock(); harmless if equal
    Wire.setClock(initial_hz);
  }

  return freed;
}