// ESP-KVM wire protocol — shared by both the controller and the dongle.
// Header-only so both PlatformIO environments can include it directly.
#pragma once
#include <stdint.h>
#include <string.h>

namespace espkvm {

static constexpr uint8_t PROTO_MAGIC   = 0xE5;  // "E5" ~ ESP-KVM
static constexpr uint8_t PROTO_VERSION = 1;
static constexpr uint8_t MAX_PAYLOAD   = 32;
static constexpr uint8_t MAX_KEYS      = 6;     // USB HID 6-key rollover

// Command types. Controller -> dongle, except STATUS (dongle -> controller).
enum class Cmd : uint8_t {
  PING        = 0,
  KEY_COMBO   = 1,   // modifiers + up to 6 HID keycodes, pressed then released
  TYPE_STRING = 2,   // ASCII string typed as-is
  MOUSE_MOVE  = 3,   // int8 dx, dy
  MOUSE_BTN   = 4,   // button mask + pressed flag
  SCROLL      = 5,   // int8 vertical, horizontal
  MEDIA       = 6,   // consumer-control usage (play/pause, volume, ...)
  ANNOUNCE    = 100, // controller -> M5Paper: profile/button context
  STATUS      = 200, // dongle -> controller heartbeat
};

// Keyboard modifier bitmask (matches the USB HID modifier byte).
enum Modifier : uint8_t {
  MOD_NONE   = 0,
  MOD_LCTRL  = 1 << 0,
  MOD_LSHIFT = 1 << 1,
  MOD_LALT   = 1 << 2,
  MOD_LGUI   = 1 << 3,  // Cmd / Win
  MOD_RCTRL  = 1 << 4,
  MOD_RSHIFT = 1 << 5,
  MOD_RALT   = 1 << 6,
  MOD_RGUI   = 1 << 7,
};

// USB HID special key codes, matching arduino-esp32 USBHIDKeyboard constants, so
// the controller can name non-ASCII keys and the dongle presses them directly.
namespace key {
  static constexpr uint8_t RET   = 0xB0, ESC = 0xB1, BACKSPACE = 0xB2, TAB = 0xB3;
  static constexpr uint8_t RIGHT = 0xD7, LEFT = 0xD8, DOWN = 0xD9, UP = 0xDA;
  static constexpr uint8_t F1=0xC2,F2=0xC3,F3=0xC4,F4=0xC5,F5=0xC6,F6=0xC7;
  static constexpr uint8_t F7=0xC8,F8=0xC9,F9=0xCA,F10=0xCB,F11=0xCC,F12=0xCD;
}

enum MouseButton : uint8_t {
  MB_LEFT   = 1 << 0,
  MB_RIGHT  = 1 << 1,
  MB_MIDDLE = 1 << 2,
};

#pragma pack(push, 1)
struct Packet {
  uint8_t  magic;                 // PROTO_MAGIC
  uint8_t  version;               // PROTO_VERSION
  uint16_t seq;                   // rolling sequence counter
  uint8_t  type;                  // Cmd
  uint8_t  len;                   // valid bytes in payload
  uint8_t  payload[MAX_PAYLOAD];
};

struct KeyCombo {
  uint8_t modifiers;              // Modifier bitmask
  uint8_t count;                  // number of keycodes in use
  uint8_t keys[MAX_KEYS];         // USB HID usage ids
};

struct MouseMove { int8_t dx; int8_t dy; };
struct Scroll    { int8_t v;  int8_t h;  };
struct MouseBtn  { uint8_t buttons; uint8_t pressed; };
struct Status    { uint8_t linked; uint8_t hid_enabled; int8_t rssi; };
struct Announce  { uint8_t profile; uint8_t button; }; // button 0xFF = profile switch only
#pragma pack(pop)

// Build a packet from an arbitrary payload struct.
static inline Packet make_packet(Cmd type, const void* data, uint8_t len, uint16_t seq) {
  Packet p{};
  p.magic   = PROTO_MAGIC;
  p.version = PROTO_VERSION;
  p.seq     = seq;
  p.type    = static_cast<uint8_t>(type);
  p.len     = (len > MAX_PAYLOAD) ? MAX_PAYLOAD : len;
  if (data && p.len) memcpy(p.payload, data, p.len);
  return p;
}

static inline bool valid(const Packet& p) {
  return p.magic == PROTO_MAGIC && p.version == PROTO_VERSION;
}

} // namespace espkvm
