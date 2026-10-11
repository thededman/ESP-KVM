// ESP-KVM dongle — Phase 1 coexistence spike.
//
// Goal: prove the T-Dongle S3 can run a USB-HID keyboard AND ESP-NOW at the same
// time. The ESP32-S3's USB-device peripheral and 2.4GHz radio are independent
// blocks, so this is expected to work — this sketch verifies it on real hardware.
//
// How to test:
//   1. Flash via BOOT-button download mode.
//   2. Plug the dongle into a PC and open a text editor.
//   3. Press the dongle's BOOT button: it should type a line that includes the
//      live ESP-NOW tx/rx counters. If the radio had crashed USB, nothing types.
//
// Because USB is claimed for HID (CDC disabled), there is no serial console here;
// the typed counters ARE the telemetry.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <espkvm_proto.h>
#include "common/version.h"

static constexpr int     PIN_BUTTON   = 0;  // BOOT button, active low
static constexpr uint8_t ESPNOW_CHAN  = 1;  // controller must match this channel

USBHIDKeyboard Keyboard;

static uint32_t g_rx = 0;  // ESP-NOW packets received
static uint32_t g_tx = 0;  // ESP-NOW packets sent (callback count)
static uint16_t          g_seq = 0;
static uint32_t          g_lastBeacon = 0;

// arduino-esp32 3.x receive-callback signature (first arg is now a struct ptr).
void onRecv(const esp_now_recv_info_t* /*info*/, const uint8_t* /*data*/, int /*len*/) {
  g_rx++;
}
// arduino-esp32 3.3.x: send-cb's first arg is now const wifi_tx_info_t* (was const uint8_t* mac).
void onSent(const wifi_tx_info_t* /*info*/, esp_now_send_status_t /*status*/) {
  g_tx++;
}

void setup() {
  pinMode(PIN_BUTTON, INPUT_PULLUP);

  // 1) USB-HID first so the host starts enumerating the keyboard.
  Keyboard.begin();
  USB.begin();

  // 2) Bring up the radio. ESP-NOW needs Wi-Fi initialised in station mode; we
  //    don't join an AP, we just need the PHY on a fixed channel.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_channel(ESPNOW_CHAN, WIFI_SECOND_CHAN_NONE);

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onRecv);
    esp_now_register_send_cb(onSent);

    // Broadcast peer so the radio actually transmits during the spike.
    esp_now_peer_info_t peer = {};
    memset(peer.peer_addr, 0xFF, 6);
    peer.channel = ESPNOW_CHAN;
    peer.encrypt = false;
    esp_now_add_peer(&peer);
  }

  delay(1500);  // give the host a moment to enumerate before we can type
}

void loop() {
  const uint32_t now = millis();

  // Exercise the radio once a second while USB-HID stays live.
  if (now - g_lastBeacon > 1000) {
    g_lastBeacon = now;
    espkvm::Packet p = espkvm::make_packet(espkvm::Cmd::PING, nullptr, 0, g_seq++);
    uint8_t bcast[6];
    memset(bcast, 0xFF, 6);
    esp_now_send(bcast, reinterpret_cast<uint8_t*>(&p), sizeof(p));
  }

  // Falling-edge detect on BOOT -> type the proof line.
  static bool released = true;
  const bool pressed = (digitalRead(PIN_BUTTON) == LOW);
  if (pressed && released) {
    Keyboard.print("ESP-KVM " ESPKVM_VERSION " spike OK: USB-HID + ESP-NOW coexist. tx=");
    Keyboard.print(g_tx);
    Keyboard.print(" rx=");
    Keyboard.print(g_rx);
    Keyboard.write(KEY_RETURN);
    delay(250);  // crude debounce
  }
  released = !pressed;
}
