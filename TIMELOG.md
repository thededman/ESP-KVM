# Time Log

One line per working day: `YYYY-MM-DD — Xh — summary`.

At each release, cross-check against measured hands-on Claude time:
`python3 ~/.claude/scripts/claude-hours.py ESP-KVM`
(Session timestamps capture Claude time only — add meetings, bench testing, and
flashing/hardware work here manually.)

## 2026

- 2026-10-11 — 3.9h — ESP-KVM built end-to-end in one session. PlatformIO
  (pioarduino / arduino-esp32 3.x) multi-env repo + private GitHub + changelog.
  Dongle: USB-HID + ESP-NOW coexistence and an ST7735 status LCD. CoreS3: LVGL
  swipeable macro deck (anim + page dots). M5Paper: e-ink legend/log. Core2 + UnitV2
  AI camera: presence-aware auto-lock. M5StickC Plus2 + SK6812 strip: ambient, switchable
  Mac system monitor (MEM/GPU/CPU/DISK) via button or CoreS3 Monitor page. All six
  nodes verified on hardware.
  (3.9h = measured Claude hands-on time; add any offline bench testing.)
