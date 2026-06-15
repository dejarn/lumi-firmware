#include "led.h"
#include "led_math.h"
#include "ledconfig_rules.h"

#include <vector>
#include <FastLED.h>
#include <Arduino.h>

namespace led {
namespace {

// ── Module-private state (exactly one strip per device) ──────────────────────
std::vector<CRGB> buffer;          // allocated once in init(), never reallocated
uint32_t          count = 0;
bool              ready = false;

LumiState state = {
  /*power*/      0x00,
  /*brightness*/ 0xFF,
  /*h*/          0,
  /*s*/          0,
  /*colorBri*/   0xFF,
  /*animId*/     0x00,
};

// Animation timing / parameters.
uint32_t lastFrameMs     = 0;
uint16_t frameIntervalMs = 33;     // ~30 fps default
uint8_t  animSpeed       = 0;
uint8_t  animIntensity   = 255;
uint16_t phase           = 0;      // animation phase accumulator (0..65535)

// Current base color as a CHSV (hue scaled from protocol 0..65535 to 0..255).
CHSV baseColor() {
  return CHSV((uint8_t)(state.h >> 8), state.s, state.colorBri);
}

// Push the buffer to the strip respecting the power flag (off ⇒ blackout,
// without mutating the logical color stored in `state`).
void present() {
  if (!ready) return;
  if (state.power) {
    FastLED.show();
  } else {
    FastLED.showColor(CRGB::Black);
  }
}

void fillSolid(const CRGB& c) {
  fill_solid(buffer.data(), count, c);
}

// ── Per-animation frame renderers ────────────────────────────────────────────
// `level` is a 0..255 triangle/sine-derived modulator built from `phase`.

void renderPulse(uint8_t level) {
  // Linear sawtooth brightness ramp on the base color (vs renderBreathe's
  // sine); `level` is the raw ramp, intensity sets depth.
  uint8_t depth = scale8(level, animIntensity);
  fillSolid((CRGB)CHSV((uint8_t)(state.h >> 8), state.s, scale8(state.colorBri, depth)));
}

void renderBreathe(uint8_t level) {
  // Smooth sine breathing; intensity sets minimum floor.
  uint8_t s = sin8(level);                       // 0..255 smooth
  uint8_t depth = scale8(s, animIntensity);
  fillSolid((CRGB)CHSV((uint8_t)(state.h >> 8), state.s, scale8(state.colorBri, depth)));
}

void renderFlash(uint8_t level) {
  // 50% duty square wave between base color and off.
  bool onPhase = level < 128;
  fillSolid(onPhase ? (CRGB)baseColor() : CRGB::Black);
}

void renderStrobe(uint8_t level) {
  // Short bright blip, mostly off; intensity widens the on window.
  uint8_t onWidth = 16 + scale8(animIntensity, 64);   // 16..80 of 256
  fillSolid(level < onWidth ? (CRGB)baseColor() : CRGB::Black);
}

void renderRainbow(uint8_t level) {
  // Sweep hue across the strip; `level` advances the start hue.
  uint8_t startHue = level;
  uint8_t deltaHue = 1 + scale8(animIntensity, 8);
  fill_rainbow(buffer.data(), count, startHue, deltaHue);
}

}  // namespace

// ── Public API ───────────────────────────────────────────────────────────────

bool init(uint32_t numLeds, uint8_t dataPin) {
  // Defence-in-depth: kMaxLeds is the authoritative constant from ledconfig_rules.h.
  if (numLeds == 0 || numLeds > ledconfig::kMaxLeds) return false;

  buffer.assign(numLeds, CRGB::Black);   // single allocation
  count = numLeds;
  CRGB* buf = buffer.data();

  switch (dataPin) {
    case 2:  FastLED.addLeds<WS2812B, 2,  GRB>(buf, numLeds); break;
    case 4:  FastLED.addLeds<WS2812B, 4,  GRB>(buf, numLeds); break;
    case 5:  FastLED.addLeds<WS2812B, 5,  GRB>(buf, numLeds); break;
    case 12: FastLED.addLeds<WS2812B, 12, GRB>(buf, numLeds); break;
    case 13: FastLED.addLeds<WS2812B, 13, GRB>(buf, numLeds); break;
    case 14: FastLED.addLeds<WS2812B, 14, GRB>(buf, numLeds); break;
    case 15: FastLED.addLeds<WS2812B, 15, GRB>(buf, numLeds); break;
    case 16: FastLED.addLeds<WS2812B, 16, GRB>(buf, numLeds); break;
    case 17: FastLED.addLeds<WS2812B, 17, GRB>(buf, numLeds); break;
    case 18: FastLED.addLeds<WS2812B, 18, GRB>(buf, numLeds); break;
    case 19: FastLED.addLeds<WS2812B, 19, GRB>(buf, numLeds); break;
    case 21: FastLED.addLeds<WS2812B, 21, GRB>(buf, numLeds); break;
    case 22: FastLED.addLeds<WS2812B, 22, GRB>(buf, numLeds); break;
    case 23: FastLED.addLeds<WS2812B, 23, GRB>(buf, numLeds); break;
    case 25: FastLED.addLeds<WS2812B, 25, GRB>(buf, numLeds); break;
    case 26: FastLED.addLeds<WS2812B, 26, GRB>(buf, numLeds); break;
    case 27: FastLED.addLeds<WS2812B, 27, GRB>(buf, numLeds); break;
    case 32: FastLED.addLeds<WS2812B, 32, GRB>(buf, numLeds); break;
    case 33: FastLED.addLeds<WS2812B, 33, GRB>(buf, numLeds); break;
    default: return false;
  }

  FastLED.setBrightness(state.brightness);
  ready = true;
  lastFrameMs = millis();
  return true;
}

void setPower(bool on) {
  state.power = on ? 0x01 : 0x00;
  // Keep the last logical color; just re-present under the new power flag.
  present();
}

void setBrightness(uint8_t brightness) {
  state.brightness = brightness;
  if (ready) FastLED.setBrightness(brightness);
  present();
}

void setColor(uint16_t h, uint8_t s, uint8_t b) {
  state.h = h;
  state.s = s;
  state.colorBri = b;
  state.animId = 0x00;          // a solid color stops any running animation
  fillSolid((CRGB)baseColor());
  present();
}

void setAnimation(uint8_t animId, uint8_t speed, uint8_t intensity) {
  if (animId < LUMI_ANIM_PULSE || animId > LUMI_ANIM_RAINBOW) {
    stopAnimation();
    return;
  }
  state.animId    = animId;
  animSpeed       = speed;
  animIntensity   = intensity;
  frameIntervalMs = intervalForSpeed(speed);
  phase           = 0;
  lastFrameMs     = millis();
}

void stopAnimation() {
  state.animId = 0x00;
  // Re-render the solid base color so we don't freeze on an animation's OFF
  // frame (e.g. flash/strobe blackout) while power is still on.
  fillSolid((CRGB)baseColor());
  present();
}

LumiState getState() {
  return state;
}

void tick() {
  if (!ready || state.animId == 0x00) return;

  uint32_t now = millis();
  if (now - lastFrameMs < frameIntervalMs) return;
  lastFrameMs = now;

  // Advance phase; speed already shapes the interval, keep a steady step here.
  phase += 1024;                 // wraps every 64 frames
  uint8_t level = (uint8_t)(phase >> 8);

  switch (state.animId) {
    case LUMI_ANIM_PULSE:   renderPulse(level);   break;
    case LUMI_ANIM_BREATHE: renderBreathe(level); break;
    case LUMI_ANIM_FLASH:   renderFlash(level);   break;
    case LUMI_ANIM_STROBE:  renderStrobe(level);  break;
    case LUMI_ANIM_RAINBOW: renderRainbow(level); break;
    default: return;
  }

  // If power is off, hold the animation phase (above) but do not light LEDs.
  present();
}

}  // namespace led
