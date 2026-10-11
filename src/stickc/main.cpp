// ESP-KVM M5StickC Plus2 — ambient RAM meter on an SK6812 flex strip.
// Reads "R<used%>\n" lines from USB serial (tools/ram-meter.sh) and shows RAM usage
// as a green->amber->red progress bar on the strip, plus the % on the StickC screen
// (double-buffered). A debug line shows received bytes to confirm the serial link.
#include <M5Unified.h>
#include <Adafruit_NeoPixel.h>
#include "common/version.h"

#ifndef RAM_LEDS
#define RAM_LEDS 30
#endif
#ifndef RAM_DATA_PIN
#define RAM_DATA_PIN 32
#endif

static Adafruit_NeoPixel strip(RAM_LEDS, RAM_DATA_PIN, NEO_GRB + NEO_KHZ800);
static M5Canvas canvas(&M5.Display);
static int      g_used = -1;
static uint32_t g_lastData = 0, g_rxBytes = 0;
static char     g_lastLine[24] = "";

static void colorFor(int u, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (u < 0) u = 0; if (u > 100) u = 100;
  if (u < 50) { r = (uint8_t)map(u, 0, 50, 0, 255); g = 255; }
  else        { r = 255; g = (uint8_t)map(u, 50, 100, 255, 0); }
  b = 0;
}
static void showStrip() {
  bool stale = (millis() - g_lastData > 6000) || g_used < 0;
  int used = g_used < 0 ? 0 : g_used;
  int lit  = (used * RAM_LEDS + 99) / 100; if (lit > RAM_LEDS) lit = RAM_LEDS;
  uint8_t r, g, b; colorFor(used, r, g, b);
  for (int i = 0; i < RAM_LEDS; i++) {
    if (!stale && i < lit)    strip.setPixelColor(i, strip.Color(r, g, b));
    else if (stale && i == 0) strip.setPixelColor(i, strip.Color(8, 8, 8));
    else                      strip.setPixelColor(i, 0);
  }
  strip.show();
}
static void drawScreen() {
  auto& d = canvas;
  d.fillScreen(TFT_BLACK);
  d.setTextColor(0x7FFF, TFT_BLACK); d.setTextSize(2);
  d.setCursor(6, 6); d.print("RAM");
  d.setTextColor(TFT_WHITE, TFT_BLACK); d.setTextSize(6);
  d.setCursor(8, 30);
  if (g_used < 0) d.print("--"); else d.printf("%d", g_used);
  d.setTextSize(2); d.print("%");
  int W = canvas.width();
  int x = 8, y = 86, w = W - 16, h = 16;
  d.drawRect(x, y, w, h, TFT_WHITE);
  int uu = g_used < 0 ? 0 : g_used;
  uint16_t c = uu < 50 ? 0x2FE6 : (uu < 80 ? 0xFD20 : 0xF800);
  d.fillRect(x + 1, y + 1, (w - 2) * uu / 100, h - 2, c);
  // debug line
  d.setTextSize(1); d.setTextColor(0x8C71, TFT_BLACK);
  d.setCursor(6, canvas.height() - 12);
  d.printf("rx:%lu [%s]", (unsigned long)g_rxBytes, g_lastLine);
  canvas.pushSprite(0, 0);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);
  Serial.begin(115200);

  canvas.createSprite(M5.Display.width(), M5.Display.height());
  strip.begin();
  strip.setBrightness(40);
  strip.clear(); strip.show();
  showStrip();
  drawScreen();
}

void loop() {
  M5.update();
  static String line;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    g_rxBytes++;
    if (ch == '\n' || ch == '\r') {
      if (line.length()) {
        strncpy(g_lastLine, line.c_str(), sizeof(g_lastLine) - 1);
        g_lastLine[sizeof(g_lastLine) - 1] = 0;
        if (line[0] == 'R' || line[0] == 'r') {
          int v = line.substring(1).toInt();
          if (v < 0) v = 0; if (v > 100) v = 100;
          g_used = v; g_lastData = millis();
        }
      }
      line = "";
    } else if (line.length() < 20) line += ch;
  }
  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 250) { lastDraw = millis(); showStrip(); drawScreen(); }
  delay(5);
}
