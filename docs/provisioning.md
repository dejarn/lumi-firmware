# Provisioning

How a fresh Lumi ESP32 gets its per-device LED configuration.

## What is stored

The firmware keeps two values in its own NVS namespace (`lumi-led`), surviving reboots and reflashes:

- **NUM_LEDS** (`uint32`) — how many LEDs are wired to this board.
- **DATA_PIN** (`uint8`) — the GPIO driving the LED data line on this board.

That is all the firmware provisions. Two things are deliberately **not** here:

- **Zone** — owned by the `LumiProtocol` library, in its own NVS namespace (`lumi`/`zone`). It defaults to 0 and is set remotely by the bridge via the `SET_ZONE` command. Never entered at the Serial prompt.
- **Device ID / name** — derived automatically from the Wi-Fi MAC at boot (the library computes the 2-byte DEVICE_ID; the firmware derives a matching `deviceName` like `lumi-a3f1`). No manual step.

## First boot (NVS empty)

The same firmware binary runs on every device, so a brand-new board has no LED config. On boot the firmware checks whether its `lumi-led` NVS already holds a complete config:

- **Provisioned** → load NUM_LEDS and DATA_PIN, continue to normal operation.
- **Unprovisioned** → drop into the Serial provisioning flow.

## Serial provisioning flow

1. Plug the board into a computer and open the Serial Monitor at **115200 baud**.
2. The firmware detects the empty `lumi-led` NVS and prints prompts.
3. Enter the number of LEDs when asked, then the data pin.
4. The firmware validates the input and writes both values to NVS.
5. It confirms over Serial and proceeds to normal startup (Wi-Fi + MQTT via the library).

Because the values now live in NVS, later reboots skip provisioning entirely. The device only asks again if the `lumi-led` NVS is cleared.

## Reprovisioning

To change NUM_LEDS or DATA_PIN, clear the `lumi-led` NVS namespace (or do a full flash erase) so the next boot is treated as a first boot and the Serial flow runs again.

> A full flash erase also wipes the library's `lumi`/`zone` namespace, so the device restarts at zone 0 and waits for the bridge to re-send `SET_ZONE`.
