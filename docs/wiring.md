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
