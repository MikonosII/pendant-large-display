#include "ui_pendant.h"
#include "pins_config.h"
#include "gcode_path.h"
#include "sample_job_nc.h"
#include "grbl_link.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <esp_heap_caps.h>

// MPG dashboard: G-code toolpath preview + live laser cursor + DRO/telemetry.

static lv_obj_t *scr;
static lv_obj_t *header;
static lv_obj_t *content;
static lv_obj_t *mpg_panel;
static lv_obj_t *other_panel;
static lv_obj_t *status_bar;
static lv_obj_t *mpg_btn;
static lv_obj_t *state_lbl;
static lv_obj_t *page_lbl;
static lv_obj_t *job_lbl;

static lv_obj_t *path_canvas;
static lv_obj_t *head_dot;
static lv_color_t *path_cbuf = nullptr;
static int path_w = 0;
static int path_h = 0;
static float path_min_x = 0, path_min_y = 0, path_scale = 1;

static path_seg_t *job_path = nullptr;
static unsigned job_path_len = 0;
static char job_name[48] = "sample-badge.nc";

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

static float wpos[3] = {40.0f, 25.0f, 0.0f};
static float wco[3] = {0.0f, 0.0f, 0.0f};
static float feed_mm_min = 1200.0f;
static float laser_s_cmd = 650.0f;
static float laser_s_act = 648.0f;
static const float laser_s_max = 1000.0f;  // S max → 100% (grblHAL $30)
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

static void map_xy(float x, float y, lv_coord_t *px, lv_coord_t *py) {
  // Machine Y up → canvas Y down
  *px = (lv_coord_t)((x - path_min_x) * path_scale);
  *py = (lv_coord_t)(path_h - (y - path_min_y) * path_scale);
}

static void load_sample_job() {
  if (!job_path) {
    job_path = (path_seg_t *)heap_caps_malloc(sizeof(path_seg_t) * GCODE_PATH_MAX_SEGS,
                                              MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!job_path) {
      job_path = (path_seg_t *)malloc(sizeof(path_seg_t) * GCODE_PATH_MAX_SEGS);
    }
  }
  if (!job_path) {
    job_path_len = 0;
    Serial.println("[gcode] path buffer alloc failed");
    return;
  }
  job_path_len = gcode_parse_toolpath(SAMPLE_JOB_NC, job_path, GCODE_PATH_MAX_SEGS);
  strncpy(job_name, SAMPLE_JOB_NAME, sizeof(job_name) - 1);
  job_name[sizeof(job_name) - 1] = '\0';
  Serial.printf("[gcode] loaded %s → %u segs\n", job_name, job_path_len);
}

static void draw_toolpath() {
  if (!path_canvas || !path_cbuf) return;
  lv_canvas_fill_bg(path_canvas, lv_color_hex(0x1a1a1a), LV_OPA_COVER);

  lv_draw_line_dsc_t rapid_dsc;
  lv_draw_line_dsc_init(&rapid_dsc);
  rapid_dsc.color = lv_color_hex(0x555555);
  rapid_dsc.width = 1;
  rapid_dsc.dash_gap = 3;
  rapid_dsc.dash_width = 3;

  lv_draw_line_dsc_t cut_dsc;
  lv_draw_line_dsc_init(&cut_dsc);
  cut_dsc.color = lv_color_hex(0xf59e0b);
  cut_dsc.width = 2;

  if (!job_path || job_path_len < 2) return;
  for (unsigned i = 1; i < job_path_len; i++) {
    lv_point_t pts[2];
    map_xy(job_path[i - 1].x, job_path[i - 1].y, &pts[0].x, &pts[0].y);
    map_xy(job_path[i].x, job_path[i].y, &pts[1].x, &pts[1].y);
    lv_canvas_draw_line(path_canvas, pts, 2, job_path[i].cut ? &cut_dsc : &rapid_dsc);
  }
}

static void update_head() {
  if (!head_dot || path_scale <= 0) return;
  lv_coord_t px, py;
  map_xy(wpos[0], wpos[1], &px, &py);
  // Center the 12×12 marker
  lv_obj_set_pos(head_dot, px - 6, py - 6);
  lv_obj_set_style_bg_color(head_dot,
                            laser_s_cmd > 0 ? lv_color_hex(0xef4444) : lv_color_hex(0x38bdf8), 0);
}

