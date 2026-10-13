#include "ui_pendant.h"
#include "pins_config.h"
#include <stdio.h>
#include <string.h>

// Rich MPG chrome for CrowPanel Advance 7" (480×800 portrait).
// Extra room vs stock 320×480: WPos + MPos per axis, laser S, feed, overrides.

static lv_obj_t *scr;
static lv_obj_t *header;
static lv_obj_t *content;
static lv_obj_t *mpg_panel;
static lv_obj_t *other_panel;
static lv_obj_t *status_bar;
static lv_obj_t *mpg_btn;
static lv_obj_t *state_lbl;
static lv_obj_t *page_lbl;

static lv_obj_t *axis_rows[3];
static lv_obj_t *axis_wpos[3];
static lv_obj_t *axis_mpos[3];
static lv_obj_t *step_btns[4];
static lv_obj_t *soft_btns[3];

static lv_obj_t *feed_lbl;
static lv_obj_t *laser_cmd_lbl;
static lv_obj_t *laser_act_lbl;
static lv_obj_t *ov_lbl;

static const char *k_pages[] = {"MPG", "OVERRIDE", "FEED", "GCODE", "PROBE", "SETTINGS"};
static const float k_steps[] = {0.01f, 0.1f, 1.0f, 10.0f};
static const char *k_axis[] = {"X", "Y", "Z"};

static int page_idx = 0;
static int active_axis = 0;
static int step_idx = 1;
static bool mpg_on = true;

// Mock machine state (replaced by grblHAL status later)
static float wpos[3] = {12.340f, -45.678f, 3.200f};
static float wco[3] = {10.0f, 20.0f, 0.0f};  // work coordinate offset → MPos = WPos + WCO
static float feed_mm_min = 1200.0f;
static float laser_s_cmd = 650.0f;   // commanded S (laser power)
static float laser_s_act = 648.0f;   // reported actual
static int feed_ov = 100;
static int speed_ov = 100;
static int rapid_ov = 100;

static void style_chrome_btn(lv_obj_t *btn, bool active) {
  lv_obj_set_style_radius(btn, 2, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_border_color(btn, lv_color_hex(0x111111), 0);
  lv_obj_set_style_bg_color(btn, active ? lv_color_hex(0x3d5a80) : lv_color_hex(0x2f2f2f), 0);
  lv_obj_set_style_text_color(btn, lv_color_hex(0xffffff), 0);
}

static void refresh_telemetry() {
  char buf[48];
  snprintf(buf, sizeof(buf), "F %0.0f", feed_mm_min);
  lv_label_set_text(feed_lbl, buf);

  snprintf(buf, sizeof(buf), "S %0.0f", laser_s_cmd);
  lv_label_set_text(laser_cmd_lbl, buf);

  snprintf(buf, sizeof(buf), "act %0.0f", laser_s_act);
  lv_label_set_text(laser_act_lbl, buf);

  snprintf(buf, sizeof(buf), "Ov F%d S%d R%d", feed_ov, speed_ov, rapid_ov);
  lv_label_set_text(ov_lbl, buf);
}

static void refresh_axes() {
  for (int i = 0; i < 3; i++) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%0.3f", wpos[i]);
    lv_label_set_text(axis_wpos[i], buf);
    snprintf(buf, sizeof(buf), "M %0.3f", wpos[i] + wco[i]);
    lv_label_set_text(axis_mpos[i], buf);

    style_chrome_btn(axis_rows[i], i == active_axis);
    if (i == active_axis) {
      lv_obj_set_style_border_color(axis_rows[i], lv_color_hex(0xf0c040), 0);
      lv_obj_set_style_border_width(axis_rows[i], 2, 0);
    }
  }
}

static void refresh_steps() {
  for (int i = 0; i < 4; i++) {
    style_chrome_btn(step_btns[i], i == step_idx);
    if (i == step_idx) {
      lv_obj_set_style_bg_color(step_btns[i], lv_color_hex(0x0369a1), 0);
    }
  }
}

static void refresh_mpg() {
  lv_obj_set_style_bg_color(mpg_btn, mpg_on ? lv_color_hex(0x059669) : lv_color_hex(0x2f2f2f), 0);
}

