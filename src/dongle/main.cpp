// ESP-KVM dongle — Phase 2: ESP-NOW receiver -> USB HID.
// Receives command packets (see lib/espkvm_proto) over ESP-NOW and injects them
// into the host as a USB keyboard/mouse. Pairing is added later; for now it
// listens on the broadcast address, channel 1.
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <USBHIDMouse.h>
#include <espkvm_proto.h>
#include "common/version.h"

using namespace espkvm;

static constexpr uint8_t ESPNOW_CHAN = 1;

USBHIDKeyboard Keyboard;
USBHIDMouse    Mouse;

// Single-producer (recv cb) / single-consumer (loop) ring buffer so HID writes
// happen in loop(), never inside the Wi-Fi callback.
static constexpr uint8_t QN = 8;
static volatile uint8_t  qHead = 0, qTail = 0;
static Packet            qbuf[QN];
static volatile uint32_t g_rx = 0;

static uint8_t ardKeyForMod(uint8_t bit) {
  switch (bit) {
    case MOD_LCTRL:  return KEY_LEFT_CTRL;
    case MOD_LSHIFT: return KEY_LEFT_SHIFT;
    case MOD_LALT:   return KEY_LEFT_ALT;
    case MOD_LGUI:   return KEY_LEFT_GUI;
    case MOD_RCTRL:  return KEY_RIGHT_CTRL;
    case MOD_RSHIFT: return KEY_RIGHT_SHIFT;
    case MOD_RALT:   return KEY_RIGHT_ALT;
    case MOD_RGUI:   return KEY_RIGHT_GUI;
  }
  return 0;
}

void onRecv(const esp_now_recv_info_t*, const uint8_t* data, int len) {
  if (len != (int)sizeof(Packet)) return;
  uint8_t next = (uint8_t)((qHead + 1) % QN);
  if (next == qTail) return;                 // full: drop
  memcpy(&qbuf[qHead], data, sizeof(Packet));
  qHead = next;
  g_rx++;
}

static void execPacket(const Packet& p) {
  if (!valid(p)) return;
  switch ((Cmd)p.type) {
    case Cmd::TYPE_STRING:
      for (uint8_t i = 0; i < p.len; i++) Keyboard.write(p.payload[i]);
      break;
    case Cmd::KEY_COMBO: {
      if (p.len < 2) break;
      const KeyCombo* c = reinterpret_cast<const KeyCombo*>(p.payload);
      for (uint8_t b = 0; b < 8; b++) {
        uint8_t m = (uint8_t)(1 << b);
        if (c->modifiers & m) { uint8_t k = ardKeyForMod(m); if (k) Keyboard.press(k); }
      }
      uint8_t n = c->count > MAX_KEYS ? MAX_KEYS : c->count;
      for (uint8_t i = 0; i < n; i++) Keyboard.press(c->keys[i]);
      delay(8);
      Keyboard.releaseAll();
      break;
    }
    case Cmd::MOUSE_MOVE: { auto* m = reinterpret_cast<const MouseMove*>(p.payload); Mouse.move(m->dx, m->dy); break; }
    case Cmd::SCROLL:     { auto* s = reinterpret_cast<const Scroll*>(p.payload);    Mouse.move(0, 0, s->v); break; }
    case Cmd::MOUSE_BTN:  {
      auto* mb = reinterpret_cast<const MouseBtn*>(p.payload);
      uint8_t btn = (mb->buttons & MB_LEFT   ? MOUSE_LEFT   : 0)
                  | (mb->buttons & MB_RIGHT  ? MOUSE_RIGHT  : 0)
                  | (mb->buttons & MB_MIDDLE ? MOUSE_MIDDLE : 0);
      if (mb->pressed) Mouse.press(btn); else Mouse.release(btn);
      break;
    }
    default: break;   // PING and others: ignored for now
  }
}

void setup() {
  Keyboard.begin();
  Mouse.begin();
  USB.begin();

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHAN, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onRecv);
    esp_now_peer_info_t peer = {};
    memset(peer.peer_addr, 0xFF, 6);         // broadcast
    peer.channel = ESPNOW_CHAN;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  }
  delay(1500);   // let the host enumerate the HID device
}

void loop() {
  while (qTail != qHead) {
    Packet p;
    memcpy(&p, &qbuf[qTail], sizeof(Packet));
    qTail = (uint8_t)((qTail + 1) % QN);
    execPacket(p);
  }
  delay(2);
}
