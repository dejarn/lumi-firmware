// lumi-firmware — boot, callback wiring, cooperative loop.
//
// Thin FastLED driver on top of the vendored LumiProtocol library. The library
// owns Wi-Fi/MQTT/framing/CRC/ACK/STATE_REPORT/DISCOVERY/zone-NVS/reconnect.
// This translation unit derives the device name from the MAC, registers the
// six command callbacks before lumi.begin(), retries lumi.begin() with capped
// backoff, and runs loop().
//
// Invariants: never open an MQTT client, build/parse a frame, or emit an ACK
// here; never touch the lumi/zone NVS namespace; no delay() in loop().

#include <Arduino.h>
#include <WiFi.h>

#include "secrets.h"
#include "ledconfig.h"
#include "led.h"
#include "LumiProtocol.h"

// Single file-scope protocol instance. led/ledconfig hold their own state.
static LumiProtocol lumi;

// MAC-derived discovery name: "lumi-" + last 2 MAC bytes as hex => "lumi-xxxx".
static char deviceName[11] = {};

void setup() {
  Serial.begin(115200);

  // 1. Load LED config from NVS; provision interactively on first boot.
  ledconfig::Config cfg;
  if (!ledconfig::load(cfg)) {
    Serial.println(F("[main] no LED config in NVS — entering provisioning"));
    ledconfig::provisionInteractive(cfg);
  }

  // 2. Bring up the strip. Abort here if the pin/length are invalid — do NOT
  //    proceed to begin(); a misconfigured strip cannot render anything.
  if (!led::init(cfg.numLeds, cfg.dataPin)) {
    Serial.println(F("[main] FATAL: led::init failed (bad data_pin or num_leds)."));
    Serial.println(F("[main] Re-provision with 'pio run -t erase' then reflash. Halting."));
    while (true) {
      delay(1000);  // halt — never reached the cooperative loop()
    }
  }

  // 3. Derive deviceName "lumi-xxxx" from the last 2 bytes of the Wi-Fi MAC.
  uint8_t mac[6];
  WiFi.macAddress(mac);
  snprintf(deviceName, sizeof(deviceName), "lumi-%02x%02x", mac[4], mac[5]);
  Serial.print(F("[main] device name: "));
  Serial.println(deviceName);

  // 4. Register the six command callbacks before begin() so no command window is
  //    missed: begin() connects + subscribes, and any retained cmd arriving
  //    immediately after subscribe would otherwise be ACK'd with no effect.
  //    Lambdas are capture-less → no heap allocation for std::function.
  lumi.onSetPower      ([](bool on)                                  { led::setPower(on); });
  lumi.onSetBrightness ([](uint8_t brightness)                       { led::setBrightness(brightness); });
  lumi.onSetColor      ([](uint16_t h, uint8_t s, uint8_t b)         { led::setColor(h, s, b); });
  lumi.onSetAnimation  ([](uint8_t id, uint8_t speed, uint8_t inten) { led::setAnimation(id, speed, inten); });
  lumi.onStopAnimation ([]()                                         { led::stopAnimation(); });
  lumi.onGetState      ([]() -> LumiState                            { return led::getState(); });

  // 5. Connect via the library with capped exponential backoff. begin() blocks
  //    up to ~30 s for Wi-Fi and returns false on timeout; the library's own
  //    5 s reconnect loop does not cover this pre-begin() case, so retry here.
  uint32_t backoffMs = 1000;                 // start at 1 s
  const uint32_t backoffCeilMs = 30000;      // cap at 30 s
  while (!lumi.begin(WIFI_SSID, WIFI_PASSWORD, MQTT_HOST, deviceName, MQTT_PORT,
                     MQTT_USER, MQTT_PASSWORD)) {
    Serial.print(F("[main] connect failed — retrying in "));
    Serial.print(backoffMs / 1000);
    Serial.println(F(" s"));
    uint32_t start = millis();
    while (millis() - start < backoffMs) {
      yield();
    }
    if (backoffMs < backoffCeilMs) {
      backoffMs *= 2;
      if (backoffMs > backoffCeilMs) backoffMs = backoffCeilMs;
    }
  }
  Serial.println(F("[main] connected"));
}

void loop() {
  lumi.loop();
  led::tick();
}
