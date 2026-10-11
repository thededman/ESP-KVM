# Changelog

All notable changes to ESP-KVM are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Project scaffold: PlatformIO dual-environment build (`dongle` + `controller`) on the
  pioarduino platform (arduino-esp32 3.x).
- Shared ESP-NOW wire protocol (`lib/espkvm_proto`): versioned packet, key-combo /
  type-string / mouse / media command types.
- Phase 1 coexistence spike: T-Dongle S3 runs a USB-HID keyboard and ESP-NOW together
  and types live tx/rx counters on a BOOT-button press.
- CoreS3 boot splash via M5Unified.
- **Phase 2 — wireless control link:** dongle is now an ESP-NOW receiver that
  injects commands as USB keyboard/mouse (`TYPE_STRING`, `KEY_COMBO`, mouse move/
  button/scroll); CoreS3 is a touch macro deck (portrait) with 6 buttons — Hello,
  Spotlight, Copy, Paste, Lock, Screenshot — sending over ESP-NOW broadcast (ch 1).
- **Phase 4 (in progress) — LVGL UI:** CoreS3 touch deck rebuilt in LVGL with a
  status bar (battery), and three swipeable profile pages (Dev / System / Snippets)
  of styled rounded buttons. Shared special-key codes added to the protocol.
- **M5Paper e-ink node:** an `ANNOUNCE` protocol message + a shared macro table
  (`espkvm_macros.h`) let the CoreS3 tell the M5Paper the active profile and fired
  button; the M5Paper renders the profile's button legend and a log of recent
  actions. The dongle ignores `ANNOUNCE`.

### Fixed
- Dongle build: strip the board's `ARDUINO_USB_MODE=1` via `build_unflags` so TinyUSB
  (OTG) HID mode (`=0`) takes effect; add the bundled USB library's missing `FS`
  include path; correct the ESP-NOW `recv`/`send` callback signatures for
  arduino-esp32 3.x (`esp_now_recv_info_t*` / `wifi_tx_info_t*`).
- Dropped the `qio_opi` PSRAM `memory_type` (not needed; was a boot-loop risk).

### Verified
- **Phase 1 coexistence proven on hardware (2026-10-11):** the T-Dongle S3 enumerates
  as a USB-HID keyboard and runs ESP-NOW simultaneously; a BOOT-button press typed
  `...USB-HID + ESP-NOW coexist. tx=155 rx=0` into a macOS text editor.
- **Phase 2 end-to-end proven on hardware (2026-10-11):** tapping *Hello* on the
  CoreS3 typed `Hello from ESP-KVM!` into the Mac via the dongle — full touch →
  ESP-NOW → USB-HID path working.
- **Phase 4 LVGL deck verified on hardware (2026-10-11):** swipe between profiles
  and button taps fire commands to the Mac.
- **M5Paper legend verified on hardware (2026-10-11):** the e-ink legend tracks the
  CoreS3's active profile and updates on swipe.
