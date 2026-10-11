// ESP-KVM dongle — Phase 2/4: ESP-NOW receiver -> USB HID, with onboard status LCD.
// Receives commands over ESP-NOW and injects them as USB keyboard/mouse. Shows a
// live status screen on the T-Dongle S3's 0.96" ST7735 (last action from ANNOUNCE,
// packet count, HID on/off). BOOT toggles HID output on/off (safety).
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <espkvm_proto.h>
#include <espkvm_macros.h>
#include "common/version.h"

using namespace espkvm;

// ---- T-Dongle S3 ST7735S (80x160) LovyanGFX config ----
class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7735S _panel;
  lgfx::Bus_SPI       _bus;
  lgfx::Light_PWM     _light;
public:
  LGFX() {
    { auto c = _bus.config();
      c.spi_host = SPI3_HOST; c.spi_mode = 0;
      c.freq_write = 27000000; c.freq_read = 16000000;
      c.spi_3wire = true; c.use_lock = true; c.dma_channel = SPI_DMA_CH_AUTO;
      c.pin_sclk = 5; c.pin_mosi = 3; c.pin_miso = -1; c.pin_dc = 2;
      _bus.config(c); _panel.setBus(&_bus);
    }
    { auto c = _panel.config();
      c.pin_cs = 4; c.pin_rst = 1; c.pin_busy = -1;
      c.panel_width = 80; c.panel_height = 160;
      c.offset_x = 26; c.offset_y = 1; c.offset_rotation = 0;
      c.readable = false; c.invert = true; c.rgb_order = false;
      c.dlen_16bit = false; c.bus_shared = false;
      _panel.config(c);
    }
    { auto c = _light.config(); c.pin_bl = 38; c.invert = false; c.freq = 44100; c.pwm_channel = 7;
      _light.config(c); _panel.setLight(&_light); }
    setPanel(&_panel);
  }
};
static LGFX lcd;

static constexpr uint8_t ESPNOW_CHAN = 1;
static constexpr int PIN_BUTTON = 0;

USBHIDKeyboard Keyboard;
USBHIDMouse    Mouse;

// recv ring (cb -> loop)
static constexpr uint8_t QN = 8;
static volatile uint8_t  qHead = 0, qTail = 0;
static Packet            qbuf[QN];

// display/status state
static volatile uint32_t g_rx = 0;      // commands executed
static uint8_t  g_lastProfile = 0, g_lastBtn = 0xFF;
static bool     g_hidEnabled = true;
static bool     g_dirty = true;

static uint8_t ardKeyForMod(uint8_t bit) {
  switch (bit) {
    case MOD_LCTRL: return KEY_LEFT_CTRL;   case MOD_LSHIFT: return KEY_LEFT_SHIFT;
    case MOD_LALT:  return KEY_LEFT_ALT;    case MOD_LGUI:   return KEY_LEFT_GUI;
    case MOD_RCTRL: return KEY_RIGHT_CTRL;  case MOD_RSHIFT: return KEY_RIGHT_SHIFT;
    case MOD_RALT:  return KEY_RIGHT_ALT;   case MOD_RGUI:   return KEY_RIGHT_GUI;
  }
  return 0;
}

void onRecv(const esp_now_recv_info_t*, const uint8_t* data, int len) {
  if (len != (int)sizeof(Packet)) return;
  uint8_t next = (uint8_t)((qHead + 1) % QN);
  if (next == qTail) return;
  memcpy(&qbuf[qHead], data, sizeof(Packet));
  qHead = next;
}

