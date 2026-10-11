// ESP-KVM controller — Phase 2: touch macro deck -> ESP-NOW sender.
// Draws a 2x3 grid of macro buttons; a tap sends the matching command to the
// dongle over ESP-NOW (broadcast, channel 1). LVGL + profiles + pairing come in
// later phases; this proves the end-to-end control path.
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <espkvm_proto.h>
#include "common/version.h"

using namespace espkvm;

static constexpr uint8_t ESPNOW_CHAN = 1;
static uint8_t  BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static uint16_t g_seq   = 0;
static uint32_t g_sends = 0;
static const char* g_last = "ready";

// ---- colors (RGB565) ----
static constexpr uint16_t C_BG    = 0x0000;
static constexpr uint16_t C_WHITE = 0xFFFF;
static constexpr uint16_t C_HDR   = 0x10A2;

enum Action { A_HELLO, A_SPOTLIGHT, A_COPY, A_PASTE, A_LOCK, A_SHOT, A_COUNT };
struct BtnDef { const char* label; uint16_t color; };
static BtnDef BTN[A_COUNT] = {
  { "Hello",      0x05A8 },  // green
  { "Spotlight",  0x1E9F },  // blue
  { "Copy",       0xFD20 },  // orange
  { "Paste",      0xC618 },  // grey
  { "Lock",       0xF9A6 },  // red
  { "Screenshot", 0x8A1F },  // purple
};

// grid geometry
static constexpr int COLS = 2, ROWS = 3;
static constexpr int HDR_H = 36, PAD = 8;

static void btnRect(int i, int& x, int& y, int& w, int& h) {
  int col = i % COLS, row = i / COLS;
  int cw = M5.Display.width() / COLS;
  int ch = (M5.Display.height() - HDR_H) / ROWS;
  x = col * cw + PAD;
  y = HDR_H + row * ch + PAD;
  w = cw - 2 * PAD;
  h = ch - 2 * PAD;
}

static void drawHeader() {
  M5.Display.fillRect(0, 0, M5.Display.width(), HDR_H, C_HDR);
  M5.Display.setTextColor(C_WHITE, C_HDR);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(6, 6);
  M5.Display.printf("ESP-KVM  %s", ESPKVM_VERSION);
  M5.Display.setCursor(6, 20);
  M5.Display.printf("sent:%lu  last:%s     ", (unsigned long)g_sends, g_last);
}

static void drawButton(int i, bool pressed) {
  int x, y, w, h; btnRect(i, x, y, w, h);
  uint16_t fill = pressed ? C_WHITE : BTN[i].color;
  uint16_t txt  = pressed ? BTN[i].color : C_WHITE;
  M5.Display.fillRoundRect(x, y, w, h, 10, fill);
  M5.Display.drawRoundRect(x, y, w, h, 10, C_WHITE);
  M5.Display.setTextColor(txt, fill);
  M5.Display.setTextSize(2);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(BTN[i].label, x + w / 2, y + h / 2);
  M5.Display.setTextDatum(top_left);
}

static void drawAll() {
  M5.Display.fillScreen(C_BG);
  drawHeader();
  for (int i = 0; i < A_COUNT; i++) drawButton(i, false);
}

static void sendPacket(const Packet& p) {
  esp_err_t r = esp_now_send(BCAST, reinterpret_cast<const uint8_t*>(&p), sizeof(p));
  if (r == ESP_OK) g_sends++;
}

static void fireAction(int i) {
  switch (i) {
    case A_HELLO: {
      const char* s = "Hello from ESP-KVM!\n";
      Packet p = make_packet(Cmd::TYPE_STRING, s, (uint8_t)strlen(s), g_seq++);
      sendPacket(p); g_last = "Hello"; break;
    }
    case A_SPOTLIGHT: { KeyCombo c{MOD_LGUI, 1, {' '}};         Packet p = make_packet(Cmd::KEY_COMBO, &c, sizeof(c), g_seq++); sendPacket(p); g_last = "Spotlight"; break; }
    case A_COPY:      { KeyCombo c{MOD_LGUI, 1, {'c'}};         Packet p = make_packet(Cmd::KEY_COMBO, &c, sizeof(c), g_seq++); sendPacket(p); g_last = "Copy"; break; }
    case A_PASTE:     { KeyCombo c{MOD_LGUI, 1, {'v'}};         Packet p = make_packet(Cmd::KEY_COMBO, &c, sizeof(c), g_seq++); sendPacket(p); g_last = "Paste"; break; }
    case A_LOCK:      { KeyCombo c{(uint8_t)(MOD_LCTRL|MOD_LGUI), 1, {'q'}};  Packet p = make_packet(Cmd::KEY_COMBO, &c, sizeof(c), g_seq++); sendPacket(p); g_last = "Lock"; break; }
    case A_SHOT:      { KeyCombo c{(uint8_t)(MOD_LGUI|MOD_LSHIFT), 1, {'4'}}; Packet p = make_packet(Cmd::KEY_COMBO, &c, sizeof(c), g_seq++); sendPacket(p); g_last = "Screenshot"; break; }
  }
}

static int hitTest(int px, int py) {
  for (int i = 0; i < A_COUNT; i++) {
    int x, y, w, h; btnRect(i, x, y, w, h);
    if (px >= x && px < x + w && py >= y && py < y + h) return i;
  }
  return -1;
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(2);  // portrait, USB-C at bottom

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHAN, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() == ESP_OK) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BCAST, 6);
    peer.channel = ESPNOW_CHAN;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  }
  drawAll();
}

void loop() {
  M5.update();
  auto t = M5.Touch.getDetail();
  if (t.wasPressed()) {
    int i = hitTest(t.x, t.y);
    if (i >= 0) {
      drawButton(i, true);
      fireAction(i);
      drawHeader();
      delay(90);
      drawButton(i, false);
    }
  }
  delay(5);
}
