#pragma once
#include <cstdint>
#include "LumiProtocol.h"   // for LumiState

namespace led {
  // Allocates the CRGB buffer (size numLeds) and registers FastLED on dataPin.
  // Returns false if dataPin is not in the supported GPIO set or numLeds == 0.
  bool init(uint32_t numLeds, uint8_t dataPin);

  void setPower(bool on);
  void setBrightness(uint8_t brightness);
  void setColor(uint16_t h, uint8_t s, uint8_t b);   // HSB; h is 0..65535
  void setAnimation(uint8_t animId, uint8_t speed, uint8_t intensity);
  void stopAnimation();

  LumiState getState();   // cheap (7 bytes), return by value

  // Renders one animation frame if its interval has elapsed. Call every loop().
  void tick();
}
