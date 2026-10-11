// Slate — Nexus M5Paper e-ink touch remote (v0.2.0).
// Big e-ink touch buttons grouped into pages; the side wheel (BtnA up / BtnC down /
// BtnB press) flips pages. A tap sends the matching command over ESP-NOW.
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <espkvm_proto.h>
#include <espkvm_macros.h>
#include "common/version.h"

using namespace espkvm;

static constexpr uint8_t ESPNOW_CHAN = 1;
static constexpr int C_W = 0xFFFF, C_B = 0x0000;
static uint8_t  BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static uint16_t g_seq = 0;
static const char* g_last = "ready";

enum RKind : uint8_t { RK_HEADER, RK_MEDIA, RK_IR, RK_MACRO, RK_METRIC };
struct RItem { const char* label; RKind kind; uint8_t arg; int16_t x, y, w, h; };

static RItem PAGE0[] = {
  {"Media", RK_HEADER, 0},
  {"Vol -", RK_MEDIA, MED_VOLDOWN}, {"Mute", RK_MEDIA, MED_MUTE}, {"Vol +", RK_MEDIA, MED_VOLUP},
  {"Prev", RK_MEDIA, MED_PREV}, {"Play/Pause", RK_MEDIA, MED_PLAYPAUSE}, {"Next", RK_MEDIA, MED_NEXT},
  {"TV  (IR)", RK_HEADER, 0},
  {"Power", RK_IR, IR_TV_POWER}, {"TV Vol -", RK_IR, IR_TV_VOLDOWN}, {"TV Vol +", RK_IR, IR_TV_VOLUP},
  {"TV Mute", RK_IR, IR_TV_MUTE}, {"Input", RK_IR, IR_TV_INPUT},
};
static RItem PAGE1[] = {
  {"Mac", RK_HEADER, 0},
  {"Spotlight", RK_MACRO, 0}, {"Lock", RK_MACRO, 1}, {"Screenshot", RK_MACRO, 2},
  {"Monitor", RK_HEADER, 0},
  {"MEM", RK_METRIC, MET_MEM}, {"GPU", RK_METRIC, MET_GPU}, {"CPU", RK_METRIC, MET_CPU}, {"DISK", RK_METRIC, MET_DISK},
};
struct Page { const char* title; RItem* items; int n; };
static Page PAGES[] = {
  {"Media & TV",    PAGE0, (int)(sizeof(PAGE0)/sizeof(PAGE0[0]))},
  {"Mac & Monitor", PAGE1, (int)(sizeof(PAGE1)/sizeof(PAGE1[0]))},
};
static const int NPAGES = sizeof(PAGES)/sizeof(PAGES[0]);
static int g_page = 0;

static void sendPacket(const Packet& p) { esp_now_send(BCAST, (const uint8_t*)&p, sizeof(p)); }
static void sendByte(Cmd c, uint8_t b) { Packet p = make_packet(c, &b, 1, g_seq++); sendPacket(p); }
static void sendMacro(uint8_t arg) {
  uint8_t mods = 0, key = 0;
  switch (arg) {
    case 0: mods = MOD_LGUI; key = ' '; break;
    case 1: mods = MOD_LCTRL | MOD_LGUI; key = 'q'; break;
    case 2: mods = MOD_LGUI | MOD_LSHIFT; key = '4'; break;
  }
  KeyCombo kc{ mods, 1, { key } };
  Packet p = make_packet(Cmd::KEY_COMBO, &kc, sizeof(kc), g_seq++);
  sendPacket(p);
}

static void layout() {
  Page& pg = PAGES[g_page];
  const int W = M5.Display.width();
  const int pad = 16, cols = 3, bh = 108;
  const int bw = (W - pad * (cols + 1)) / cols;
  int y = 100, col = 0;
  for (int i = 0; i < pg.n; i++) {
    if (pg.items[i].kind == RK_HEADER) {
      if (col != 0) { y += bh + pad; col = 0; }
      pg.items[i].x = pad; pg.items[i].y = y; pg.items[i].w = W - 2 * pad; pg.items[i].h = 42;
      y += 58;
    } else {
      pg.items[i].x = pad + col * (bw + pad); pg.items[i].y = y; pg.items[i].w = bw; pg.items[i].h = bh;
      if (++col >= cols) { col = 0; y += bh + pad; }
    }
  }
}

