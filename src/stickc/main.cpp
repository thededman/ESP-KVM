// ESP-KVM M5StickC Plus2 — system monitor on an SK6812 flex strip.
// Reads "S <mem> <gpu> <cpu> <disk>\n" lines from USB serial (tools/sysmon.sh) and
// shows the SELECTED metric as a green->amber->red progress bar on the strip + the
// value on-screen. Button A cycles the metric; the CoreS3 can also set it via ESP-NOW.
#include <M5Unified.h>
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <espkvm_proto.h>
#include <espkvm_macros.h>
#include "common/version.h"

using namespace espkvm;

#ifndef RAM_LEDS
#define RAM_LEDS 30
#endif
#ifndef RAM_DATA_PIN
#define RAM_DATA_PIN 32
#endif
static constexpr uint8_t ESPNOW_CHAN = 1;

static Adafruit_NeoPixel strip(RAM_LEDS, RAM_DATA_PIN, NEO_GRB + NEO_KHZ800);
static M5Canvas canvas(&M5.Display);
static int      g_val[MET_COUNT] = { -1, -1, -1, -1 };
static uint8_t  g_metric = MET_MEM;
static uint32_t g_lastData = 0, g_rxBytes = 0;
static char     g_lastLine[28] = "";
static volatile uint8_t g_setMetric = 0xFF;   // from ESP-NOW (applied in loop)

void onEspNow(const esp_now_recv_info_t*, const uint8_t* data, int len) {
  if (len != (int)sizeof(Packet)) return;
  Packet p; memcpy(&p, data, sizeof(p));
  if (valid(p) && (Cmd)p.type == Cmd::SET_METRIC && p.len >= 1 && p.payload[0] < MET_COUNT)
    g_setMetric = p.payload[0];
}

static void colorFor(int u, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (u < 0) u = 0; if (u > 100) u = 100;
  if (u < 50) { r = (uint8_t)map(u, 0, 50, 0, 255); g = 255; }
  else        { r = 255; g = (uint8_t)map(u, 50, 100, 255, 0); }
  b = 0;
}
static void showStrip() {
  int v = g_val[g_metric];
  bool stale = (millis() - g_lastData > 6000) || v < 0;
  int used = v < 0 ? 0 : v;
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
  int v = g_val[g_metric];
  d.fillScreen(TFT_BLACK);
  d.setTextColor(0x7FFF, TFT_BLACK); d.setTextSize(3);
  d.setCursor(6, 6); d.print(METRIC_NAMES[g_metric]);
  d.setTextColor(TFT_WHITE, TFT_BLACK); d.setTextSize(6);
  d.setCursor(8, 36);
  if (v < 0) d.print("--"); else d.printf("%d", v);
  d.setTextSize(2); d.print("%");
  int W = canvas.width(), x = 8, y = 94, w = W - 16, h = 16;
  d.drawRect(x, y, w, h, TFT_WHITE);
  int uu = v < 0 ? 0 : v;
  uint16_t c = uu < 50 ? 0x2FE6 : (uu < 80 ? 0xFD20 : 0xF800);
  d.fillRect(x + 1, y + 1, (w - 2) * uu / 100, h - 2, c);
  d.setTextSize(1); d.setTextColor(0x8C71, TFT_BLACK);
  d.setCursor(6, canvas.height() - 12);
  d.printf("BtnA=cycle  rx:%lu", (unsigned long)g_rxBytes);
  canvas.pushSprite(0, 0);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);
  Serial.begin(115200);
  canvas.createSprite(M5.Display.width(), M5.Display.height());

  strip.begin(); strip.setBrightness(40); strip.clear(); strip.show();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHAN, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onEspNow);
    esp_now_peer_info_t peer = {};
    memset(peer.peer_addr, 0xFF, 6); peer.channel = ESPNOW_CHAN; peer.encrypt = false;
    esp_now_add_peer(&peer);
  }
  showStrip(); drawScreen();
}

void loop() {
  M5.update();
  if (M5.BtnA.wasPressed()) g_metric = (g_metric + 1) % MET_COUNT;
  if (g_setMetric != 0xFF) { g_metric = g_setMetric; g_setMetric = 0xFF; }

  static String line;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    g_rxBytes++;
    if (ch == '\n' || ch == '\r') {
      if (line.length()) {
        strncpy(g_lastLine, line.c_str(), sizeof(g_lastLine) - 1);
        g_lastLine[sizeof(g_lastLine) - 1] = 0;
        int m, g, c, dsk;
        if (sscanf(line.c_str(), "S %d %d %d %d", &m, &g, &c, &dsk) == 4) {
          g_val[MET_MEM] = m; g_val[MET_GPU] = g; g_val[MET_CPU] = c; g_val[MET_DISK] = dsk;
          g_lastData = millis();
        }
      }
      line = "";
    } else if (line.length() < 26) line += ch;
  }

  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 200) { lastDraw = millis(); showStrip(); drawScreen(); }
  delay(5);
}
