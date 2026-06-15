#include "ledconfig.h"
// ledconfig_rules.h is included transitively via ledconfig.h and provides
// kSupportedPins, kMinLeds, kMaxLeds, isSupportedPin(), isValidConfig().

#include <Arduino.h>
#include <Preferences.h>

namespace ledconfig {
namespace {

constexpr char kNamespace[] = "lumi-led";
constexpr char kKeyNumLeds[] = "num_leds";
constexpr char kKeyDataPin[] = "data_pin";

// Reads a non-empty, trimmed line from Serial. Blocking; setup-stage only.
String readLine() {
  for (;;) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() > 0) {
      return line;
    }
  }
}

}  // namespace

bool load(Config& out) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/true);

  Config cfg;
  cfg.numLeds = prefs.getULong(kKeyNumLeds, 0);
  cfg.dataPin = prefs.getUChar(kKeyDataPin, 0);

  prefs.end();

  if (!isValidConfig(cfg)) {
    return false;
  }

  out = cfg;
  return true;
}

void save(const Config& cfg) {
  Preferences prefs;
  prefs.begin(kNamespace, /*readOnly=*/false);
  prefs.putULong(kKeyNumLeds, cfg.numLeds);
  prefs.putUChar(kKeyDataPin, cfg.dataPin);
  prefs.end();
}

void provisionInteractive(Config& out) {
  Config cfg{};

  Serial.println();
  Serial.println(F("=== Lumi LED provisioning ==="));
  Serial.println(F("No valid LED config found. Please configure this device."));

  // Prompt: number of LEDs.
  for (;;) {
    Serial.print(F("Number of LEDs (1-1000): "));
    String line = readLine();
    long value = line.toInt();
    Serial.println(value);
    if (value >= static_cast<long>(kMinLeds) &&
        value <= static_cast<long>(kMaxLeds)) {
      cfg.numLeds = static_cast<uint32_t>(value);
      break;
    }
    Serial.println(F("  Invalid value. Expected an integer in 1..1000."));
  }

  // Prompt: data pin.
  for (;;) {
    Serial.print(F("Data GPIO pin: "));
    String line = readLine();
    long value = line.toInt();
    Serial.println(value);
    if (value >= 0 && value <= 255 &&
        isSupportedPin(static_cast<uint8_t>(value))) {
      cfg.dataPin = static_cast<uint8_t>(value);
      break;
    }
    Serial.println(F("  Unsupported GPIO. Supported pins:"));
    Serial.print(F("  "));
    for (size_t i = 0; i < sizeof(kSupportedPins) / sizeof(kSupportedPins[0]);
         ++i) {
      if (i > 0) {
        Serial.print(F(", "));
      }
      Serial.print(kSupportedPins[i]);
    }
    Serial.println();
  }

  save(cfg);

  Serial.print(F("Saved config: numLeds="));
  Serial.print(cfg.numLeds);
  Serial.print(F(", dataPin="));
  Serial.println(cfg.dataPin);

  out = cfg;
}

}  // namespace ledconfig
