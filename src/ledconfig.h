#pragma once
#include "ledconfig_rules.h"  // Config, kMinLeds, kMaxLeds, isSupportedPin, isValidConfig

namespace ledconfig {
  // Returns true only if a complete, valid config was loaded from NVS.
  bool load(Config& out);

  // Persists cfg to NVS namespace "lumi-led".
  void save(const Config& cfg);

  // Blocking serial prompt loop (setup-stage only). Re-prompts until valid,
  // persists via save(), fills out.
  void provisionInteractive(Config& out);
}
