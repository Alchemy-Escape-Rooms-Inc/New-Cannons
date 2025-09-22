#pragma once
#include <cstdint>
#include <cstddef>
#include "board/pins.h"

/**
 * I2CBus: Arduino/Wire-backed I²C wrapper with no globals.
 * - Owns SDA/SCL/frequency/timeout.
 * - Idempotent init via begin().
 * - Static callback thunks so sensor drivers (e.g., ALS31300) can pass
 *   plain function pointers without depending on Wire directly.
 *
 * Usage:
 *   I2CBus bus(pins.i2c());      // construct from your BoardPins::I2C
 *   bus.begin();                 // initialize Wire once
 *   I2CBus::setActive(&bus);     // route callbacks to this instance
 *   ALS31300::Sensor::setCallbacks(
 *     I2CBus::cbRegisterDevice, I2CBus::cbUnregisterDevice,
 *     I2CBus::cbChangeAddress,  I2CBus::cbWrite, I2CBus::cbRead);
 */
class I2CBus {
public:
  using Addr = std::uint8_t;

  // Construct from discrete pins/speed
  constexpr I2CBus(BoardPins::Pin sda,
                   BoardPins::Pin scl,
                   BoardPins::I2CFreqHz hz = 400000U,
                   std::uint16_t timeout_ms = 50) noexcept
  : sda_(sda), scl_(scl), hz_(hz), timeout_ms_(timeout_ms) {}

  // Construct from BoardPins::I2C struct
  constexpr explicit I2CBus(const BoardPins::I2C& cfg,
                            std::uint16_t timeout_ms = 50) noexcept
  : sda_(cfg.sda), scl_(cfg.scl), hz_(cfg.hz), timeout_ms_(timeout_ms) {}

  /** Initialize Wire with this bus config (safe to call multiple times). */
  void begin();

  /** Probe an address (returns true if device ACKs). */
  bool devicePresent(Addr address);

  /** Write payload with STOP. */
  bool write(Addr address, const std::uint8_t* payload, std::size_t n);

  /** Write register/index (no STOP), then read bytes (repeated START). */
  bool read(Addr address,
            const std::uint8_t* index, std::size_t index_len,
            std::uint8_t* out, std::size_t out_len);

  // ---- Driver callback thunks (route to the "active" I2CBus instance) ----
  static void setActive(I2CBus* bus);
  static bool cbRegisterDevice(std::uint8_t addr);
  static bool cbUnregisterDevice(std::uint8_t addr);
  static bool cbChangeAddress(std::uint8_t oldAddr, std::uint8_t newAddr);
  static bool cbWrite(std::uint8_t addr, std::uint8_t* payload, std::size_t n);
  static bool cbRead (std::uint8_t addr,
                      std::uint8_t* send, std::size_t send_n,
                      std::uint8_t* recv, std::size_t recv_n);

  /**
   * @brief Attempt to free a stuck I²C bus when SDA is held low by a slave.
   *
   * Procedure (per common I²C recovery practice):
   *  1) Detach Wire to take GPIO control.
   *  2) Drive SCL as open-drain and clock it up to `pulses` times while sampling SDA.
   *  3) Generate a STOP (SDA ↑ while SCL is high).
   *  4) Reattach Wire at `hz_` (optionally step in at 100 kHz first, then restore).
   *
   * @param pulses         Number of SCL pulses to attempt (typ. 9).
   * @param slow_recover   Re-init at 100 kHz first (true) then restore to `hz_`.
   * @return true if SDA reads high at the end; false if still stuck.
   */
  bool clearBus(std::uint8_t pulses = 9, bool slow_recover = true);
  

private:
  BoardPins::Pin        sda_;
  BoardPins::Pin        scl_;
  BoardPins::I2CFreqHz  hz_;
  std::uint16_t         timeout_ms_;
  bool                  inited_ = false;

  static I2CBus* active_; // used only by the static callback thunks
};
