// ESP-KVM Core2 "Guard" — bridge to the M5Stack UnitV2 AI camera.
// Reads the UnitV2's JSON face detections over PORT.A UART (115200 8N1) and does
// presence-aware auto-lock: if no face is seen for a timeout while ARMED, it fires
// the System > Lock macro over ESP-NOW. On-screen ARM/DISARM toggle (default off)
// keeps testing safe.
#include <M5Unified.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <espkvm_proto.h>
#include <espkvm_macros.h>
#include "common/version.h"

using namespace espkvm;

// --- UnitV2 link (PORT.A on Core2: RX=33, TX=32; swap if no data) ---
static constexpr int UV2_RX = 33, UV2_TX = 32;
static constexpr uint32_t AWAY_TIMEOUT_MS = 15000;   // away this long -> lock

// --- ESP-NOW ---
static uint8_t  BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static constexpr uint8_t ESPNOW_CHAN = 1;
static uint16_t g_seq = 0;

static void sendPacket(const Packet& p) { esp_now_send(BCAST, (const uint8_t*)&p, sizeof(p)); }
static void sendLock() {
  const Macro& m = SYS[4];                    // System > Lock (Ctrl+Cmd+Q)
  KeyCombo c{ m.mods, 1, { m.key } };
  sendPacket(make_packet(Cmd::KEY_COMBO, &c, sizeof(c), g_seq++));
  Announce a{1, 4};                           // tell M5Paper/dongle: System > Lock
  sendPacket(make_packet(Cmd::ANNOUNCE, &a, sizeof(a), g_seq++));
}

// --- state ---
static int      g_faces = 0;
static uint32_t g_lastSeen = 0;
static uint32_t g_lastData = 0;
static bool     g_armed = false;
static bool     g_locked = false;
static uint32_t g_lockCount = 0;
static char     g_raw[256] = "(waiting for UnitV2...)";
static int      g_dbgNum = -999, g_dbgArr = -1;
static M5Canvas canvas(&M5.Display);

static void sendFunction(const char* fn) {
  JsonDocument d; d["function"] = fn; d["args"] = "";
  String out; serializeJson(d, out);
  Serial2.println(out);
  Serial2.flush();
}

// --- UI ---
static int BTN_X, BTN_Y, BTN_W, BTN_H;
static void draw() {
  auto& d = canvas;
  const int W = d.width(), H = d.height();
  d.fillScreen(0x0000);

  d.fillRect(0, 0, W, 30, 0x10A2);
  d.setTextColor(0x7FFF, 0x10A2); d.setTextSize(2); d.setCursor(6, 7);
  d.print("ESP-KVM Guard");

  bool present = g_faces > 0;
  uint16_t col = present ? 0x2FE6 : 0xFD20;
  d.setTextColor(col, 0x0000); d.setTextSize(3); d.setCursor(10, 48);
  d.printf("%s", present ? "PRESENT" : "AWAY");
  d.setTextColor(0xFFFF, 0x0000); d.setTextSize(2); d.setCursor(10, 86);
  d.printf("faces: %d", g_faces);

  d.setTextSize(2); d.setCursor(10, 112);
  if (g_locked) { d.setTextColor(0xF800, 0x0000); d.print("LOCKED (sent)"); }
  else if (!present && g_armed) {
    uint32_t el = millis() - g_lastSeen;
    int secs = el < AWAY_TIMEOUT_MS ? (AWAY_TIMEOUT_MS - el) / 1000 : 0;
    d.setTextColor(0xFD20, 0x0000); d.printf("Lock in %ds ", secs);
  } else { d.setTextColor(0x8410, 0x0000); d.print(g_armed ? "armed" : "disarmed"); }

  BTN_X = 10; BTN_Y = 150; BTN_W = W - 20; BTN_H = 50;
  uint16_t bc = g_armed ? 0xC000 : 0x4208;
  d.fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, 10, bc);
  d.setTextColor(0xFFFF, bc); d.setTextSize(2);
  const char* bl = g_armed ? "ARMED - tap to disarm" : "DISARMED - tap to ARM";
  int tw = d.textWidth(bl);
  d.setCursor(BTN_X + (BTN_W - tw) / 2, BTN_Y + 16);
  d.print(bl);

  d.setTextColor(0x8C71, 0x0000); d.setTextSize(1);
  d.setCursor(10, H - 12);
  d.printf("locks:%lu  faces live  %s", (unsigned long)g_lockCount, ESPKVM_VERSION);
  canvas.pushSprite(0, 0);
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(1);
  canvas.createSprite(M5.Display.width(), M5.Display.height());

  Serial2.begin(115200, SERIAL_8N1, UV2_RX, UV2_TX);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHAN, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() == ESP_OK) {
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, BCAST, 6);
    peer.channel = ESPNOW_CHAN; peer.encrypt = false;
    esp_now_add_peer(&peer);
  }

  g_lastSeen = millis();
  delay(300);
  sendFunction("Face Detector");
  draw();
}

void loop() {
  M5.update();

  // read a line from the UnitV2
  static String line;
  while (Serial2.available()) {
    char ch = (char)Serial2.read();
    if (ch == '\n' || ch == '\r') {
      if (line.length() > 1) {
        int b = line.indexOf('{');
        if (b >= 0) {
          String js = line.substring(b);
          JsonDocument doc;
          if (!deserializeJson(doc, js)) {
            int num = doc["num"] | -999;
            int arr = -1;
            if (doc["face"].is<JsonArray>())       arr = doc["face"].size();
            else if (doc["faces"].is<JsonArray>()) arr = doc["faces"].size();
            else if (doc["obj"].is<JsonArray>())   arr = doc["obj"].size();
            g_dbgNum = num; g_dbgArr = arr;
            g_faces = (arr >= 0) ? arr : (num > -999 ? num : 0);
            g_lastData = millis();
            if (g_faces > 0) { g_lastSeen = millis(); g_locked = false; }
            strncpy(g_raw, js.c_str(), sizeof(g_raw) - 1);
            g_raw[sizeof(g_raw) - 1] = 0;
          }
        }
      }
      line = "";
    } else if (line.length() < 300) {
      line += ch;
    }
  }

  // re-assert the function if the UnitV2 has gone quiet
  static uint32_t lastFn = 0;
  if (millis() - g_lastData > 8000 && millis() - lastFn > 8000) {
    lastFn = millis(); sendFunction("Face Detector");
  }

  // auto-lock logic
  if (g_armed && !g_locked && g_faces == 0 &&
      (millis() - g_lastSeen > AWAY_TIMEOUT_MS)) {
    sendLock(); g_locked = true; g_lockCount++;
  }

  // ARM/DISARM touch
  auto t = M5.Touch.getDetail();
  if (t.wasPressed() && t.x >= BTN_X && t.x < BTN_X + BTN_W &&
      t.y >= BTN_Y && t.y < BTN_Y + BTN_H) {
    g_armed = !g_armed;
    if (g_armed) { g_lastSeen = millis(); g_locked = false; }
  }

  static uint32_t lastDraw = 0;
  if (millis() - lastDraw > 300) { lastDraw = millis(); draw(); }
  delay(5);
}