static int laser_pct(float s) {
  int pct = (int)((s * 100.0f) / laser_s_max + 0.5f);
  if (pct < 0) pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

static void refresh_telemetry() {
  char buf[48];
  snprintf(buf, sizeof(buf), "F %0.0f", feed_mm_min);
  lv_label_set_text(feed_lbl, buf);
  // Fixed-width so CMD / ACT columns stay aligned (e.g. " 65%" / "100%")
  snprintf(buf, sizeof(buf), "%3d%%", laser_pct(laser_s_cmd));
  lv_label_set_text(laser_cmd_lbl, buf);
  snprintf(buf, sizeof(buf), "%3d%%", laser_pct(laser_s_act));
  lv_label_set_text(laser_act_lbl, buf);
  snprintf(buf, sizeof(buf), "Ov F%d S%d R%d", feed_ov, speed_ov, rapid_ov);
  lv_label_set_text(ov_lbl, buf);
  update_head();
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
  update_head();
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
    if (idx == 3 && job_lbl) {
      char buf[96];
      snprintf(buf, sizeof(buf), "GCODE\n%s\n%u segments", job_name, job_path_len);
      lv_label_set_text(job_lbl, buf);
      lv_label_set_text(lv_obj_get_child(other_panel, 0), "");
    } else {
      lv_label_set_text(lv_obj_get_child(other_panel, 0), k_pages[idx]);
      if (job_lbl) lv_label_set_text(job_lbl, "");
    }
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

static float jog_feed_for_step() {
  float s = k_steps[step_idx];
  if (s >= 1.0f) return 2000.0f;
  if (s >= 0.1f) return 800.0f;
  return 200.0f;
}

static void on_zero(lv_event_t *e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (grbl_link_online()) {
    grbl_link_zero_axis(i);
  } else {
    wco[i] += wpos[i];
    wpos[i] = 0.0f;
    refresh_axes();
  }
  lv_event_stop_bubbling(e);
}

static void on_step(lv_event_t *e) {
  step_idx = (int)(intptr_t)lv_event_get_user_data(e);
  refresh_steps();
}

static void on_mpg(lv_event_t *e) {
  (void)e;
  mpg_on = !mpg_on;
  grbl_link_set_mpg(mpg_on);
  refresh_mpg();
}

static void on_laser_bump(lv_event_t *e) {
  // Demo-only bump when offline; live S comes from grblHAL FS: when linked.
  if (grbl_link_online()) return;
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
    if (which == 0) {
      if (grbl_link_online()) grbl_link_feed_hold();
      return;
    }
    float delta = (which == 1) ? -k_steps[step_idx] : k_steps[step_idx];
    if (grbl_link_online()) {
      grbl_link_jog(active_axis, delta, jog_feed_for_step());
    } else {
      wpos[active_axis] += delta;
      refresh_axes();
    }
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
  lv_obj_set_style_pad_all(box, 3, 0);

  lv_obj_t *t = lv_label_create(box);
  lv_label_set_text(t, title);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(0xaaaaaa), 0);
  lv_obj_align(t, LV_ALIGN_TOP_LEFT, 0, 0);

  *value_out = lv_label_create(box);
  lv_obj_set_style_text_font(*value_out, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(*value_out, lv_color_hex(0xffffff), 0);
  lv_obj_align(*value_out, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  return box;
}

/** Paired CMD / ACT % columns that share one LASER header so values line up. */
static lv_obj_t *make_laser_pair(lv_obj_t *parent, lv_obj_t **cmd_out, lv_obj_t **act_out) {
  lv_obj_t *box = lv_obj_create(parent);
  lv_obj_set_flex_grow(box, 2);
  lv_obj_set_height(box, LV_PCT(100));
  lv_obj_set_style_bg_color(box, lv_color_hex(0x2a2a2a), 0);
  lv_obj_set_style_border_width(box, 1, 0);
  lv_obj_set_style_border_color(box, lv_color_hex(0x111111), 0);
  lv_obj_set_style_radius(box, 2, 0);
  lv_obj_set_style_pad_all(box, 3, 0);
  lv_obj_set_style_pad_column(box, 8, 0);
  lv_obj_clear_flag(box, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(box);
  lv_label_set_text(title, "LASER");
  lv_obj_set_style_text_font(title, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(title, lv_color_hex(0xaaaaaa), 0);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *row = lv_obj_create(box);
  lv_obj_set_size(row, LV_PCT(100), 22);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_style_pad_column(row, 6, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < 2; i++) {
    lv_obj_t *col = lv_obj_create(row);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_set_height(col, LV_PCT(100));
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *tag = lv_label_create(col);
    lv_label_set_text(tag, i == 0 ? "CMD" : "ACT");
    lv_obj_set_style_text_font(tag, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tag, lv_color_hex(0x888888), 0);
    lv_obj_align(tag, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *val = lv_label_create(col);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(val, lv_color_hex(0xffffff), 0);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);
    if (i == 0) *cmd_out = val;
    else *act_out = val;
  }
  return box;
}

static void setup_path_preview(lv_obj_t *parent, int y, int w, int h) {
  path_w = w;
  path_h = h;

  load_sample_job();

  float min_x = 1e9f, min_y = 1e9f, max_x = -1e9f, max_y = -1e9f;
  for (unsigned i = 0; i < job_path_len; i++) {
    if (job_path[i].x < min_x) min_x = job_path[i].x;
    if (job_path[i].y < min_y) min_y = job_path[i].y;
    if (job_path[i].x > max_x) max_x = job_path[i].x;
    if (job_path[i].y > max_y) max_y = job_path[i].y;
  }
  if (job_path_len == 0) {
    min_x = 0;
    min_y = 0;
    max_x = 80;
    max_y = 50;
  }
  const float pad = 4.0f;
  min_x -= pad;
  min_y -= pad;
  max_x += pad;
  max_y += pad;
  float bw = max_x - min_x;
  float bh = max_y - min_y;
  if (bw < 1) bw = 1;
  if (bh < 1) bh = 1;
  float sx = (float)(w - 4) / bw;
  float sy = (float)(h - 4) / bh;
  path_scale = sx < sy ? sx : sy;
  path_min_x = min_x - ((w / path_scale) - bw) * 0.5f;
  path_min_y = min_y - ((h / path_scale) - bh) * 0.5f;

  lv_obj_t *frame = lv_obj_create(parent);
  lv_obj_set_size(frame, w, h);
  lv_obj_set_pos(frame, 0, y);
  lv_obj_set_style_bg_color(frame, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_style_border_width(frame, 1, 0);
  lv_obj_set_style_border_color(frame, lv_color_hex(0x111111), 0);
  lv_obj_set_style_pad_all(frame, 0, 0);
  lv_obj_set_style_radius(frame, 2, 0);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *hint = lv_label_create(frame);
  char hint_buf[64];
  snprintf(hint_buf, sizeof(hint_buf), "Job · %s", job_name);
  lv_label_set_text(hint, hint_buf);
  lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(hint, lv_color_hex(0x666666), 0);
  lv_obj_set_pos(hint, 6, 4);

  size_t buf_sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR(w, h);
  path_cbuf = (lv_color_t *)heap_caps_malloc(buf_sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!path_cbuf) {
    path_cbuf = (lv_color_t *)heap_caps_malloc(buf_sz, MALLOC_CAP_8BIT);
  }
  if (!path_cbuf) {
    Serial.println("[ui] path canvas alloc failed");
    return;
  }

  path_canvas = lv_canvas_create(frame);
  lv_obj_set_size(path_canvas, w, h);
  lv_obj_set_pos(path_canvas, 0, 0);
  lv_canvas_set_buffer(path_canvas, path_cbuf, w, h, LV_IMG_CF_TRUE_COLOR);
  draw_toolpath();

  head_dot = lv_obj_create(frame);
  lv_obj_set_size(head_dot, 12, 12);
  lv_obj_set_style_radius(head_dot, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(head_dot, lv_color_hex(0x38bdf8), 0);
  lv_obj_set_style_border_width(head_dot, 2, 0);
  lv_obj_set_style_border_color(head_dot, lv_color_hex(0xffffff), 0);
  lv_obj_clear_flag(head_dot, LV_OBJ_FLAG_SCROLLABLE);
  update_head();
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

  mpg_panel = lv_obj_create(content);
  lv_obj_set_size(mpg_panel, UI_H_RES - 12, content_h - 8);
  lv_obj_set_pos(mpg_panel, 0, 0);
  lv_obj_set_style_bg_opa(mpg_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(mpg_panel, 0, 0);
  lv_obj_set_style_pad_all(mpg_panel, 0, 0);
  lv_obj_clear_flag(mpg_panel, LV_OBJ_FLAG_SCROLLABLE);

  const int preview_h = 260;
  setup_path_preview(mpg_panel, 0, UI_H_RES - 12, preview_h);

  // Compact 3-column DRO under the path
  const int dro_y = preview_h + 6;
  const int row_h = 56;
  const int col_w = (UI_H_RES - 12 - 8) / 3;
  for (int i = 0; i < 3; i++) {
    axis_rows[i] = lv_btn_create(mpg_panel);
    lv_obj_set_size(axis_rows[i], col_w, row_h);
    lv_obj_set_pos(axis_rows[i], i * (col_w + 4), dro_y);
    style_chrome_btn(axis_rows[i], i == 0);
    lv_obj_add_event_cb(axis_rows[i], on_axis, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    lv_obj_t *name = lv_label_create(axis_rows[i]);
    lv_label_set_text(name, k_axis[i]);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(0xf0c040), 0);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 4, 4);

    axis_wpos[i] = lv_label_create(axis_rows[i]);
    lv_obj_set_style_text_font(axis_wpos[i], &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(axis_wpos[i], lv_color_hex(0xffffff), 0);
    lv_obj_align(axis_wpos[i], LV_ALIGN_TOP_LEFT, 22, 4);

    axis_mpos[i] = lv_label_create(axis_rows[i]);
    lv_obj_set_style_text_font(axis_mpos[i], &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(axis_mpos[i], lv_color_hex(0xa0c4e8), 0);
    lv_obj_align(axis_mpos[i], LV_ALIGN_BOTTOM_LEFT, 4, -4);

    lv_obj_t *zero = lv_btn_create(axis_rows[i]);
    lv_obj_set_size(zero, 36, 22);
    lv_obj_align(zero, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
    style_chrome_btn(zero, false);
    lv_obj_t *zl = lv_label_create(zero);
    lv_label_set_text(zl, "0");
    lv_obj_set_style_text_font(zl, &lv_font_montserrat_12, 0);
    lv_obj_center(zl);
    lv_obj_add_event_cb(zero, on_zero, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  const int tele_y = dro_y + row_h + 6;
  lv_obj_t *tele = lv_obj_create(mpg_panel);
  lv_obj_set_size(tele, UI_H_RES - 12, 56);
  lv_obj_set_pos(tele, 0, tele_y);
  lv_obj_set_style_bg_opa(tele, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tele, 0, 0);
  lv_obj_set_style_pad_all(tele, 0, 0);
  lv_obj_set_style_pad_gap(tele, 4, 0);
  lv_obj_set_flex_flow(tele, LV_FLEX_FLOW_ROW);

  make_telemetry_cell(tele, "FEED", &feed_lbl);
  lv_obj_t *laser_box = make_laser_pair(tele, &laser_cmd_lbl, &laser_act_lbl);
  lv_obj_add_flag(laser_box, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(laser_box, on_laser_bump, LV_EVENT_CLICKED, (void *)(intptr_t)1);
  make_telemetry_cell(tele, "OV", &ov_lbl);

  const int step_y = tele_y + 62;
  for (int i = 0; i < 4; i++) {
    step_btns[i] = lv_btn_create(mpg_panel);
    int bw = (UI_H_RES - 12 - 9) / 4;
    lv_obj_set_size(step_btns[i], bw, 40);
    lv_obj_set_pos(step_btns[i], i * (bw + 3), step_y);
    char buf[16];
    snprintf(buf, sizeof(buf), "%g", k_steps[i]);
    lv_obj_t *lbl = lv_label_create(step_btns[i]);
    lv_label_set_text(lbl, buf);
    lv_obj_center(lbl);
    style_chrome_btn(step_btns[i], i == step_idx);
    lv_obj_add_event_cb(step_btns[i], on_step, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  other_panel = lv_obj_create(content);
  lv_obj_set_size(other_panel, UI_H_RES - 12, content_h - 8);
  lv_obj_set_pos(other_panel, 0, 0);
  lv_obj_set_style_bg_opa(other_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(other_panel, 0, 0);
  lv_obj_t *other_lbl = lv_label_create(other_panel);
  lv_obj_set_style_text_font(other_lbl, &lv_font_montserrat_20, 0);
  lv_obj_set_style_text_color(other_lbl, lv_color_hex(0xffffff), 0);
  lv_obj_center(other_lbl);
  job_lbl = lv_label_create(other_panel);
  lv_obj_set_style_text_font(job_lbl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(job_lbl, lv_color_hex(0xdddddd), 0);
  lv_label_set_long_mode(job_lbl, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(job_lbl, UI_H_RES - 40);
  lv_obj_align(job_lbl, LV_ALIGN_CENTER, 0, 0);
  lv_label_set_text(job_lbl, "");
  lv_obj_add_flag(other_panel, LV_OBJ_FLAG_HIDDEN);

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
  static uint32_t last_ui_ms = 0;
  const uint32_t now = millis();
  if (now - last_ui_ms < 100) return;
  last_ui_ms = now;

  grbl_status_t st;
  if (!grbl_link_get_status(&st) || !st.online) {
    lv_label_set_text(state_lbl, "Offline  ·  demo  ·  mm");
    return;
  }

  for (int i = 0; i < 3; i++) {
    wpos[i] = st.wpos[i];
    wco[i] = st.wco[i];
  }
  feed_mm_min = st.feed;
  laser_s_cmd = st.laser_s_cmd;
  laser_s_act = st.laser_s_act;
  feed_ov = st.feed_ov;
  rapid_ov = st.rapid_ov;
  speed_ov = st.speed_ov;

  if (mpg_on != st.mpg_mode) {
    mpg_on = st.mpg_mode;
    refresh_mpg();
  }

  char buf[48];
  snprintf(buf, sizeof(buf), "%s  ·  Laser  ·  mm", st.state[0] ? st.state : "?");
  lv_label_set_text(state_lbl, buf);

  refresh_axes();
  refresh_telemetry();
}
