# Time Log

One line per working day: `YYYY-MM-DD — Xh — summary`.

At each release, cross-check against measured hands-on Claude time:
`python3 ~/.claude/scripts/claude-hours.py ESP-KVM`
(Session timestamps capture Claude time only — add meetings, bench testing, and
flashing/hardware work here manually.)

## 2026

- 2026-10-11 — 2.2h — ESP-KVM built end-to-end in one session. Set up PlatformIO
  (pioarduino / arduino-esp32 3.x) triple-env repo + private GitHub + changelog.
  Phase 1: USB-HID + ESP-NOW coexistence on the T-Dongle S3 (hardware-verified,
  learned its no-auto-reset flash/run quirk). Phase 2: CoreS3 touch -> ESP-NOW ->
  dongle -> Mac control link. Phase 4: LVGL swipeable macro deck (Dev/System/
  Snippets). Added the M5Paper e-ink legend/log node. All three verified on
  hardware. (2.2h = measured Claude hands-on time; add any offline bench testing.)
