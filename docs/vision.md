# Vision

_Last updated: 2026-06-05_

lumi-firmware is the firmware flashed onto every custom ESP32 LED strip controller in the Lumi system. A single binary runs the entire fleet. It is a **thin LED driver**: it consumes the `LumiProtocol` Arduino library, wires a handful of GPIO callbacks, and renders LED output. The library owns the network and the protocol — Wi-Fi, MQTT, framing, ACK, discovery, and zone state. This repo owns the LEDs.

## Problem

The Lumi platform talks to its custom LED strips over MQTT using the lumi-protocol binary framing. Something has to run *on* the ESP32 to turn those commands into light: drive the physical strip, track current state, and report it back.

lumi-protocol already provides that plumbing as a ready-made device library (`LumiProtocol`): it connects Wi-Fi and MQTT, parses and validates frames, emits ACK / STATE_REPORT / DISCOVERY, and persists zone membership in NVS. What it deliberately does **not** know is anything about LEDs — pin, strip length, colors, animations. That gap is lumi-firmware.

The constraints are embedded ones. Each board is headless — no buttons, no screen — yet must be configurable per unit (strip length, data pin). The same image ships to every device, so LED config lives in non-volatile storage, set once at provisioning. The firmware has to be small, predictable, and reliable enough to mount behind furniture and forget.

## Out of scope

- **The protocol and its transport** — byte layout, opcodes, CRC, MQTT topics, ACK, discovery, and Wi-Fi/MQTT connection live in [lumi-protocol](https://github.com/dejarn/lumi-protocol) and its `LumiProtocol` library. This firmware never opens an MQTT client, builds a frame, or emits an ACK itself.
- **Zone membership** — owned by the library: default 0, changed remotely via `SET_ZONE`, persisted in the library's NVS namespace. The firmware does not provision or store the zone.
- **Platform logic** — scenes, triggers, device registry, UI belong to [lumi](https://github.com/dejarn/lumi). The ESP32 only executes commands and reports state.
- **OTA firmware updates** — flashing is done over USB. Remote update is deferred.
- **Physical input** — no buttons, switches, or sensors on these boards.
- **Custom animation sequences** — v1 ships a fixed set of predefined animations. User-defined sequences are deferred.
- **Non-LED hardware** — audio, HVAC, displays, motors are out.
- **Multi-strip per board** — one ESP32 drives exactly one strip.

## Core concepts

| Concept | Definition |
|---|---|
| **Thin LED driver** | The firmware is callbacks + rendering. `LumiProtocol` owns the network and protocol; the firmware reacts to decoded commands and drives the strip. |
| **Single binary** | One firmware image runs the whole fleet. No per-device builds — LED variation is in NVS. |
| **LED provisioning** | First-boot setup over the Serial Monitor when the firmware's NVS is empty. The operator enters NUM_LEDS and DATA_PIN; the firmware persists them in its own NVS namespace (`lumi-led`). See [provisioning.md](provisioning.md). |
| **Callbacks** | The firmware registers handlers the library invokes per decoded command: power, brightness, color (HSB), animation, stop, and a `getState` that returns current LED state. |
| **HSB → RGB** | Commands carry color as HSB (hue 0–65535, sat, bri). The firmware converts to RGB for FastLED. |
| **Animation** | A predefined effect (pulse, breathe, flash, strobe, rainbow) rendered locally each loop via FastLED, parameterised by the command's speed and intensity. |
| **Identity** | DEVICE_ID (2 bytes) is derived by the library from the Wi-Fi MAC. The firmware derives a matching `deviceName` (e.g. `lumi-a3f1`) from the MAC for discovery. |

## Actors

| Actor | Role |
|---|---|
| **ESP32 (this firmware)** | Drives the strip, tracks LED state, converts HSB→RGB, renders animations. The deployable LED driver. |
| **`LumiProtocol` library** | Owns Wi-Fi, MQTT, framing, ACK, discovery, zone NVS. Invokes the firmware's callbacks. Vendored as a git submodule. |
| **Raspberry Pi (mqtt-bridge)** | Sends commands, sets zone via `SET_ZONE`, tracks state. The fleet's counterpart. |
| **Operator** | Flashes and provisions a board over USB at first boot. Sets NUM_LEDS + DATA_PIN once. |

## Libraries consumed

| Library | Role |
|---|---|
| **lumi-protocol** (`device/arduino`, `LumiProtocol`) | Wi-Fi + MQTT + framing + ACK + discovery + zone NVS. Git submodule, exposed via `lib_extra_dirs`. |
| **FastLED** | LED rendering and animation primitives. |
| **PubSubClient** | MQTT client — pulled in transitively as a `LumiProtocol` dependency, not driven directly by the firmware. |

## Accepted trade-offs

- **Thin firmware over the framework** — the firmware delegates all network and protocol behavior to `LumiProtocol`. Less control over the connection lifecycle, but protocol *behavior* (topics, ACK, discovery, zone) versions in one place with the spec. The firmware's value is LED driving, and it stays focused there.
- **Single binary, NVS-driven LED config** — no per-device firmware variants. NUM_LEDS and DATA_PIN are provisioned, never compiled. One image to build, test, and flash.
- **Serial provisioning** — headless boards have no other input channel. First boot over USB is a one-time manual step, accepted over the complexity of a captive-portal or BLE setup flow.
- **DATA_PIN in NVS** — boards may be wired to different GPIOs; storing the pin avoids rebuilds for hardware variation.
- **Zone owned by the library** — the firmware does not provision zone. Default 0 until the bridge sends `SET_ZONE`; the library persists it. One owner, no double source.
- **deviceName from MAC** — automatic, no extra provisioning value. The platform can rename a device on its side.
- **Predefined animations only (v1)** — rendered locally on the ESP32. Keeps the loop simple; custom sequences deferred.
- **USB flashing, no OTA (v1)** — the fleet is small and physically accessible. Remote update is not worth the firmware complexity yet.
