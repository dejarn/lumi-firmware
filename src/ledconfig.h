#pragma once
#include <cstdint>

namespace ledconfig {
  struct Config {
    uint32_t numLeds;   // strip length, sizes the CRGB buffer
    uint8_t  dataPin;   // GPIO driving the strip
  };

  // Returns true only if a complete, valid config was loaded from NVS.
  bool load(Config& out);

  // Persists cfg to NVS namespace "lumi-led".
  void save(const Config& cfg);

  // Blocking serial prompt loop (setup-stage only). Re-prompts until valid,
  // persists via save(), fills out.
  void provisionInteractive(Config& out);

  // Validates a candidate data pin against the supported GPIO set.
  // Keep this in sync with the led module's switch-dispatch pin list.
  bool isSupportedPin(uint8_t pin);
}
