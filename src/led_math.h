#pragma once
#include <cstdint>

// Pure, Arduino-free math helpers for the led module.
// Inline to avoid ODR violations when included from multiple TUs.

namespace led {

// Map protocol speed (0..255) → frame interval in milliseconds.
// Higher speed => shorter interval. Range: 90 ms (slow) down to 10 ms (fast).
inline uint16_t intervalForSpeed(uint8_t speed) {
  const uint16_t kMax = 90;
  const uint16_t kMin = 10;
  uint16_t span = kMax - kMin;
  return kMax - static_cast<uint16_t>((span * static_cast<uint16_t>(speed)) / 255);
}

}  // namespace led
