# Changelog

All notable changes to Nexus are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- **Renamed the project to Nexus**, and the M5Paper e-ink remote to **Slate** &mdash; on-device
  titles (dongle, CoreS3, Core2 "Guard", the remote), the README, docs, and the GitHub repo.
  Internal code identifiers (`espkvm_proto`, `namespace espkvm`, `ESPKVM_VERSION`) are unchanged.
- Added a project **showcase** (`docs/index.html`) published via **GitHub Pages**.

## [0.2.0] - 2026-10-11

### Added
- **Media / volume keys:** the dongle adds USB consumer-control — Vol+/-, Mute,
  Play/Pause, Next, Prev (`Cmd::MEDIA`).
- **M5Paper touch remote:** the e-ink is now an interactive, paginated touch remote
  (FreeSans fonts) with Media / TV / Mac / Monitor buttons; the side wheel (BtnA/BtnB/
  BtnC) flips pages. Sends MEDIA / KEY_COMBO / IR / SET_METRIC over ESP-NOW.
- **IR TV control:** the StickC Plus2 is an IR blaster (IRremoteESP8266, GPIO19) driven
  by `Cmd::IR`. Verified powering a Hisense-made Sharp LC-50P7000U with official Hisense
  NEC codes via `encodeNEC`.
- **Wireless system monitor:** the CoreS3 relays Mac stats from its USB serial to the
  StickC over ESP-NOW (`Cmd::STATS`), so the StickC runs untethered on battery. Host
  script `tools/sysmon.sh`.

### Changed
- Moved the stats path off a dongle USB HID+CDC composite (unreliable — the HID
  interface dropped) to the CoreS3 relay; the dongle stays a clean HID device.

### Verified
- IR TV Power toggled the TV on hardware; M5Paper metric switching (MEM/GPU/CPU) works.

## [0.1.0] - 2026-10-11

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
- **Core2 + UnitV2 guard node:** the Core2 reads face detections from the M5Stack
  UnitV2 AI camera (JSON over PORT.A UART, 115200) and does presence-aware auto-lock
  — after 15s with no face while ARMED, it fires System > Lock over ESP-NOW. On-screen
  ARM/DISARM toggle; robust JSON parse (num / face / obj / faces).
- **System monitor node:** M5StickC Plus2 drives a 30-LED SK6812 flex strip as an
  ambient Mac gauge, switchable between **MEM / GPU / CPU / DISK** via the StickC's
  Button A or a new CoreS3 **Monitor** page (over ESP-NOW, `SET_METRIC`). Host script
  `tools/sysmon.sh` streams all four metrics over USB serial. Double-buffered displays;
  CoreS3 gains a 4th swipe page + dot.

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
- **Dongle status LCD verified on hardware (2026-10-11):** the T-Dongle S3's ST7735
  shows the last action + packet count and updates on each tap; BOOT toggles HID.
- **Guard node verified on hardware (2026-10-11):** the UnitV2 detects a face, the
  Core2 tracks PRESENT/AWAY, and arming + walking away locked the Mac via the dongle.
- **System monitor verified on hardware (2026-10-11):** StickC Button A cycles
  MEM/GPU/CPU/DISK and the CoreS3 Monitor page switches the metric over ESP-NOW.

[Unreleased]: https://github.com/thededman/Nexus/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/thededman/Nexus/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/thededman/Nexus/releases/tag/v0.1.0
