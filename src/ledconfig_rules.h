#pragma once
#include <cstdint>

// Pure, Arduino-free rules shared between ledconfig.cpp, led.cpp, and native
// unit tests. All functions are inline to avoid ODR violations when included
// from multiple TUs.

namespace ledconfig {

constexpr uint8_t kSupportedPins[] = {
    2, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33};

constexpr uint32_t kMinLeds = 1;
constexpr uint32_t kMaxLeds = 1000;

struct Config {
  uint32_t numLeds;   // strip length, sizes the CRGB buffer
  uint8_t  dataPin;   // GPIO driving the strip
};

inline bool isSupportedPin(uint8_t pin) {
  for (uint8_t supported : kSupportedPins) {
    if (supported == pin) return true;
  }
  return false;
}

inline bool isValidConfig(const Config& cfg) {
  return cfg.numLeds >= kMinLeds && cfg.numLeds <= kMaxLeds &&
         isSupportedPin(cfg.dataPin);
}

}  // namespace ledconfig
