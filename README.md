# lumi-firmware

> Firmware for the custom ESP32 LED strip controllers in the **lumi** self-hosted home automation platform.

![board ESP32](https://img.shields.io/badge/board-ESP32-e7352c?logo=espressif&logoColor=white) ![framework Arduino](https://img.shields.io/badge/framework-Arduino-00979d?logo=arduino&logoColor=white) ![PlatformIO](https://img.shields.io/badge/PlatformIO-esp32dev-f5822a?logo=platformio&logoColor=white) ![FastLED](https://img.shields.io/badge/FastLED-3.6-ff5252)

lumi-firmware is the single binary flashed onto every custom ESP32 LED strip controller in [lumi](https://github.com/dejarn/lumi), a self-hosted home automation platform running on a Raspberry Pi — no cloud, no Home Assistant. It is a **thin LED driver** on top of the [lumi-protocol](https://github.com/dejarn/lumi-protocol) `LumiProtocol` library: the library owns Wi-Fi, MQTT, framing, ACK, discovery, and zone state — the firmware wires GPIO callbacks and renders the strip.

```
Next.js UI / Automations
         │
 mqtt-bridge (Raspberry Pi)
 lumi-protocol  [Node.js]
         │  MQTT  (lumi/… topics)
    ┌────┴────┐
 ESP32 #1  ESP32 #2  …
 lumi-firmware  ←  this repo
 LumiProtocol  [Arduino]
```

## How it works

The firmware never touches MQTT, frames, or ACK. The library decodes a command and fires a callback; the firmware applies it to the strip and tracks state; the library emits ACK + STATE_REPORT automatically.

```
        ┌──────────────────────────────────────────────────────────┐
        │                         main.cpp                          │
        │  setup() ── provision · ledInit · lumi.begin · wire cbs   │
        │  loop()  ── lumi.loop() + render frame                    │
        └───────┬───────────────────────────┬──────────────────────┘
                │ begin(ssid,pass,host,name) │ callbacks (GPIO)
                ▼                            ▼
   ┌──────────────────────────┐   ┌──────────────────────────────┐
   │   LumiProtocol  (lib)    │   │            led               │
   │  Wi-Fi · MQTT · framing  │   │  FastLED render              │
   │  CRC · ACK · STATE_REPORT│   │  HSB→RGB · animations        │
   │  DISCOVERY · zone NVS    │   │  current LumiState           │
   └──────────────────────────┘   └──────────────────────────────┘
```

| Owned by `LumiProtocol` (library) | Owned by lumi-firmware |
|---|---|
| Wi-Fi connect + MQTT client | LED rendering (FastLED) |
| Frame encode/decode, CRC, version | HSB → RGB conversion |
| ACK, STATE_REPORT, DISCOVERY, ERROR | Current LED state (`LumiState`) |
| Zone (default, `SET_ZONE`, NVS) | NUM_LEDS + DATA_PIN NVS |
| DEVICE_ID from MAC, MQTT topics | `deviceName` derived from MAC |
| Reconnect loop | Re-`begin()` backoff on first connect |

## Modules

| Module | Responsibility |
|---|---|
| **main** | Boot sequence + cooperative `loop()`. Wires callbacks to `led`. Derives `deviceName` from the MAC. |
| **led** | FastLED setup and all strip output — power, brightness, HSB→RGB color, animations. Holds the current `LumiState`. |
| **ledconfig** | The firmware's own NVS (`lumi-led`): NUM_LEDS + DATA_PIN. First-boot Serial provisioning. |

There is no `mqtt` or `protocol` module — `LumiProtocol` replaces both.

## Getting started

### Prerequisites

- [PlatformIO Core](https://platformio.org/) (`pio`).
- A generic **ESP32 DevKit** (`esp32dev`) driving **WS2812B** strips (reference hardware).

### Setup

```bash
git clone https://github.com/dejarn/lumi-firmware.git
cd lumi-firmware
git submodule update --init --recursive
```

Create `src/secrets.h` (gitignored, identical fleet-wide):

```cpp
#pragma once
#define WIFI_SSID     "your-ssid"
#define WIFI_PASSWORD "your-password"
#define MQTT_HOST     "192.168.1.10"
#define MQTT_PORT     1883
#define MQTT_USER     "lumi"
#define MQTT_PASSWORD "your-mqtt-password"
```

> [!IMPORTANT]
> Pin the `vendor/lumi-protocol` submodule to the **same git tag as the mqtt-bridge** (e.g. `v1.0.0`). Bridge and firmware must agree on the protocol version.

### Build & flash

```bash
pio run                       # build esp32dev
pio run -t upload             # build + flash over USB
pio device monitor -b 115200  # serial monitor (provisioning, logs)
```

## Provisioning

The same binary ships to every board, so per-device LED config lives in NVS, set once at first boot.

A freshly flashed board has empty NVS and drops into Serial provisioning. Open the monitor at **115200 baud** and enter, when prompted:

- **NUM_LEDS** — how many LEDs are wired to this board.
- **DATA_PIN** — the GPIO driving the strip data line.

Both persist to the `lumi-led` NVS namespace; later boots skip this step. Reflashing does **not** clear NVS.

To re-provision, erase NVS first:

```bash
pio run -t erase
pio run -t upload
```

> [!NOTE]
> Zone is **not** provisioned here — the library owns it (default 0, set remotely by the bridge via `SET_ZONE`). DEVICE_ID and `deviceName` (e.g. `lumi-a3f1`) are derived automatically from the Wi-Fi MAC.

## Command path

The firmware sees only callbacks; the library does the rest:

| Callback | Signature | Firmware action |
|---|---|---|
| `onSetPower` | `void(bool on)` | strip on / blackout, keep last frame |
| `onSetBrightness` | `void(uint8_t brightness)` | FastLED master brightness |
| `onSetColor` | `void(uint16_t h, uint8_t s, uint8_t b)` | HSB→RGB, solid fill |
| `onSetAnimation` | `void(uint8_t animId, uint8_t speed, uint8_t intensity)` | start animation |
| `onStopAnimation` | `void()` | stop animation; re-render solid base color (avoids freezing on a flash/strobe blackout frame) |
| `onGetState` | `LumiState()` | return current `{power, brightness, h, s, colorBri, animId}` |

After each state-changing command the library emits ACK + STATE_REPORT, calling `onGetState()`. Animations (pulse, breathe, flash, strobe, rainbow) render locally via `millis()` deltas.

> [!WARNING]
> No `delay()` in `loop()`. The single cooperative loop services both `lumi.loop()` and animation rendering — any `delay()` stalls both. Use `millis()` deltas.

## Commands

| Command | Purpose |
|---|---|
| `pio run` | Build only |
| `pio run -t upload` | Build + flash |
| `pio device monitor -b 115200` | Serial monitor (provisioning, logs) |
| `pio run -t erase` | Erase flash incl. NVS (forces re-provisioning) |
| `pio run -t clean` | Clean build artifacts |

## Documentation

| Doc | What it covers |
|---|---|
| [`docs/vision.md`](docs/vision.md) | Scope, firmware-vs-library responsibility split, trade-offs |
| [`docs/architecture.md`](docs/architecture.md) | Module map, boot sequence, callbacks, rendering, resilience |
| [`docs/provisioning.md`](docs/provisioning.md) | First-boot NVS flow (NUM_LEDS, DATA_PIN) |
| [`docs/flashing.md`](docs/flashing.md) | Build, flash, submodule pinning |

## Related

- [lumi](https://github.com/dejarn/lumi) — the self-hosted platform (Next.js UI, automations, mqtt-bridge)
- [lumi-protocol](https://github.com/dejarn/lumi-protocol) — the binary MQTT protocol + Node.js/Arduino libraries
