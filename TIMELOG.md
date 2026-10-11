# Time Log

One line per working day: `YYYY-MM-DD — Xh — summary`.

At each release, cross-check against measured hands-on Claude time:
`python3 ~/.claude/scripts/claude-hours.py ESP-KVM`
(Session timestamps capture Claude time only — add meetings, bench testing, and
flashing/hardware work here manually.)

## 2026

- 2026-10-11 — 1.0h — Kickoff. Chose ESP-KVM wireless macro deck (T-Dongle S3 USB-HID
  dongle + CoreS3 LVGL remote over ESP-NOW). Verified hardware capabilities, scaffolded
  PlatformIO dual-env repo, shared protocol, Phase 1 USB-HID+ESP-NOW coexistence spike,
  CoreS3 boot splash. (Hours are an estimate — adjust to actual.)
- 2026-10-11 — 1.5h — Hardware bring-up. Installed PlatformIO (pioarduino / arduino-esp32
  3.3.12), got both envs compiling, created private GitHub repo (thededman/ESP-KVM) and
  pushed. Flashed the dongle and debugged USB enumeration: learned the T-Dongle S3 needs
  a physical power-cycle to run new firmware (no auto-reset) and BOOT-hold to flash.
  Confirmed Phase 1 on hardware — USB-HID keyboard + ESP-NOW coexist (typed tx=155).
  (Estimate — adjust to actual.)
- 2026-10-11 — 1.0h — Phase 2 built & verified. Dongle receiver (ESP-NOW -> USB HID
  keyboard/mouse) + CoreS3 touch macro deck (6 buttons, portrait). Flashed both;
  confirmed end-to-end on hardware: CoreS3 "Hello" tap typed into the Mac. Noted a
  USB-hub flashing hiccup (retry fixed it) and both device MACs for pairing.
  (Estimate — adjust to actual.)
