// Nexus controller — Phase 4: LVGL touch macro deck (+ ANNOUNCE for M5Paper).
// Swipeable profile pages of styled buttons; a tap sends the command to the dongle
// AND an ANNOUNCE (profile+button) so the M5Paper can show the legend/log. A swipe
// sends an ANNOUNCE with button=0xFF (profile switch only).
#include <M5Unified.h>
#include <lvgl.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <espkvm_proto.h>
#include <espkvm_macros.h>
#include "common/version.h"

using namespace espkvm;

static uint8_t  BCAST[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
static constexpr uint8_t ESPNOW_CHAN = 1;
static uint16_t g_seq = 0;

static void sendPacket(const Packet& p) {
  esp_now_send(BCAST, reinterpret_cast<const uint8_t*>(&p), sizeof(p));
}
static void sendAnnounce(uint8_t pi, uint8_t bi) {
  Announce a{pi, bi};
  Packet p = make_packet(Cmd::ANNOUNCE, &a, sizeof(a), g_seq++);
  sendPacket(p);
}
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
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = t.x; data->point.y = t.y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

// --------------------------- UI ---------------------------
static lv_obj_t* lbl_batt;
static lv_obj_t* g_tv;
static constexpr int NUM_TILES = PROFILE_COUNT + 1;   // 3 profiles + Monitor
static lv_obj_t* g_dots[NUM_TILES];
static lv_style_transition_dsc_t g_btn_tr;
static const lv_style_prop_t g_btn_tr_props[] = { LV_STYLE_TRANSFORM_ZOOM, LV_STYLE_BG_COLOR, (lv_style_prop_t)0 };

static void updateDots(int active) {
  for (int i = 0; i < NUM_TILES; i++)
    lv_obj_set_style_bg_color(g_dots[i], lv_color_hex(i == active ? 0x7FD6FF : 0x3A3F4B), 0);
}
static void sendMetric(uint8_t m) {
  Packet p = make_packet(Cmd::SET_METRIC, &m, 1, g_seq++);
  sendPacket(p);
}
static void metric_event(lv_event_t* e) {
  sendMetric((uint8_t)(intptr_t)lv_event_get_user_data(e));
}

static void btn_event(lv_event_t* e) {
  intptr_t idx = (intptr_t)lv_event_get_user_data(e);
  uint8_t pi = (uint8_t)(idx / BTN_COUNT), bi = (uint8_t)(idx % BTN_COUNT);
  fireMacro(&PROFILES[pi].macros[bi]);
  sendAnnounce(pi, bi);
}
static void tv_event(lv_event_t*) {
  lv_obj_t* act = lv_tileview_get_tile_act(g_tv);
  if (!act) return;
  intptr_t pi = (intptr_t)lv_obj_get_user_data(act);
  updateDots((int)pi);
  if (pi < PROFILE_COUNT) sendAnnounce((uint8_t)pi, 0xFF);
}

static void makeButton(lv_obj_t* parent, const Macro* m, intptr_t idx, int x, int y, int w, int h) {
  lv_obj_t* b = lv_btn_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_radius(b, 12, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(m->rgb), 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
  lv_obj_set_style_shadow_width(b, 6, 0);
  lv_obj_set_style_shadow_opa(b, LV_OPA_40, 0);
  lv_obj_set_style_shadow_ofs_y(b, 3, 0);
  // press animation: shrink slightly, smooth transition
  lv_obj_set_style_transform_pivot_x(b, LV_PCT(50), 0);
  lv_obj_set_style_transform_pivot_y(b, LV_PCT(50), 0);
  lv_obj_set_style_transform_zoom(b, 236, LV_STATE_PRESSED);
  lv_obj_set_style_transition(b, &g_btn_tr, 0);
  lv_obj_add_event_cb(b, btn_event, LV_EVENT_CLICKED, (void*)idx);

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

  const int W = M5.Display.width(), H = M5.Display.height(), BAR_H = 30;

  lv_obj_t* bar = lv_obj_create(scr);
  lv_obj_set_size(bar, W, BAR_H);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x15151F), 0);
  lv_obj_set_style_pad_all(bar, 4, 0);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t* title = lv_label_create(bar);
  lv_label_set_text(title, "Nexus");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0x7FD6FF), 0);
  lv_obj_align(title, LV_ALIGN_LEFT_MID, 2, 0);

  lbl_batt = lv_label_create(bar);
  lv_label_set_text(lbl_batt, "--%");
  lv_obj_set_style_text_font(lbl_batt, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(lbl_batt, lv_color_hex(0xCCCCCC), 0);
  lv_obj_align(lbl_batt, LV_ALIGN_RIGHT_MID, -2, 0);

  // page-indicator dots (centered in the bar)
  for (int i = 0; i < NUM_TILES; i++) {
    lv_obj_t* d = lv_obj_create(bar);
    lv_obj_set_size(d, 7, 7);
    lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(d, 0, 0);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(d, LV_ALIGN_CENTER, i * 14 - 21, 0);
    g_dots[i] = d;
  }

  lv_style_transition_dsc_init(&g_btn_tr, g_btn_tr_props, lv_anim_path_ease_out, 110, 0, nullptr);

  lv_obj_t* tv = lv_tileview_create(scr);
  g_tv = tv;
  lv_obj_set_pos(tv, 0, BAR_H);
  lv_obj_set_size(tv, W, H - BAR_H);
  lv_obj_set_style_bg_color(tv, lv_color_hex(0x0A0A12), 0);
  lv_obj_add_event_cb(tv, tv_event, LV_EVENT_VALUE_CHANGED, nullptr);

  const int th = H - BAR_H, HDR = 30, PAD = 8, COLS = 2, ROWS = 3;
  const int bw = (W - PAD * (COLS + 1)) / COLS;
  const int bh = (th - HDR - PAD * (ROWS + 1)) / ROWS;

  for (int pi = 0; pi < PROFILE_COUNT; pi++) {
    lv_obj_t* tile = lv_tileview_add_tile(tv, pi, 0, LV_DIR_HOR);
    lv_obj_set_user_data(tile, (void*)(intptr_t)pi);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* name = lv_label_create(tile);
    lv_label_set_text(name, PROFILES[pi].name);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(name, LV_ALIGN_TOP_MID, 0, 4);

    for (int i = 0; i < BTN_COUNT; i++) {
      int col = i % COLS, row = i / COLS;
      int x = PAD + col * (bw + PAD);
      int y = HDR + PAD + row * (bh + PAD);
      makeButton(tile, &PROFILES[pi].macros[i], pi * BTN_COUNT + i, x, y, bw, bh);
    }
  }

  // Monitor tile: 2x2 buttons that set the StickC's active metric over ESP-NOW.
  {
    lv_obj_t* mt = lv_tileview_add_tile(tv, PROFILE_COUNT, 0, LV_DIR_HOR);
    lv_obj_set_user_data(mt, (void*)(intptr_t)PROFILE_COUNT);
    lv_obj_clear_flag(mt, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t* mn = lv_label_create(mt);
    lv_label_set_text(mn, "Monitor");
    lv_obj_set_style_text_font(mn, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(mn, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(mn, LV_ALIGN_TOP_MID, 0, 4);
    const uint32_t mcol[MET_COUNT] = { 0x1565C0, 0x6A1B9A, 0xEF6C00, 0x00838F };
    const int mc = 2, mr = 2;
    int mbw = (W - PAD * (mc + 1)) / mc;
    int mbh = (th - HDR - PAD * (mr + 1)) / mr;
    for (int i = 0; i < MET_COUNT; i++) {
      int col = i % mc, row = i / mc;
      int bx = PAD + col * (mbw + PAD);
      int by = HDR + PAD + row * (mbh + PAD);
      lv_obj_t* b = lv_btn_create(mt);
      lv_obj_set_pos(b, bx, by); lv_obj_set_size(b, mbw, mbh);
      lv_obj_set_style_radius(b, 12, 0);
      lv_obj_set_style_bg_color(b, lv_color_hex(mcol[i]), 0);
      lv_obj_set_style_bg_color(b, lv_color_hex(0xFFFFFF), LV_STATE_PRESSED);
      lv_obj_set_style_transform_pivot_x(b, LV_PCT(50), 0);
      lv_obj_set_style_transform_pivot_y(b, LV_PCT(50), 0);
      lv_obj_set_style_transform_zoom(b, 236, LV_STATE_PRESSED);
      lv_obj_set_style_transition(b, &g_btn_tr, 0);
      lv_obj_add_event_cb(b, metric_event, LV_EVENT_CLICKED, (void*)(intptr_t)i);
      lv_obj_t* lab = lv_label_create(b);
      lv_label_set_text(lab, METRIC_NAMES[i]);
      lv_obj_set_style_text_font(lab, &lv_font_montserrat_20, 0);
      lv_obj_set_style_text_color(lab, lv_color_hex(0xFFFFFF), 0);
      lv_obj_center(lab);
    }
  }
}

void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);
  M5.Display.setRotation(2);
  Serial.begin(115200);   // USB serial: Mac stats in (relayed to StickC)

  lv_init();
  lv_disp_draw_buf_init(&draw_buf, lvbuf, nullptr, 240 * 40);
  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = M5.Display.width();
  disp_drv.ver_res = M5.Display.height();
  disp_drv.flush_cb = flush_cb;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
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
  updateDots(0);
  sendAnnounce(0, 0xFF);   // tell the M5Paper the starting profile
}

void loop() {
  M5.update();
  lv_timer_handler();

  // Relay Mac system-monitor stats from USB serial to the StickC over ESP-NOW.
  static String sline;
  while (Serial.available()) {
    char ch = (char)Serial.read();
    if (ch == '\n' || ch == '\r') {
      int m, g, c, dsk;
      if (sscanf(sline.c_str(), "S %d %d %d %d", &m, &g, &c, &dsk) == 4) {
        auto cl = [](int v){ return (uint8_t)(v < 0 ? 0 : v > 100 ? 100 : v); };
        Stats st{ cl(m), cl(g), cl(c), cl(dsk) };
        Packet p = make_packet(Cmd::STATS, &st, sizeof(st), g_seq++);
        sendPacket(p);
      }
      sline = "";
    } else if (sline.length() < 28) sline += ch;
  }
  static uint32_t lastBatt = 0;
  if (millis() - lastBatt > 5000) {
    lastBatt = millis();
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", M5.Power.getBatteryLevel());
    lv_label_set_text(lbl_batt, buf);
  }
  delay(5);
}
