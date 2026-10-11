// ESP-KVM M5Paper — ambient e-ink legend + action log.
// Listens for ANNOUNCE packets from the CoreS3 controller and renders the current
// profile's button map plus a log of recently fired macros on the e-ink display.
#include <M5Unified.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <espkvm_proto.h>
#include <espkvm_macros.h>
#include "common/version.h"

using namespace espkvm;

static constexpr uint8_t ESPNOW_CHAN = 1;
static constexpr int C_W = 0xFFFF;   // white
static constexpr int C_B = 0x0000;   // black
static constexpr int C_G = 0x8410;   // mid grey

// ---- state ----
static volatile bool g_have = false;
static Packet        g_pkt;
static uint8_t       g_profile = 0;
static uint8_t       g_lastBtn = 0xFF;
static uint32_t      g_renderCount = 0;

struct LogEntry { uint8_t profile; uint8_t button; uint32_t ts; };
static constexpr int LOGN = 8;
static LogEntry g_log[LOGN];
static int      g_logHead = 0, g_logCount = 0;

void onRecv(const esp_now_recv_info_t*, const uint8_t* data, int len) {
  if (len == (int)sizeof(Packet)) { memcpy(&g_pkt, data, sizeof(Packet)); g_have = true; }
}

static void drawCentered(int x, int y, int w, int h, const char* s) {
  int tw = M5.Display.textWidth(s), th = M5.Display.fontHeight();
  M5.Display.setCursor(x + (w - tw) / 2, y + (h - th) / 2);
  M5.Display.print(s);
}

static void render(bool full) {
  auto& d = M5.Display;
  d.setEpdMode(full ? m5gfx::epd_mode_t::epd_quality : m5gfx::epd_mode_t::epd_fast);
  const int W = d.width(), H = d.height();

  d.startWrite();
  d.fillScreen(C_W);

  // header
  d.fillRect(0, 0, W, 96, C_B);
  d.setTextColor(C_W, C_B);
  d.setTextSize(4);
  d.setCursor(20, 32);
  d.print("ESP-KVM");
  d.setTextSize(5);
  { const char* n = PROFILES[g_profile].name;
    int tw = d.textWidth(n); d.setCursor(W - 20 - tw, 26); d.print(n); }

  // legend grid (2x3)
  d.setTextColor(C_B, C_W);
  const int pad = 18, cols = 2, rows = 3, gy = 120, gh = 372;
  const int bw = (W - pad * (cols + 1)) / cols;
  const int bh = (gh - pad * (rows + 1)) / rows;
  d.setTextSize(4);
  for (int i = 0; i < BTN_COUNT; i++) {
    int c = i % cols, r = i / cols;
    int x = pad + c * (bw + pad), y = gy + r * (bh + pad);
    bool last = (i == g_lastBtn);
    if (last) { d.fillRoundRect(x, y, bw, bh, 14, C_B); d.setTextColor(C_W, C_B); }
    else      { d.fillRoundRect(x, y, bw, bh, 14, C_W); d.setTextColor(C_B, C_W); }
    d.drawRoundRect(x, y, bw, bh, 14, C_B);
    d.drawRoundRect(x + 1, y + 1, bw - 2, bh - 2, 13, C_B);
    drawCentered(x, y, bw, bh, PROFILES[g_profile].macros[i].label);
  }

  // log
  int ly = gy + gh + 24;
  d.setTextColor(C_B, C_W);
  d.setTextSize(3);
  d.setCursor(20, ly); d.print("Recent actions");
  ly += 42;
  d.drawFastHLine(20, ly, W - 40, C_B);
  ly += 14;
  for (int k = 0; k < g_logCount; k++) {
    int idx = (g_logHead - 1 - k + LOGN) % LOGN;
    const LogEntry& e = g_log[idx];
    char line[72];
    snprintf(line, sizeof(line), "%02lu:%02lu   %s  >  %s",
             (unsigned long)(e.ts / 60), (unsigned long)(e.ts % 60),
             PROFILES[e.profile].name, PROFILES[e.profile].macros[e.button].label);
    d.setCursor(24, ly); d.print(line);
    ly += 40;
    if (ly > H - 40) break;
  }

  // footer
  d.setTextSize(2);
  d.setTextColor(C_G, C_W);
  d.setCursor(20, H - 28);
  d.print("ESP-KVM legend  " ESPKVM_VERSION);

  d.endWrite();
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(0);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHAN, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onRecv);
    esp_now_peer_info_t peer = {};
    memset(peer.peer_addr, 0xFF, 6);
    peer.channel = ESPNOW_CHAN;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  }
  render(true);   // initial full refresh
}

void loop() {
  if (g_have) {
    g_have = false;
    if (valid(g_pkt) && (Cmd)g_pkt.type == Cmd::ANNOUNCE && g_pkt.len >= sizeof(Announce)) {
      const Announce* a = reinterpret_cast<const Announce*>(g_pkt.payload);
      if (a->profile < PROFILE_COUNT) g_profile = a->profile;
      if (a->button != 0xFF && a->button < BTN_COUNT) {
        g_lastBtn = a->button;
        g_log[g_logHead] = { g_profile, a->button, millis() / 1000 };
        g_logHead = (g_logHead + 1) % LOGN;
        if (g_logCount < LOGN) g_logCount++;
      } else {
        g_lastBtn = 0xFF;   // profile switch
      }
      bool full = (g_renderCount++ % 8 == 0);   // periodic clean refresh
      render(full);
    }
  }
  delay(10);
}
