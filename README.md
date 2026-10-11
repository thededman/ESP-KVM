# ESP-KVM

A wireless macro deck + trackpad for **your own** computer, built from ESP32 gear.

- **Dongle** — LilyGO **T-Dongle S3** plugs into your PC's USB port and enumerates as a
  USB keyboard + mouse.
- **Controller** — M5Stack **CoreS3** touchscreen is the wireless remote (LVGL UI).
- **Link** — ESP-NOW (encrypted), no Wi-Fi router required.

Tap a touch button to fire a macro (key combo, text snippet, media key), or use the
trackpad screen to drive the mouse. Think DIY Elgato Stream Deck.

> **Scope:** "KVM" here means keyboard + mouse + macros, *not* video. This controls
> *your own* machine — a productivity tool, not an attack device.

## Hardware

| Role | Board | Display | Link |
|------|-------|---------|------|
| Dongle | LilyGO T-Dongle S3 (ESP32-S3) | 0.96" ST7735 160x80 | USB-HID + ESP-NOW |
| Controller | M5Stack CoreS3 (ESP32-S3) | 2.0" 320x240 cap-touch | ESP-NOW |

## Build (PlatformIO)

```bash
pio run                      # compile both environments
pio run -e dongle            # compile just the dongle
pio run -e controller        # compile just the controller
pio run -e controller -t upload   # flash the CoreS3 (USB-C)
pio run -e dongle -t upload       # flash the dongle (enter BOOT download mode first)
```

Or open the folder in VS Code with the **PlatformIO IDE** extension for a GUI — same project.

## Toolchain

Uses the [pioarduino](https://github.com/pioarduino/platform-espressif32) platform fork
(arduino-esp32 3.x), required for ESP32-S3 native USB-HID and M5Unified.

## Status

Early development — see [CHANGELOG.md](CHANGELOG.md). Hardware notes in [docs/wiring.md](docs/wiring.md).
