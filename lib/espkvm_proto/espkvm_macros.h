// ESP-KVM macro/profile definitions — shared by the CoreS3 controller (which
// sends the commands) and the M5Paper (which renders the legend + log).
#pragma once
#include <stdint.h>
#include "espkvm_proto.h"

namespace espkvm {

enum Kind : uint8_t { K_STR, K_COMBO };

struct Macro {
  const char* label;
  uint32_t    rgb;     // button color 0xRRGGBB
  Kind        kind;
  const char* str;     // K_STR payload
  uint8_t     mods;    // K_COMBO modifiers
  uint8_t     key;     // K_COMBO key (ASCII or espkvm::key::*)
};

static const Macro DEV[6] = {
  {"Copy",   0x2E7D32, K_COMBO, nullptr, MOD_LGUI, 'c'},
  {"Paste",  0x1565C0, K_COMBO, nullptr, MOD_LGUI, 'v'},
  {"Cut",    0xAD1457, K_COMBO, nullptr, MOD_LGUI, 'x'},
  {"Undo",   0x6A1B9A, K_COMBO, nullptr, MOD_LGUI, 'z'},
  {"Save",   0xEF6C00, K_COMBO, nullptr, MOD_LGUI, 's'},
  {"Find",   0x00838F, K_COMBO, nullptr, MOD_LGUI, 'f'},
};
static const Macro SYS[6] = {
  {"Spotlight", 0x1565C0, K_COMBO, nullptr, MOD_LGUI, ' '},
  {"Mission",   0x00838F, K_COMBO, nullptr, MOD_LCTRL, key::UP},
  {"App Switch",0x6A1B9A, K_COMBO, nullptr, MOD_LGUI, key::TAB},
  {"Screenshot",0xEF6C00, K_COMBO, nullptr, (uint8_t)(MOD_LGUI|MOD_LSHIFT), '4'},
  {"Lock",      0xC62828, K_COMBO, nullptr, (uint8_t)(MOD_LCTRL|MOD_LGUI), 'q'},
  {"Desktop",   0x2E7D32, K_COMBO, nullptr, 0, key::F11},
};
static const Macro SNIP[6] = {
  {"Hello",  0x2E7D32, K_STR, "Hello from ESP-KVM!\n", 0, 0},
  {"Email",  0x1565C0, K_STR, "greystonehacker@gmail.com", 0, 0},
  {"Thanks", 0x00838F, K_STR, "Thanks!\n", 0, 0},
  {"Sig",    0x6A1B9A, K_STR, "\n-- David\n", 0, 0},
  {"Code",   0xEF6C00, K_STR, "console.log();", 0, 0},
  {"Arrow",  0xAD1457, K_STR, " => ", 0, 0},
};

struct Profile { const char* name; const Macro* macros; };
static const Profile PROFILES[3] = { {"Dev", DEV}, {"System", SYS}, {"Snippets", SNIP} };
static constexpr uint8_t PROFILE_COUNT = 3;
static constexpr uint8_t BTN_COUNT     = 6;

// System-monitor metrics shared by the StickC meter and the CoreS3 Monitor page.
enum Metric : uint8_t { MET_MEM = 0, MET_GPU = 1, MET_CPU = 2, MET_DISK = 3, MET_COUNT = 4 };
static const char* const METRIC_NAMES[MET_COUNT] = { "MEM", "GPU", "CPU", "DISK" };

// Media (USB consumer-control) actions handled by the dongle.
enum Media : uint8_t { MED_VOLUP = 0, MED_VOLDOWN = 1, MED_MUTE = 2,
                       MED_PLAYPAUSE = 3, MED_NEXT = 4, MED_PREV = 5 };
// TV IR actions handled by the StickC IR blaster.
enum IrCmd : uint8_t { IR_TV_POWER = 0, IR_TV_VOLUP = 1, IR_TV_VOLDOWN = 2,
                       IR_TV_MUTE = 3, IR_TV_INPUT = 4 };

} // namespace espkvm
