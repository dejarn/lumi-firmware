# Architecture

_Last updated: 2026-06-05_

lumi-firmware is a single-binary Arduino/ESP32 program built with PlatformIO. It is a thin driver on top of the `LumiProtocol` library: the library owns Wi-Fi, MQTT, framing, ACK, discovery, and zone state; the firmware registers GPIO callbacks and renders the LED strip. One cooperative `loop()` services the library and advances the animation, both non-blocking.

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
   │  reconnect               │   └──────────────────────────────┘
   └──────────────────────────┘                ▲
                                                │ numLeds, dataPin
                                     ┌──────────┴───────────┐
                                     │       ledconfig      │
                                     │  NVS "lumi-led"      │
                                     │  Serial provisioning │
                                     └──────────────────────┘
```

The firmware never touches MQTT, frames, or ACK directly — that is entirely inside the library. The firmware's surface is: provide config to `begin()`, react to callbacks, render.

## Modules

| Module | Responsibility |
|---|---|
| **main** | Boot sequence and the cooperative loop. Wires callbacks to `led`. Derives `deviceName` from the MAC. |
| **led** | FastLED setup and all strip output — power, brightness, HSB→RGB color, animations. Holds the current `LumiState` for `onGetState`. |
| **ledconfig** | The firmware's own NVS (namespace `lumi-led`): NUM_LEDS + DATA_PIN. First-boot Serial provisioning. |

There is **no** `mqtt` or `protocol` module — `LumiProtocol` replaces both. `led` knows nothing about MQTT or framing; it only renders and tracks state.

## Library responsibility split

| Owned by `LumiProtocol` (lib) | Owned by firmware |
|---|---|
| Wi-Fi connect + MQTT client | LED rendering (FastLED) |
| Frame encode/decode, CRC, version | HSB → RGB conversion |
| ACK, STATE_REPORT, DISCOVERY, ERROR | Current LED state (`LumiState`) |
| Zone: default, `SET_ZONE`, NVS (`"lumi"`/`"zone"`) | NUM_LEDS + DATA_PIN NVS (`"lumi-led"`) |
| DEVICE_ID from MAC, MQTT topics | `deviceName` derived from MAC |
| Reconnect loop (every 5 s) | Re-`begin()` backoff on initial Wi-Fi failure |

## Boot sequence (`setup()`)

1. `Serial.begin(115200)`.
2. If the `lumi-led` NVS is empty → Serial provisioning (prompt NUM_LEDS, DATA_PIN, persist).
3. Load NUM_LEDS + DATA_PIN from NVS.
4. `ledInit(numLeds, dataPin)` — allocate the CRGB buffer, register FastLED on the provisioned GPIO.
5. Derive `deviceName` from the Wi-Fi MAC (e.g. `lumi-a3f1`).
6. Wire callbacks (`onSetPower`, `onSetBrightness`, `onSetColor`, `onSetAnimation`, `onStopAnimation`, `onGetState`) to `led` — **before** `begin()` so no command arriving immediately after subscribe is ACK'd without effect.
7. Connect: loop `lumi.begin(WIFI_SSID, WIFI_PASS, MQTT_HOST, deviceName, MQTT_PORT, MQTT_USER, MQTT_PASSWORD)` with backoff until it returns true (see Resilience).

## Loop (`loop()`)

Cooperative, single-threaded, no `delay()`:

1. `lumi.loop()` — services the MQTT client and reconnects if dropped. Decoded commands fire the firmware's callbacks synchronously from here.
2. Render one animation frame if the per-animation interval has elapsed (see Rendering).

A `delay()` anywhere would stall both `lumi.loop()` and rendering — forbidden.

## Command path (library → firmware)

The firmware sees only callbacks; the library does the rest:

```
broker → LumiProtocol (parse, CRC, version check, dispatch)
              │
              ▼  one of:
   onSetPower(bool) · onSetBrightness(uint8) · onSetColor(h,s,b)
   onSetAnimation(id, speed, intensity) · onStopAnimation()
              │
              ▼
   led applies it (FastLED) + updates current LumiState
              │
              ▼
   library emits ACK + STATE_REPORT automatically (calls onGetState)