static void drawStatus() {
  const int W = lcd.width(), H = lcd.height();
  lcd.startWrite();
  lcd.fillScreen(TFT_BLACK);

  // header
  lcd.fillRect(0, 0, W, 18, g_hidEnabled ? lcd.color565(0x12,0x5E,0x9E) : lcd.color565(0x7A,0x1E,0x1E));
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.setCursor(4, 2);
  lcd.print("ESP-KVM");
  lcd.setTextSize(1);
  lcd.setCursor(W - 30, 5);
  lcd.print(g_hidEnabled ? "HID" : "OFF");

  // last action label
  lcd.setTextColor(TFT_WHITE);
  lcd.setTextSize(2);
  lcd.setCursor(4, 26);
  if (g_lastBtn < BTN_COUNT) {
    lcd.print(PROFILES[g_lastProfile].name);
  } else {
    lcd.print("ready");
  }
  lcd.setTextColor(lcd.color565(0x6F,0xD0,0xFF));
  lcd.setCursor(4, 46);
  if (g_lastBtn < BTN_COUNT) lcd.print(PROFILES[g_lastProfile].macros[g_lastBtn].label);

  // footer: packet count
  lcd.setTextColor(lcd.color565(0xA0,0xA0,0xA0));
  lcd.setTextSize(1);
  lcd.setCursor(4, H - 10);
  lcd.printf("RX:%lu  %s", (unsigned long)g_rx, ESPKVM_VERSION);
  lcd.endWrite();
}

static void execPacket(const Packet& p) {
  if (!valid(p)) return;
  if ((Cmd)p.type == Cmd::ANNOUNCE && p.len >= sizeof(Announce)) {
    const Announce* a = reinterpret_cast<const Announce*>(p.payload);
    if (a->profile < PROFILE_COUNT) g_lastProfile = a->profile;
    g_lastBtn = (a->button < BTN_COUNT) ? a->button : 0xFF;
    g_dirty = true;
    return;
  }
  if (!g_hidEnabled) return;   // output gated off
  switch ((Cmd)p.type) {
    case Cmd::TYPE_STRING:
      for (uint8_t i = 0; i < p.len; i++) Keyboard.write(p.payload[i]);
      g_rx++; g_dirty = true; break;
    case Cmd::KEY_COMBO: {
      if (p.len < 2) break;
      const KeyCombo* c = reinterpret_cast<const KeyCombo*>(p.payload);
      for (uint8_t b = 0; b < 8; b++) { uint8_t m = (uint8_t)(1 << b);
        if (c->modifiers & m) { uint8_t k = ardKeyForMod(m); if (k) Keyboard.press(k); } }
      uint8_t n = c->count > MAX_KEYS ? MAX_KEYS : c->count;
      for (uint8_t i = 0; i < n; i++) Keyboard.press(c->keys[i]);
      delay(8); Keyboard.releaseAll(); g_rx++; g_dirty = true; break;
    }
    case Cmd::MOUSE_MOVE: { auto* m = reinterpret_cast<const MouseMove*>(p.payload); Mouse.move(m->dx, m->dy); break; }
    case Cmd::SCROLL:     { auto* s = reinterpret_cast<const Scroll*>(p.payload);    Mouse.move(0, 0, s->v); break; }
    case Cmd::MOUSE_BTN:  { auto* mb = reinterpret_cast<const MouseBtn*>(p.payload);
      uint8_t btn = (mb->buttons & MB_LEFT ? MOUSE_LEFT : 0) | (mb->buttons & MB_RIGHT ? MOUSE_RIGHT : 0) | (mb->buttons & MB_MIDDLE ? MOUSE_MIDDLE : 0);
      if (mb->pressed) Mouse.press(btn); else Mouse.release(btn); break; }
    default: break;
  }
}

void setup() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  Keyboard.begin();
  Mouse.begin();
  USB.begin();

  lcd.init();
  lcd.setRotation(1);
  lcd.setBrightness(160);
  drawStatus();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHAN, WIFI_SECOND_CHAN_NONE);
  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onRecv);
    esp_now_peer_info_t peer = {};
    memset(peer.peer_addr, 0xFF, 6);
    peer.channel = ESPNOW_CHAN; peer.encrypt = false;
    esp_now_add_peer(&peer);
  }
  delay(1500);
}

void loop() {
  while (qTail != qHead) {
    Packet p;
    memcpy(&p, &qbuf[qTail], sizeof(Packet));
    qTail = (uint8_t)((qTail + 1) % QN);
    execPacket(p);
  }

  // BOOT toggles HID output on/off
  static bool released = true;
  bool pressed = (digitalRead(PIN_BUTTON) == LOW);
  if (pressed && released) { g_hidEnabled = !g_hidEnabled; g_dirty = true; delay(50); }
  released = !pressed;

  if (g_dirty) { g_dirty = false; drawStatus(); }
  delay(3);
}
