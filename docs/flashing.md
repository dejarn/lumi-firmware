# Flashing

_Last updated: 2026-06-05_

How to build lumi-firmware and flash it onto a board. Reference hardware: a generic **ESP32 DevKit** (`esp32dev`) driving **WS2812B** strips. Same binary for every device — per-board values (NUM_LEDS, DATA_PIN) are set afterward at first boot, see [provisioning.md](provisioning.md). Zone is not flashed or provisioned here — the bridge sets it remotely.

## Prerequisites

- [PlatformIO Core](https://platformio.org/) (`pio`) installed.
- The lumi-protocol submodule populated under `vendor/lumi-protocol`. If empty: `git submodule update --init --recursive`. The `LumiProtocol` library is exposed to the build via `lib_extra_dirs = vendor/lumi-protocol/device/arduino` in `platformio.ini`.
- `src/secrets.h` present (gitignored). Create it with your Wi-Fi + MQTT values:
  ```cpp
  #pragma once
  #define WIFI_SSID     "your-ssid"
  #define WIFI_PASSWORD "your-password"
  #define MQTT_HOST     "192.168.1.10"
  #define MQTT_PORT     1883
  #define MQTT_USER     "lumi"
  #define MQTT_PASSWORD "your-mqtt-password"
  ```

## Build

```
pio run
```

Compiles the `esp32dev` environment. First run downloads the toolchain and FastLED; `LumiProtocol` comes from the submodule and pulls in PubSubClient transitively (declared as its dependency).

## Flash

Connect the board over USB, then:

```
pio run -t upload
```

PlatformIO auto-detects the serial port. To force one:

```
pio run -t upload --upload-port COM5
```

## First boot — provisioning

A freshly flashed board has empty NVS and drops into Serial provisioning. Open the monitor at 115200 baud:

```
pio device monitor -b 115200
```

Enter NUM_LEDS and DATA_PIN when prompted. Values persist to NVS (`lumi-led`); later boots skip this step. Full flow: [provisioning.md](provisioning.md).

## Reflash

Reflashing the firmware does **not** clear NVS — the board keeps its existing config and goes straight to normal operation. To re-provision, erase NVS first:

```
pio run -t erase
pio run -t upload
```

## Submodule version

Pin the `vendor/lumi-protocol` submodule to the **same git tag as the mqtt-bridge** (e.g. `v1.0.0`), not a moving branch. Bridge and firmware must agree on the protocol version. To pin:

```
cd vendor/lumi-protocol
git fetch --tags
git checkout v1.0.0
cd ../..
git add vendor/lumi-protocol
```

## Common commands

| Command | Purpose |
|---|---|
| `pio run` | Build only |
| `pio run -t upload` | Build + flash |
| `pio device monitor -b 115200` | Serial monitor (provisioning, logs) |
| `pio run -t erase` | Erase flash incl. NVS (forces re-provisioning) |
| `pio run -t clean` | Clean build artifacts |