static void show_page(int idx) {
  page_idx = idx;
  lv_label_set_text(page_lbl, k_pages[idx]);
  if (idx == 0) {
    lv_obj_clear_flag(mpg_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(other_panel, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(mpg_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(other_panel, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lv_obj_get_child(other_panel, 0), k_pages[idx]);
  }
}

static void on_tab(lv_event_t *e) {
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  uint32_t n = lv_obj_get_child_cnt(header);
  for (uint32_t i = 0; i < n; i++) {
    style_chrome_btn(lv_obj_get_child(header, i), (int)i == idx);
  }
  show_page(idx);
}

static void on_axis(lv_event_t *e) {
  active_axis = (int)(intptr_t)lv_event_get_user_data(e);
  refresh_axes();
}

static void on_zero(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  // Zero WPos: shift WCO so machine stays put
  wco[i] += wpos[i];
  wpos[i] = 0.0f;
  refresh_axes();
  lv_event_stop_bubbling(e);
}

static void on_step(lv_event_t *e) {
  step_idx = (int)(intptr_t)lv_event_get_user_data(e);
  refresh_steps();
}

static void on_mpg(lv_event_t *e) {
  (void)e;
  mpg_on = !mpg_on;
  refresh_mpg();
}

static void on_laser_bump(lv_event_t *e) {
  int dir = (int)(intptr_t)lv_event_get_user_data(e);
  laser_s_cmd += dir * 10.0f;
  if (laser_s_cmd < 0) laser_s_cmd = 0;
  if (laser_s_cmd > 1000) laser_s_cmd = 1000;
  laser_s_act = laser_s_cmd - 2.0f;
  if (laser_s_act < 0) laser_s_act = 0;
  refresh_telemetry();
}

static void on_soft(lv_event_t *e) {
  int which = (int)(intptr_t)lv_event_get_user_data(e);
  if (page_idx == 0) {
    if (which == 1) {
      wpos[active_axis] -= k_steps[step_idx];
    } else if (which == 2) {
      wpos[active_axis] += k_steps[step_idx];
    }
    refresh_axes();
  } else if (which == 0) {
    show_page(0);
    uint32_t n = lv_obj_get_child_cnt(header);
    for (uint32_t i = 0; i < n; i++) {
      style_chrome_btn(lv_obj_get_child(header, i), i == 0);
    }
  }
}

static lv_obj_t *make_telemetry_cell(lv_obj_t *parent, const char *title, lv_obj_t **value_out) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_set_flex_grow(box, 1);
  lv_obj_set_height(box, LV_PCT(100));
  lv_obj_set_style_bg_color(box, lv_color_hex(0x2a2a2a), 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, lv_color_hex(0x111111), 0);
  lv_obj_set_style_radius(box, 2, 0);
  lv_obj_set_style_pad_all(box, 4, 0);

  lv_obj_t *t = lv_label_create(box);
  lv_label_set_text(t, title);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(0xaaaaaa), 0);
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

  *value_out = lv_label_create(box);
  lv_obj_set_style_text_font(*value_out, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(*value_out, lv_color_hex(0xffffff), 0);
  lv_obj_align(*value_out, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  return box;
}

void ui_pendant_create() {
  scr = lv_obj_create(NULL);
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x404040), 0);
  lv_obj_set_style_pad_all(scr, 0, 0);
  lv_scr_load(scr);

  header = lv_obj_create(scr);
  lv_obj_set_size(header, UI_H_RES, SCREEN_HEADER_H);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_style_pad_all(header, 0, 0);
  lv_obj_set_style_bg_color(header, lv_color_hex(0x1e1e1e), 0);
  lv_obj_set_style_border_width(header, 0, 0);
  lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
  lv_obj_set_style_pad_gap(header, 0, 0);

  for (int i = 0; i < 6; i++) {
    lv_obj_t *btn = lv_btn_create(header);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_height(btn, SCREEN_HEADER_H);
    style_chrome_btn(btn, i == 0);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, k_pages[i]);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(btn, on_tab, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  const int status_h = 40;
  const int content_h = UI_V_RES - SCREEN_HEADER_H - status_h - SCREEN_SOFT_BTN_H;

  content = lv_obj_create(scr);
  lv_obj_set_size(content, UI_H_RES, content_h);
  lv_obj_set_pos(content, 0, SCREEN_HEADER_H);
  lv_obj_set_style_bg_color(content, lv_color_hex(0x404040), 0);
  lv_obj_set_style_border_width(content, 0, 0);
  lv_obj_set_style_pad_all(content, 6, 0);

  page_lbl = lv_label_create(content);
  lv_obj_set_style_text_color(page_lbl, lv_color_hex(0xcccccc), 0);
  lv_obj_align(page_lbl, LV_ALIGN_TOP_RIGHT, -4, 0);
  lv_label_set_text(page_lbl, "MPG");

  // ---- MPG panel ----
  mpg_panel = lv_obj_create(content);
  lv_obj_set_size(mpg_panel, UI_H_RES - 12, content_h - 8);
  lv_obj_set_pos(mpg_panel, 0, 0);
  lv_obj_set_style_bg_opa(mpg_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(mpg_panel, 0, 0);
  lv_obj_set_style_pad_all(mpg_panel, 0, 0);

  // Column headers
  lv_obj_t *col_hdr = lv_label_create(mpg_panel);
  lv_label_set_text(col_hdr, "     WPos            MPos");
  lv_obj_set_style_text_font(col_hdr, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(col_hdr, lv_color_hex(0xaaaaaa), 0);
  lv_obj_set_pos(col_hdr, 40, 2);

  const int row_h = 70;
  for (int i = 0; i < 3; i++) {
    axis_rows[i] = lv_btn_create(mpg_panel);
    lv_obj_set_size(axis_rows[i], UI_H_RES - 12, row_h);
    lv_obj_set_pos(axis_rows[i], 0, 20 + i * (row_h + 6));
    style_chrome_btn(axis_rows[i], i == 0);
    lv_obj_add_event_cb(axis_rows[i], on_axis, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    lv_obj_t *name = lv_label_create(axis_rows[i]);
    lv_label_set_text(name, k_axis[i]);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(0xf0c040), 0);
    lv_obj_align(name, LV_ALIGN_LEFT_MID, 8, 0);

    axis_wpos[i] = lv_label_create(axis_rows[i]);
    lv_obj_set_style_text_font(axis_wpos[i], &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(axis_wpos[i], lv_color_hex(0xffffff), 0);
    lv_obj_align(axis_wpos[i], LV_ALIGN_LEFT_MID, 48, -8);

    axis_mpos[i] = lv_label_create(axis_rows[i]);
    lv_obj_set_style_text_font(axis_mpos[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(axis_mpos[i], lv_color_hex(0xa0c4e8), 0);
    lv_obj_align(axis_mpos[i], LV_ALIGN_LEFT_MID, 48, 18);

    lv_obj_t *zero = lv_btn_create(axis_rows[i]);
    lv_obj_set_size(zero, 56, row_h - 16);
    lv_obj_align(zero, LV_ALIGN_RIGHT_MID, -6, 0);
    style_chrome_btn(zero, false);
    lv_obj_t *zl = lv_label_create(zero);
    lv_label_set_text(zl, "<0>");
    lv_obj_center(zl);
    lv_obj_add_event_cb(zero, on_zero, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  // Telemetry strip: feed, laser commanded, laser actual, overrides
  const int tele_y = 20 + 3 * (row_h + 6) + 4;
  lv_obj_t *tele = lv_obj_create(mpg_panel);
  lv_obj_set_size(tele, UI_H_RES - 12, 72);
  lv_obj_set_pos(tele, 0, tele_y);
  lv_obj_set_style_bg_opa(tele, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tele, 0, 0);
  lv_obj_set_style_pad_all(tele, 0, 0);
  lv_obj_set_style_pad_gap(tele, 4, 0);
  lv_obj_set_flex_flow(tele, LV_FLEX_FLOW_ROW);

  lv_obj_t *feed_box = make_telemetry_cell(tele, "FEED mm/min", &feed_lbl);
  (void)feed_box;

  lv_obj_t *laser_box = make_telemetry_cell(tele, "LASER S cmd", &laser_cmd_lbl);
  lv_obj_add_flag(laser_box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(laser_box, on_laser_bump, LV_EVENT_CLICKED, (void *)(intptr_t)1);

  lv_obj_t *act_box = make_telemetry_cell(tele, "LASER S act", &laser_act_lbl);
  lv_obj_add_flag(act_box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(act_box, on_laser_bump, LV_EVENT_CLICKED, (void *)(intptr_t)-1);

  lv_obj_t *ov_box = make_telemetry_cell(tele, "OVERRIDES", &ov_lbl);
  (void)ov_box;

  // Step buttons
  const int step_y = tele_y + 80;
  for (int i = 0; i < 4; i++) {
    step_btns[i] = lv_btn_create(mpg_panel);
    int w = (UI_H_RES - 12 - 9) / 4;
    lv_obj_set_size(step_btns[i], w, 48);
    lv_obj_set_pos(step_btns[i], i * (w + 3), step_y);
    char buf[16];
    snprintf(buf, sizeof(buf), "%g", k_steps[i]);
    lv_obj_t *lbl = lv_label_create(step_btns[i]);
    lv_label_set_text(lbl, buf);
    lv_obj_center(lbl);
    style_chrome_btn(step_btns[i], i == step_idx);
    lv_obj_add_event_cb(step_btns[i], on_step, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  // Placeholder for other tabs
  other_panel = lv_obj_create(content);
  lv_obj_set_size(other_panel, UI_H_RES - 12, content_h - 8);
  lv_obj_set_pos(other_panel, 0, 0);
  lv_obj_set_style_bg_opa(other_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(other_panel, 0, 0);
  lv_obj_t *other_lbl = lv_label_create(other_panel);
  lv_obj_set_style_text_font(other_lbl, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(other_lbl, lv_color_hex(0xffffff), 0);
  lv_obj_center(other_lbl);
  lv_obj_add_flag(other_panel, LV_OBJ_FLAG_HIDDEN);

  // Status + MPG
  status_bar = lv_obj_create(scr);
  lv_obj_set_size(status_bar, UI_H_RES, status_h);
  lv_obj_set_pos(status_bar, 0, SCREEN_HEADER_H + content_h);
  lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_width(status_bar, 0, 0);
  lv_obj_set_style_pad_all(status_bar, 4, 0);
  lv_obj_set_style_radius(status_bar, 0, 0);

  state_lbl = lv_label_create(status_bar);
  lv_label_set_text(state_lbl, "Idle  ·  Laser  ·  mm");
  lv_obj_set_style_text_color(state_lbl, lv_color_hex(0xffffff), 0);
  lv_obj_align(state_lbl, LV_ALIGN_LEFT_MID, 4, 0);

  mpg_btn = lv_btn_create(status_bar);
  lv_obj_set_size(mpg_btn, 72, status_h - 4);
  lv_obj_align(mpg_btn, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_t *mpg_lbl = lv_label_create(mpg_btn);
  lv_label_set_text(mpg_lbl, "MPG");
  lv_obj_center(mpg_lbl);
  lv_obj_add_event_cb(mpg_btn, on_mpg, LV_EVENT_CLICKED, NULL);
  refresh_mpg();

  lv_obj_t *soft = lv_obj_create(scr);
  lv_obj_set_size(soft, UI_H_RES, SCREEN_SOFT_BTN_H);
  lv_obj_set_pos(soft, 0, UI_V_RES - SCREEN_SOFT_BTN_H);
  lv_obj_set_style_pad_all(soft, 0, 0);
  lv_obj_set_style_pad_gap(soft, 1, 0);
  lv_obj_set_style_bg_color(soft, lv_color_hex(0x000000), 0);
  lv_obj_set_style_border_width(soft, 0, 0);
  lv_obj_set_flex_flow(soft, LV_FLEX_FLOW_ROW);

  const char *soft_labels[] = {"Hold", "- Jog", "+ Jog"};
  for (int i = 0; i < 3; i++) {
    soft_btns[i] = lv_btn_create(soft);
    lv_obj_set_flex_grow(soft_btns[i], 1);
    lv_obj_set_height(soft_btns[i], SCREEN_SOFT_BTN_H);
    style_chrome_btn(soft_btns[i], false);
    lv_obj_t *lbl = lv_label_create(soft_btns[i]);
    lv_label_set_text(lbl, soft_labels[i]);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_center(lbl);
    lv_obj_add_event_cb(soft_btns[i], on_soft, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  refresh_axes();
  refresh_steps();
  refresh_telemetry();
  show_page(0);
}

void ui_pendant_tick() {
  // Future: pull WPos/MPos/S/F/Ov from grblHAL status parser
}
