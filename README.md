# ESP-KVM

A wireless macro deck + trackpad for **your own** computer, built from ESP32 gear.

- **Dongle** — LilyGO **T-Dongle S3** plugs into your PC's USB port and enumerates as a
  USB keyboard + mouse.
- **Controller** — M5Stack **CoreS3** touchscreen is the wireless remote (LVGL UI).
- **Legend** — M5Paper (e-ink) shows the active profile's button map + an action log.
- **Guard** — Core2 + M5Stack UnitV2 AI camera: presence-aware auto-lock (face gone -> Lock).
- **RAM meter** — M5StickC Plus2 drives an SK6812 strip as an ambient Mac RAM gauge.
- **Link** — ESP-NOW, no Wi-Fi router required.

Tap a touch button to fire a macro (key combo, text snippet, media key), or use the
trackpad screen to drive the mouse. Think DIY Elgato Stream Deck.

> **Scope:** "KVM" here means keyboard + mouse + macros, *not* video. This controls
> *your own* machine — a productivity tool, not an attack device.

## Hardware

| Role | Board | Display | Link |
|------|-------|---------|------|
| Dongle | LilyGO T-Dongle S3 (ESP32-S3) | 0.96" ST7735 160x80 | USB-HID + ESP-NOW |
| Controller | M5Stack CoreS3 (ESP32-S3) | 2.0" 320x240 cap-touch | ESP-NOW |
| Legend (optional) | M5Paper (ESP32) | 4.7" e-ink 540x960 | ESP-NOW |
| Guard (optional) | M5Stack Core2 (ESP32) + UnitV2 AI camera | 2.0" 320x240 | ESP-NOW + UART |
| RAM meter (optional) | M5StickC Plus2 (ESP32) + SK6812 strip | 1.14" + 30 LEDs | USB serial |

## Build (PlatformIO)

```bash
pio run                      # compile both environments
pio run -e dongle            # compile just the dongle
pio run -e controller        # compile just the controller
pio run -e paper             # compile the M5Paper e-ink legend
pio run -e core2             # compile the Core2 UnitV2 guard bridge
pio run -e stickc            # compile the StickC Plus2 RAM meter
pio run -e controller -t upload   # flash the CoreS3 (USB-C)
pio run -e dongle -t upload       # flash the dongle (enter BOOT download mode first)
```

Or open the folder in VS Code with the **PlatformIO IDE** extension for a GUI — same project.

> **Dongle flashing:** the T-Dongle S3 has no auto-reset. Hold **BOOT** while plugging
> in to flash, then **unplug/replug** to run the new firmware. See
> [docs/wiring.md](docs/wiring.md). The CoreS3 flashes normally over USB-C.

## Toolchain

Uses the [pioarduino](https://github.com/pioarduino/platform-espressif32) platform fork
(arduino-esp32 3.x), required for ESP32-S3 native USB-HID and M5Unified.

## Status

Early development — see [CHANGELOG.md](CHANGELOG.md). Hardware notes in [docs/wiring.md](docs/wiring.md).