static void drawCentered(const char* s, int x, int y, int w, int h) {
  int tw = M5.Display.textWidth(s), th = M5.Display.fontHeight();
  M5.Display.setCursor(x + (w - tw) / 2, y + (h - th) / 2);
  M5.Display.print(s);
}
static void drawItem(const RItem& it, bool pressed) {
  auto& d = M5.Display;
  if (it.kind == RK_HEADER) {
    d.setFont(&fonts::FreeSansBold18pt7b); d.setTextColor(C_B, C_W);
    d.setCursor(it.x + 4, it.y + 4); d.print(it.label);
    d.drawFastHLine(it.x, it.y + it.h, it.w, C_B);
  } else {
    uint16_t fill = pressed ? C_B : C_W, txt = pressed ? C_W : C_B;
    d.fillRoundRect(it.x, it.y, it.w, it.h, 12, fill);
    d.drawRoundRect(it.x, it.y, it.w, it.h, 12, C_B);
    d.drawRoundRect(it.x + 1, it.y + 1, it.w - 2, it.h - 2, 11, C_B);
    d.setFont(&fonts::FreeSansBold12pt7b); d.setTextColor(txt, fill);
    drawCentered(it.label, it.x, it.y, it.w, it.h);
  }
}
static void drawFooter() {
  auto& d = M5.Display;
  int y = d.height() - 46;
  d.fillRect(0, y, d.width(), 46, C_W);
  d.drawFastHLine(0, y, d.width(), C_B);
  d.setFont(&fonts::FreeSans9pt7b); d.setTextColor(C_B, C_W);
  d.setCursor(14, y + 14);
  d.printf("last: %s     wheel = page     Slate %s", g_last, ESPKVM_VERSION);
}
static void renderAll() {
  auto& d = M5.Display;
  d.setEpdMode(m5gfx::epd_mode_t::epd_quality);
  d.startWrite();
  d.fillScreen(C_W);
  d.fillRect(0, 0, d.width(), 80, C_B);
  d.setFont(&fonts::FreeSansBold24pt7b); d.setTextColor(C_W, C_B);
  d.setCursor(16, 18); d.print("Slate");
  d.setFont(&fonts::FreeSansBold18pt7b);
  char pg[16]; snprintf(pg, sizeof(pg), "%d/%d", g_page + 1, NPAGES);
  int tw = d.textWidth(pg); d.setCursor(d.width() - tw - 16, 22); d.print(pg);
  Page& p = PAGES[g_page];
  for (int i = 0; i < p.n; i++) drawItem(p.items[i], false);
  drawFooter();
  d.endWrite();
}

static void dispatch(const RItem& it) {
  switch (it.kind) {
    case RK_MEDIA:  sendByte(Cmd::MEDIA, it.arg);      g_last = it.label; break;
    case RK_IR:     sendByte(Cmd::IR, it.arg);         g_last = it.label; break;
    case RK_METRIC: sendByte(Cmd::SET_METRIC, it.arg); g_last = it.label; break;
    case RK_MACRO:  sendMacro(it.arg);                 g_last = it.label; break;
    default: break;
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(0);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHAN, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() == ESP_OK) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BCAST, 6);
    peer.channel = ESPNOW_CHAN; peer.encrypt = false;
    esp_now_add_peer(&peer);
  }
  layout();
  renderAll();
}

void loop() {
  M5.update();

  bool paged = false;
  if (M5.BtnA.wasPressed()) { g_page = (g_page + NPAGES - 1) % NPAGES; paged = true; }
  if (M5.BtnC.wasPressed() || M5.BtnB.wasPressed()) { g_page = (g_page + 1) % NPAGES; paged = true; }
  if (paged) { layout(); renderAll(); }

  auto t = M5.Touch.getDetail();
  if (t.wasPressed()) {
    Page& p = PAGES[g_page];
    for (int i = 0; i < p.n; i++) {
      const RItem& it = p.items[i];
      if (it.kind == RK_HEADER) continue;
      if (t.x >= it.x && t.x < it.x + it.w && t.y >= it.y && t.y < it.y + it.h) {
        M5.Display.setEpdMode(m5gfx::epd_mode_t::epd_fast);
        drawItem(it, true);
        dispatch(it);
        drawFooter();
        delay(140);
        drawItem(it, false);
        break;
      }
    }
  }
  delay(8);
}