```

The firmware does **not** build or publish ACK / STATE_REPORT — the library does, calling `onGetState()` after each state-changing command. `GET_STATE`, `SET_ZONE`, and discovery are handled entirely inside the library.

### Callback signatures (from `LumiProtocol.h`)

| Callback | Signature | Firmware action |
|---|---|---|
| `onSetPower` | `void(bool on)` | strip on / blackout, keep last frame |
| `onSetBrightness` | `void(uint8_t brightness)` | FastLED master brightness |
| `onSetColor` | `void(uint16_t h, uint8_t s, uint8_t b)` | HSB→RGB, solid fill |
| `onSetAnimation` | `void(uint8_t animId, uint8_t speed, uint8_t intensity)` | start animation |
| `onStopAnimation` | `void()` | stop animation; re-render solid base color (avoids freezing on a flash/strobe blackout frame) |
| `onGetState` | `LumiState()` | return current `{power, brightness, h, s, colorBri, animId}` |

## State tracking

`led` keeps a `LumiState` (power, brightness, hue, sat, colorBri, animId) updated on every command, and returns it from `onGetState`. This is the firmware's authoritative LED state; the library forwards it to the bridge in STATE_REPORT frames.

## Rendering

- Animations are driven by `millis()` deltas, not `delay()` or spin-loops.
- Each animation carries a runtime `frameIntervalMs` derived from the command's speed parameter. Each `loop()` pass: if `millis() - lastFrame >= frameIntervalMs`, advance one frame and `FastLED.show()`.
- A runtime interval is why the firmware uses an explicit `millis()` check rather than FastLED's `EVERY_N_MILLISECONDS` (which fixes the interval at compile time).
- Animation IDs match the library constants: PULSE, BREATHE, FLASH, STROBE, RAINBOW.
- Solid color / power / brightness are immediate writes followed by `FastLED.show()` — no animation loop involved.

## Configuration & identity

| Value | Owner | Source | Notes |
|---|---|---|---|
| NUM_LEDS | firmware | NVS `lumi-led` | Strip length. Sizes the CRGB buffer. |
| DATA_PIN | firmware | NVS `lumi-led` | GPIO driving the strip. See FastLED pin note below. |
| ZONE_ID | library | NVS `lumi`/`zone` | Default 0, set via remote `SET_ZONE`. Not the firmware's concern. |
| DEVICE_ID | library | Wi-Fi MAC (last 2 bytes) | Used in MQTT topics and client id. |
| deviceName | firmware | derived from MAC | Passed to `begin()`, used in discovery (e.g. `lumi-a3f1`). |
| Wi-Fi / MQTT creds | firmware | `src/secrets.h` | Passed into `begin()`. Gitignored, same fleet-wide. |

**FastLED pin caveat:** `FastLED.addLeds<CHIPSET, PIN>()` requires PIN as a compile-time template argument. A runtime `dataPin` from NVS cannot be passed directly — `ledInit` dispatches via a `switch(dataPin)` over the supported GPIO set, each branch calling `addLeds` with a literal pin.

## Resilience

- **Runtime Wi-Fi / MQTT drop:** owned by the library. `lumi.loop()` detects disconnect and reconnects every 5 s. The strip holds its last rendered frame meanwhile.
- **Initial connection failure:** `lumi.begin()` blocks up to 30 s for Wi-Fi and returns `false` on timeout. The library's reconnect loop does *not* cover this pre-`begin()` case, so the firmware re-calls `begin()` with backoff until it succeeds. No reboot.
- **Provisioning is sticky:** LED config lives in NVS `lumi-led`; a reboot never re-prompts unless that NVS is erased. (A full flash erase also clears the library's zone NVS → device restarts at zone 0.)

## Threat model / accepted trade-offs

| Threat | Detail | Accepted trade-off |
|---|---|---|
| **MQTT in cleartext** | Traffic travels over TCP port 1883 with no TLS. Any host on the same LAN can sniff or inject frames. | Acceptable on a private home LAN. The MQTT broker (Mosquitto) requires username/password auth, preventing trivial unauthenticated injection. TLS (`WiFiClientSecure` + cert) is a future backlog item. |
| **Secrets compiled into the binary** | `WIFI_SSID`, `WIFI_PASSWORD`, `MQTT_HOST`, `MQTT_USER`, `MQTT_PASSWORD` are `#define`d in `src/secrets.h`, which is compiled into the binary. The plaintext credentials can be extracted via `esptool read_flash`. | Acceptable on hardware physically controlled by the flat. Flash encryption is a future backlog item. |
| **Single shared fleet credential** | All ESP32 devices use the same MQTT username/password (`lumi-fleet`). A compromised credential affects the whole strip fleet simultaneously. Rotation requires re-flashing every device (compile-time constant). | Accepted. Per-device credentials would require per-device builds, defeating the single-binary principle. Mitigated by: (a) using a dedicated `lumi-fleet` broker user distinct from the bridge's own credential, enabling independent rotation; (b) limiting broker ACLs to the `lumi/` topic namespace only. See also `src/secrets.h.example`. |
| **LAN co-location** | Other devices on the same flat LAN (residents' phones, IoT devices) can reach the broker on port 1883. | Accepted. Mosquitto mandatory auth blocks unauthenticated access. A Mosquitto ACL limits `lumi-fleet` to `lumi/#` only. Physical network segregation (VLAN) is out of scope for a home flat. |

**Backlog (not planned):** `WiFiClientSecure` + broker certificate, ESP32 flash encryption, OTA-safe credential rotation.

## Broker outage behaviour

When the MQTT broker is unreachable, `lumi.loop()` calls `_reconnectMqtt()` (inside the library) approximately every 5 s. Each reconnection attempt blocks the cooperative loop for up to **~15 s** (TCP connect timeout). During this window:

- The animation loop does not advance; the strip holds its last rendered frame.
- No callbacks fire (no commands can arrive).
- The device resumes normal operation immediately once the broker is reachable again.

This "stall" is a known limitation of the current synchronous reconnect implementation inside `LumiProtocol`. A fix (e.g. `_mqtt.setSocketTimeout(1)` or a state-machine reconnect) will be delivered in a future library release and picked up via the next submodule bump.

## Build

| Aspect | Choice |
|---|---|
| Build system | PlatformIO |
| Board / framework | `esp32dev` / Arduino |
| Libraries | FastLED + PubSubClient (`lib_deps`, pinned); `LumiProtocol` via `lib_extra_dirs = vendor/lumi-protocol/device/arduino` (git submodule). PubSubClient is listed explicitly in `lib_deps` because the LDF does not resolve transitive dependencies under `lib_extra_dirs`. |
| Secrets | `src/secrets.h` (gitignored) — WIFI_SSID, WIFI_PASSWORD, MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASSWORD |

Flashing and wiring: see [flashing.md](flashing.md).

## Partition scheme

**No OTA in v1.** The firmware uses the ESP32 default partition table (`default.csv`: single app partition + NVS + SPIFFS). OTA (`default_ota.csv`) would halve the available app flash per slot, and USB flashing is acceptable for a small controlled fleet. Switching to an OTA partition table requires a full flash erase (re-provisioning). If OTA becomes a requirement, the decision should be made before a fleet grows — the migration cost scales with device count. Document the choice here when made.
