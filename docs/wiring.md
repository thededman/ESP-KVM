# Wiring & hardware notes

## LilyGO T-Dongle S3 (dongle)
- ESP32-S3, USB-A plug wired to native USB (D+/D-) -> enumerates as USB-HID.
- Flashing: hold **BOOT** while plugging in / resetting to enter download mode
  (single USB port can't run HID and flash at once).
- ST7735 0.96" 160x80 LCD pins: CS=4, DC=2, RST=1, SDA(MOSI)=3, SCL(SCLK)=5, BL=38.
- Onboard APA102 RGB LED + BOOT button on GPIO0.

## M5Stack CoreS3 (controller)
- ESP32-S3, 2.0" 320x240 capacitive touch (ILI9342C + FT6336).
- Power (AXP2101) and reset/backlight (AW9523) are on I2C expanders — handled by
  M5Unified; do not drive those pins directly.

## Link
- ESP-NOW on a fixed channel (currently 1). Both devices must share the channel.

## Flashing & running the dongle (IMPORTANT)

The T-Dongle S3 has **no auto-reset circuit**, so the usual DTR/RTS reset does not
work. The workflow is manual:

1. **To flash:** hold **BOOT**, plug the dongle into USB (release BOOT) — this enters
   download mode and a `/dev/cu.usbmodem*` serial port appears. Then upload:
   `pio run -e dongle -t upload --upload-port /dev/cu.usbmodemXXXX`
2. **To run the new firmware:** **unplug and replug** (plain, no BOOT). `esptool`'s
   post-flash reset does *not* start the app — only a real power cycle does.

Once the firmware runs, it switches USB to TinyUSB/OTG and the serial port disappears
(it's now a HID keyboard). On first connect macOS may show the **Keyboard Setup
Assistant** — just close it; typing still works.

> Phase 5 adds OTA so routine updates skip this dance.

### Verified (2026-10-11)
USB-HID + ESP-NOW coexistence confirmed on hardware: pressing BOOT typed
`...spike OK: USB-HID + ESP-NOW coexist. tx=155 rx=0` — the climbing `tx` counter
proves the radio runs concurrently with the USB keyboard. Dongle MAC: `F0:9E:9E:29:E6:CC`.
