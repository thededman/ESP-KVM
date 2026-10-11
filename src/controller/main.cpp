// ESP-KVM controller — Phase 4: LVGL touch macro deck.
// Swipeable profile pages (Dev / System / Snippets) of styled buttons; a tap
// sends the matching command to the dongle over ESP-NOW (broadcast, channel 1).
// Top status bar shows battery. LVGL draws to the CoreS3 via M5Unified.
#include <M5Unified.h>
#include <lvgl.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <espkvm_proto.h>
#include "common/version.h"

using namespace espkvm;

// --------------------------- ESP-NOW ---------------------------
static uint8_t  BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static constexpr uint8_t ESPNOW_CHAN = 1;
static uint16_t g_seq = 0;

static void sendPacket(const Packet& p) {
  esp_now_send(BCAST, reinterpret_cast<const uint8_t*>(&p), sizeof(p));
}

// --------------------------- macro model ---------------------------
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
  {"Email",  0x1565C0, K_STR, "you@example.com", 0, 0},
  {"Thanks", 0x00838F, K_STR, "Thanks!\n", 0, 0},
  {"Sig",    0x6A1B9A, K_STR, "\n-- David\n", 0, 0},
  {"Code",   0xEF6C00, K_STR, "console.log();", 0, 0},
  {"Arrow",  0xAD1457, K_STR, " => ", 0, 0},
};
struct Profile { const char* name; const Macro* macros; };
static const Profile PROFILES[3] = {
  {"Dev", DEV}, {"System", SYS}, {"Snippets", SNIP},
};

static void fireMacro(const Macro* m) {
  if (m->kind == K_STR) {
    Packet p = make_packet(Cmd::TYPE_STRING, m->str, (uint8_t)strlen(m->str), g_seq++);
    sendPacket(p);
  } else {
    KeyCombo c{ m->mods, 1, { m->key } };
    Packet p = make_packet(Cmd::KEY_COMBO, &c, sizeof(c), g_seq++);
    sendPacket(p);
  }
}

// --------------------------- LVGL glue ---------------------------
static lv_disp_draw_buf_t draw_buf;
static lv_color_t         lvbuf[240 * 40];

static void flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
  int32_t w = area->x2 - area->x1 + 1;
  int32_t h = area->y2 - area->y1 + 1;
  M5.Display.startWrite();
  M5.Display.setAddrWindow(area->x1, area->y1, w, h);
  M5.Display.writePixels(reinterpret_cast<uint16_t*>(&color_p->full), w * h, true);
  M5.Display.endWrite();
  lv_disp_flush_ready(drv);
}

static void touch_cb(lv_indev_drv_t*, lv_indev_data_t* data) {
  auto t = M5.Touch.getDetail();
  if (t.isPressed() || t.isHolding()) {
    data->state   = LV_INDEV_STATE_PRESSED;
    data->point.x = t.x;
    data->point.y = t.y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// --------------------------- UI ---------------------------
static lv_obj_t* lbl_batt;

static void btn_event(lv_event_t* e) {
  const Macro* m = static_cast<const Macro*>(lv_event_get_user_data(e));
  fireMacro(m);
}

static void makeButton(lv_obj_t* parent, const Macro* m, int x, int y, int w, int h) {
  lv_obj_t* b = lv_btn_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_radius(b, 12, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(m->rgb), 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(b, 6, 0);
  lv_obj_set_style_shadow_opa(b, LV_OPA_40, 0);
  lv_obj_set_style_shadow_ofs_y(b, 3, 0);
  lv_obj_add_event_cb(b, btn_event, LV_EVENT_CLICKED, (void*)m);

  lv_obj_t* lab = lv_label_create(b);
  lv_label_set_text(lab, m->label);
  lv_obj_set_style_text_font(lab, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lab, lv_color_hex(0xFFFFFF), 0);
  lv_obj_center(lab);
}

static void build_ui() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0A0A12), 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  const int W = M5.Display.width();     // 240 (portrait)
  const int H = M5.Display.height();    // 320
  const int BAR_H = 30;

  // --- status bar ---
  lv_obj_t* bar = lv_obj_create(scr);
  lv_obj_set_size(bar, W, BAR_H);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x15151F), 0);
  lv_obj_set_style_pad_all(bar, 4, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(bar);
  lv_label_set_text(title, "ESP-KVM");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0x7FD6FF), 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 2, 0);

  lbl_batt = lv_label_create(bar);
  lv_label_set_text(lbl_batt, "--%");
  lv_obj_set_style_text_font(lbl_batt, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_batt, lv_color_hex(0xCCCCCC), 0);
  lv_obj_align(lbl_batt, LV_ALIGN_RIGHT_MID, -2, 0);

  // --- swipeable profile pages ---
  lv_obj_t* tv = lv_tileview_create(scr);
  lv_obj_set_pos(tv, 0, BAR_H);
  lv_obj_set_size(tv, W, H - BAR_H);
  lv_obj_set_style_bg_color(tv, lv_color_hex(0x0A0A12), 0);

  const int th = H - BAR_H;             // tile height
  const int HDR = 30;                   // profile-name header
  const int PAD = 8, COLS = 2, ROWS = 3;
  const int bw = (W - PAD * (COLS + 1)) / COLS;
  const int bh = (th - HDR - PAD * (ROWS + 1)) / ROWS;

  for (int pi = 0; pi < 3; pi++) {
    lv_obj_t* tile = lv_tileview_add_tile(tv, pi, 0, LV_DIR_HOR);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* name = lv_label_create(tile);
    lv_label_set_text(name, PROFILES[pi].name);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 4);

    for (int i = 0; i < 6; i++) {
      int col = i % COLS, row = i / COLS;
      int x = PAD + col * (bw + PAD);
      int y = HDR + PAD + row * (bh + PAD);
      makeButton(tile, &PROFILES[pi].macros[i], x, y, bw, bh);
    }
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(2);            // portrait, USB-C at bottom

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, lvbuf, nullptr, 240 * 40);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res  = M5.Display.width();
  disp_drv.ver_res  = M5.Display.height();
  disp_drv.flush_cb = flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type    = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touch_cb;
  lv_indev_drv_register(&indev_drv);

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

  build_ui();
}

void loop() {
  M5.update();
  lv_timer_handler();

  static uint32_t lastBatt = 0;
  if (millis() - lastBatt > 5000) {
    lastBatt = millis();
    int lvl = M5.Power.getBatteryLevel();
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", lvl);
    lv_label_set_text(lbl_batt, buf);
  }
  delay(5);
}
