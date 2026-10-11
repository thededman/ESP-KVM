// ESP-KVM controller — Phase 0 boot splash.
//
// Minimal M5Unified bring-up: initialises the CoreS3 (display, touch, AXP2101
// power, AW9523 IO expander) and shows a splash. LVGL and the macro UI arrive
// in Phase 4. Colours are raw RGB565 to avoid depending on colour-name macros.

#include <M5Unified.h>
#include "common/version.h"

static constexpr uint16_t COL_BLACK = 0x0000;
static constexpr uint16_t COL_WHITE = 0xFFFF;
static constexpr uint16_t COL_CYAN  = 0x07FF;

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  M5.Display.setRotation(1);               // 320x240 landscape
  M5.Display.fillScreen(COL_BLACK);

  M5.Display.setTextColor(COL_CYAN, COL_BLACK);
  M5.Display.setTextSize(3);
  M5.Display.setCursor(70, 95);
  M5.Display.print("ESP-KVM");

  M5.Display.setTextColor(COL_WHITE, COL_BLACK);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(70, 140);
  M5.Display.print("controller " ESPKVM_VERSION);
}

void loop() {
  M5.update();   // services touch + buttons; UI comes in later phases
  delay(10);
}
