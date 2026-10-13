#include "ui_pendant.h"
#include "pins_config.h"
#include "pendant_cfg.h"
#include "gcode_path.h"
#include "sample_job_nc.h"
#include "grbl_link.h"
#include "handwheel.h"
#include "i2c_mpg_io.h"
#include "job_store.h"
#include "program_sender.h"
#include "macros.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <esp_heap_caps.h>

// MPG dashboard: G-code toolpath preview + live laser cursor + DRO/telemetry.

static lv_obj_t *scr;
static lv_obj_t *header;
static lv_obj_t *content;
static lv_obj_t *mpg_panel;
static lv_obj_t *ov_panel;
static lv_obj_t *feed_panel;
static lv_obj_t *gcode_panel;
static lv_obj_t *probe_panel;
static lv_obj_t *settings_panel;
static lv_obj_t *transport_btn;
static lv_obj_t *transport_lbl;
static lv_obj_t *other_panel;
static lv_obj_t *status_bar;
static lv_obj_t *mpg_btn;
static lv_obj_t *state_lbl;
static lv_obj_t *page_lbl;
static lv_obj_t *path_job_hint;

static lv_obj_t *gcode_list;
static lv_obj_t *gcode_status_lbl;
static lv_obj_t *gcode_run_btn;
static lv_obj_t *gcode_hold_btn;
static lv_obj_t *gcode_stop_btn;
static lv_obj_t *gcode_prog_lbl;
static job_entry_t gcode_entries[JOB_STORE_MAX_FILES];
static unsigned gcode_entry_count = 0;
static char job_store_path[JOB_STORE_PATH_LEN] = "embedded:sample";

static lv_obj_t *probe_axis_btns[3];
static lv_obj_t *probe_contact_lbl;
static lv_obj_t *probe_result_lbl;
static lv_obj_t *probe_feed_lbl;
static float probe_feed = 100.0f;
static float probe_travel = 20.0f;  // mm relative travel for toward/away
static bool probe_strict = true;

static lv_obj_t *ov_feed_val;
static lv_obj_t *ov_speed_val;
static lv_obj_t *ov_rapid_val;
static lv_obj_t *ov_flood_btn;
static lv_obj_t *ov_mist_btn;
static lv_obj_t *pf_feed_val;
static lv_obj_t *pf_axis_btns[3];
static bool coolant_flood = false;
static bool coolant_mist = false;
static float power_feed = 300.0f;  // POWER FEED jog rate mm/min

// MPG live bed map (WPos head — no toolpath)
static lv_obj_t *map_canvas;
static lv_obj_t *map_head;
static lv_obj_t *map_hint;
static lv_color_t *map_cbuf = nullptr;
static int map_w = 0, map_h = 0;
static float map_scale = 1;
static float map_pad = 10;
static float bed_x_mm = 400, bed_y_mm = 300;

// GCODE tab toolpath preview
static lv_obj_t *path_canvas;
static lv_obj_t *path_head;
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
static lv_obj_t *start_btn;
static lv_obj_t *laser_test_btn;
static lv_obj_t *laser_test_lbl;
static lv_obj_t *macro_btns[MACRO_MAX];
static lv_obj_t *feed_laser_on_btn;
static lv_obj_t *feed_laser_off_btn;
static lv_obj_t *pf_laser_val;
static bool laser_k_on = false;  // PSU K+ enable latched
static float laser_feed_pct = 5.0f;  // percent, 0.1% steps → S via $30 (from settings.cfg)

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
static float laser_s_max = 1000.0f;  // S max → 100% (grblHAL $30 / settings.cfg)
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

static void path_map_xy(float x, float y, lv_coord_t *px, lv_coord_t *py) {
  // Machine Y up → canvas Y down
  *px = (lv_coord_t)((x - path_min_x) * path_scale);
  *py = (lv_coord_t)(path_h - (y - path_min_y) * path_scale);
}

static void draw_toolpath();
static void draw_live_map();
static void update_heads();
static float map_ox = 0, map_oy = 0, map_used_h = 0;

static bool ensure_job_path_buf() {
  if (job_path) return true;
  job_path = (path_seg_t *)heap_caps_malloc(sizeof(path_seg_t) * GCODE_PATH_MAX_SEGS,
                                            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!job_path) {
    job_path = (path_seg_t *)malloc(sizeof(path_seg_t) * GCODE_PATH_MAX_SEGS);
  }
  if (!job_path) {
    job_path_len = 0;
    Serial.println("[gcode] path buffer alloc failed");
    return false;
  }
  return true;
}

static void recompute_path_bounds(int w, int h) {
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
}

static void refresh_path_job_hint() {
  if (!path_job_hint) return;
  char hint_buf[64];
  snprintf(hint_buf, sizeof(hint_buf), "Job · %s", job_name);
  lv_label_set_text(path_job_hint, hint_buf);
}

static void apply_loaded_job() {
  if (path_w > 0 && path_h > 0) recompute_path_bounds(path_w, path_h);
  draw_toolpath();
  refresh_path_job_hint();
  update_heads();
}

static void refresh_prog_ui(void);

static void load_sample_job() {
  if (!ensure_job_path_buf()) return;
  job_path_len = gcode_parse_toolpath(SAMPLE_JOB_NC, job_path, GCODE_PATH_MAX_SEGS);
  strncpy(job_name, SAMPLE_JOB_NAME, sizeof(job_name) - 1);
  job_name[sizeof(job_name) - 1] = '\0';
  strncpy(job_store_path, "embedded:sample", sizeof(job_store_path) - 1);
  job_store_path[sizeof(job_store_path) - 1] = '\0';
  (void)program_sender_load(job_store_path);
  Serial.printf("[gcode] loaded %s → %u segs\n", job_name, job_path_len);
}

static bool load_job_from_store(const char *path) {
  if (!ensure_job_path_buf()) return false;
  unsigned n =
      job_store_load_path(path, job_path, GCODE_PATH_MAX_SEGS, job_name, sizeof(job_name));
  job_path_len = n;
  if (n == 0) return false;
  strncpy(job_store_path, path, sizeof(job_store_path) - 1);
  job_store_path[sizeof(job_store_path) - 1] = '\0';
  (void)program_sender_load(path);
  apply_loaded_job();
  refresh_prog_ui();
  return true;
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
    path_map_xy(job_path[i - 1].x, job_path[i - 1].y, &pts[0].x, &pts[0].y);
    path_map_xy(job_path[i].x, job_path[i].y, &pts[1].x, &pts[1].y);
    lv_canvas_draw_line(path_canvas, pts, 2, job_path[i].cut ? &cut_dsc : &rapid_dsc);
  }
}

static void draw_live_map() {
  if (!map_canvas || !map_cbuf || map_w < 8 || map_h < 8) return;
  lv_canvas_fill_bg(map_canvas, lv_color_hex(0x141414), LV_OPA_COVER);

  if (bed_x_mm < 1) bed_x_mm = 1;
  if (bed_y_mm < 1) bed_y_mm = 1;
  const float pad = 12.0f;
  float sx = ((float)map_w - 2 * pad) / bed_x_mm;
  float sy = ((float)map_h - 2 * pad) / bed_y_mm;
  map_scale = sx < sy ? sx : sy;

  float used_w = bed_x_mm * map_scale;
  float used_h = bed_y_mm * map_scale;
  map_ox = ((float)map_w - used_w) * 0.5f;
  map_oy = ((float)map_h - used_h) * 0.5f;
  map_used_h = used_h;
  map_pad = pad;

  lv_draw_rect_dsc_t bed;
  lv_draw_rect_dsc_init(&bed);
  bed.bg_opa = LV_OPA_10;
  bed.bg_color = lv_color_hex(0x1e293b);
  bed.border_color = lv_color_hex(0x475569);
  bed.border_width = 2;
  bed.border_opa = LV_OPA_COVER;
  lv_canvas_draw_rect(map_canvas, (lv_coord_t)map_ox, (lv_coord_t)map_oy, (lv_coord_t)used_w,
                      (lv_coord_t)used_h, &bed);

  lv_draw_line_dsc_t grid;
  lv_draw_line_dsc_init(&grid);
  grid.color = lv_color_hex(0x2a2a2a);
  grid.width = 1;

  float step = (bed_x_mm <= 200 || bed_y_mm <= 200) ? 25.0f : 50.0f;
  for (float gx = 0; gx <= bed_x_mm + 0.1f; gx += step) {
    lv_point_t pts[2];
    pts[0].x = (lv_coord_t)(map_ox + gx * map_scale);
    pts[0].y = (lv_coord_t)map_oy;
    pts[1].x = pts[0].x;
    pts[1].y = (lv_coord_t)(map_oy + used_h);
    lv_canvas_draw_line(map_canvas, pts, 2, &grid);
  }
  for (float gy = 0; gy <= bed_y_mm + 0.1f; gy += step) {
    lv_point_t pts[2];
    pts[0].x = (lv_coord_t)map_ox;
    pts[0].y = (lv_coord_t)(map_oy + used_h - gy * map_scale);
    pts[1].x = (lv_coord_t)(map_ox + used_w);
    pts[1].y = pts[0].y;
    lv_canvas_draw_line(map_canvas, pts, 2, &grid);
  }

  lv_draw_line_dsc_t axes;
  lv_draw_line_dsc_init(&axes);
  axes.color = lv_color_hex(0x64748b);
  axes.width = 2;
  {
    lv_point_t hx[2] = {{(lv_coord_t)map_ox, (lv_coord_t)(map_oy + used_h)},
                        {(lv_coord_t)(map_ox + 18), (lv_coord_t)(map_oy + used_h)}};
    lv_point_t vy[2] = {{(lv_coord_t)map_ox, (lv_coord_t)(map_oy + used_h)},
                        {(lv_coord_t)map_ox, (lv_coord_t)(map_oy + used_h - 18)}};
    lv_canvas_draw_line(map_canvas, hx, 2, &axes);
    lv_canvas_draw_line(map_canvas, vy, 2, &axes);
  }
}

static void update_heads() {
  lv_color_t col = laser_s_cmd > 0 ? lv_color_hex(0xef4444) : lv_color_hex(0x38bdf8);
  if (map_head && map_scale > 0) {
    lv_coord_t px = (lv_coord_t)(map_ox + wpos[0] * map_scale);
    lv_coord_t py = (lv_coord_t)(map_oy + map_used_h - wpos[1] * map_scale);
    if (px < 0) px = 0;
    if (py < 0) py = 0;
    if (px > map_w) px = (lv_coord_t)map_w;
    if (py > map_h) py = (lv_coord_t)map_h;
    lv_obj_set_pos(map_head, px - 7, py - 7);
    lv_obj_set_style_bg_color(map_head, col, 0);
  }
  if (path_head && path_scale > 0) {
    lv_coord_t px, py;
    path_map_xy(wpos[0], wpos[1], &px, &py);
    lv_obj_set_pos(path_head, px - 6, py - 6);
    lv_obj_set_style_bg_color(path_head, col, 0);
  }
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
  update_heads();
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
  update_heads();
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

static int clamp_ov(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static void refresh_override_ui() {
  char buf[16];
  if (ov_feed_val) {
    snprintf(buf, sizeof(buf), "%d%%", feed_ov);
    lv_label_set_text(ov_feed_val, buf);
  }
  if (ov_speed_val) {
    snprintf(buf, sizeof(buf), "%d%%", speed_ov);
    lv_label_set_text(ov_speed_val, buf);
  }
  if (ov_rapid_val) {
    snprintf(buf, sizeof(buf), "%d%%", rapid_ov);
    lv_label_set_text(ov_rapid_val, buf);
  }
  if (ov_flood_btn) {
    lv_obj_set_style_bg_color(ov_flood_btn,
                              coolant_flood ? lv_color_hex(0x059669) : lv_color_hex(0x2f2f2f), 0);
  }
  if (ov_mist_btn) {
    lv_obj_set_style_bg_color(ov_mist_btn,
                              coolant_mist ? lv_color_hex(0x059669) : lv_color_hex(0x2f2f2f), 0);
  }
}

static void refresh_laser_enable_ui(void);

static void refresh_power_feed_ui() {
  char buf[24];
  if (pf_feed_val) {
    snprintf(buf, sizeof(buf), "%0.0f", power_feed);
    lv_label_set_text(pf_feed_val, buf);
  }
  for (int i = 0; i < 3; i++) {
    if (pf_axis_btns[i]) style_chrome_btn(pf_axis_btns[i], i == active_axis);
  }
  refresh_laser_enable_ui();
}

static void on_start(lv_event_t *e) {
  (void)e;
  if (grbl_link_online()) {
    grbl_link_cycle_start();
    Serial.println("[grbl] cycle start");
  } else {
    lv_label_set_text(state_lbl, "Start  ·  demo (no link)");
  }
}

static void refresh_laser_enable_ui() {
  if (laser_test_btn) {
    if (laser_k_on) {
      lv_obj_set_style_bg_color(laser_test_btn, lv_color_hex(0xea580c), 0);
      lv_obj_set_style_border_color(laser_test_btn, lv_color_hex(0xfbbf24), 0);
      lv_obj_set_style_border_width(laser_test_btn, 2, 0);
      if (laser_test_lbl) lv_label_set_text(laser_test_lbl, "LASER TEST · ON");
    } else {
      lv_obj_set_style_bg_color(laser_test_btn, lv_color_hex(0x7c2d12), 0);
      lv_obj_set_style_border_color(laser_test_btn, lv_color_hex(0x9a3412), 0);
      lv_obj_set_style_border_width(laser_test_btn, 1, 0);
      if (laser_test_lbl) lv_label_set_text(laser_test_lbl, "LASER TEST");
    }
  }
  if (feed_laser_on_btn) {
    lv_obj_set_style_bg_color(
        feed_laser_on_btn, laser_k_on ? lv_color_hex(0xea580c) : lv_color_hex(0x2f2f2f), 0);
    lv_obj_set_style_border_color(
        feed_laser_on_btn, laser_k_on ? lv_color_hex(0xfbbf24) : lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(feed_laser_on_btn, laser_k_on ? 2 : 1, 0);
  }
  if (feed_laser_off_btn) {
    style_chrome_btn(feed_laser_off_btn, !laser_k_on);
  }
  if (pf_laser_val) {
    char buf[24];
    snprintf(buf, sizeof(buf), "%0.1f%%", laser_feed_pct);
    lv_label_set_text(pf_laser_val, buf);
  }
}

static float laser_pct_to_s(float pct) {
  // $30 scale: S = pct% × laser_s_max / 100 (tenths → whole S when max=1000)
  float s = pct * laser_s_max / 100.0f;
  if (s < 0) s = 0;
  if (s > laser_s_max) s = laser_s_max;
  return s;
}

static void set_laser_k(bool on, bool with_pwm) {
  if (on) {
    if (grbl_link_online()) {
      float s = laser_pct_to_s(laser_feed_pct);
      bool ok = with_pwm ? grbl_link_laser_on(s) : grbl_link_laser_enable();
      if (!ok) {
        lv_label_set_text(state_lbl, "Laser blocked (Idle+MPG?)");
        return;
      }
      char buf[64];
      if (with_pwm) {
        snprintf(buf, sizeof(buf), "Laser ON  ·  K+  %0.1f%%  S%.0f", laser_feed_pct, s);
      } else {
        snprintf(buf, sizeof(buf), "Laser TEST ON  ·  K+ M64P%d",
                 pendant_cfg_get()->laser_enable_dout);
      }
      lv_label_set_text(state_lbl, buf);
    } else {
      lv_label_set_text(state_lbl, with_pwm ? "Laser ON  ·  demo" : "Laser TEST ON  ·  demo");
    }
    laser_k_on = true;
  } else {
    if (grbl_link_online()) {
      grbl_link_laser_off();
      char buf[48];
      snprintf(buf, sizeof(buf), "Laser OFF  ·  M65P%d", pendant_cfg_get()->laser_enable_dout);
      lv_label_set_text(state_lbl, buf);
    } else {
      lv_label_set_text(state_lbl, "Laser OFF  ·  demo");
    }
    laser_k_on = false;
  }
  refresh_laser_enable_ui();
}

static void on_laser_test(lv_event_t *e) {
  (void)e;
  // Toggle K+ enable only (no PWM) for beam / wiring check
  set_laser_k(!laser_k_on, false);
}

static bool macro_ignore_click = false;

static void refresh_macro_btns(void) {
  int teach = macros_teach_slot();
  for (unsigned i = 0; i < MACRO_MAX; i++) {
    if (!macro_btns[i]) continue;
    const macro_t *m = macros_get(i);
    lv_obj_t *lbl = lv_obj_get_child(macro_btns[i], 0);
    if (lbl && m) lv_label_set_text(lbl, m->name);
    bool taught = m && m->gcode[0];
    style_chrome_btn(macro_btns[i], (int)i == teach || taught);
    if ((int)i == teach) {
      lv_obj_set_style_bg_color(macro_btns[i], lv_color_hex(0xb45309), 0);
      lv_obj_set_style_border_color(macro_btns[i], lv_color_hex(0xfbbf24), 0);
    }
  }
}

static void on_macro_long(lv_event_t *e) {
  unsigned idx = (unsigned)(uintptr_t)lv_event_get_user_data(e);
  if (!macros_teach_begin(idx)) {
    if (state_lbl) lv_label_set_text(state_lbl, "Can't teach this macro");
    return;
  }
  macro_ignore_click = true;  // swallow the release click after long-press
  const macro_t *m = macros_get(idx);
  if (state_lbl) {
    char buf[64];
    snprintf(buf, sizeof(buf), "Teach %s — jog, then tap", m ? m->name : "?");
    lv_label_set_text(state_lbl, buf);
  }
  refresh_macro_btns();
}

static void on_macro(lv_event_t *e) {
  unsigned idx = (unsigned)(uintptr_t)lv_event_get_user_data(e);
  if (macro_ignore_click) {
    macro_ignore_click = false;
    return;
  }

  int teach = macros_teach_slot();
  if (teach >= 0) {
    if ((unsigned)teach == idx) {
      if (macros_teach_capture(wpos[0], wpos[1], wpos[2])) {
        const macro_t *m = macros_get(idx);
        if (state_lbl) {
          char buf[72];
          snprintf(buf, sizeof(buf), "Saved %s  %.2f, %.2f, %.2f", m ? m->name : "?", wpos[0],
                   wpos[1], wpos[2]);
          lv_label_set_text(state_lbl, buf);
        }
      } else if (state_lbl) {
        lv_label_set_text(state_lbl, "Teach save failed");
      }
    } else {
      macros_teach_cancel();
      if (state_lbl) lv_label_set_text(state_lbl, "Teach cancelled");
    }
    refresh_macro_btns();
    return;
  }

  const macro_t *m = macros_get(idx);
  if (m && !m->gcode[0]) {
    if (state_lbl) lv_label_set_text(state_lbl, "Empty — long-press to teach");
    return;
  }
  if (!macros_run(idx)) {
    if (state_lbl) lv_label_set_text(state_lbl, "Macro blocked (Idle+MPG?)");
  } else if (state_lbl && m) {
    char buf[48];
    snprintf(buf, sizeof(buf), "Macro · %s", m->name);
    lv_label_set_text(state_lbl, buf);
  }
}

static void on_feed_laser_on(lv_event_t *e) {
  (void)e;
  set_laser_k(true, true);  // K+ + M3 S from FEED %
}

static void on_feed_laser_off(lv_event_t *e) {
  (void)e;
  set_laser_k(false, false);
}

static void on_pf_laser_bump(lv_event_t *e) {
  // user_data is tenths of a percent (e.g. −1 = −0.1%, +10 = +1.0%)
  int tenths = (int)(intptr_t)lv_event_get_user_data(e);
  laser_feed_pct += (float)tenths / 10.0f;
  if (laser_feed_pct < 0.1f) laser_feed_pct = 0.1f;
  if (laser_feed_pct > 100.0f) laser_feed_pct = 100.0f;
  // Snap to 0.1%
  laser_feed_pct = (float)((int)(laser_feed_pct * 10.0f + 0.5f)) / 10.0f;
  refresh_laser_enable_ui();
}

static void refresh_soft_labels() {
  const char *a = "Back";
  const char *b = "";
  const char *c = "";
  if (page_idx == 0) {
    a = "Hold";
    b = "- Jog";
    c = "+ Jog";
  } else if (page_idx == 1) {
    a = "Back";
    b = "F −10";
    c = "F +10";
  } else if (page_idx == 2) {
    a = "Stop";
    b = "− Feed";
    c = "+ Feed";
  } else if (page_idx == 3) {
    a = "Back";
    b = "Refresh";
    c = "Sample";
  } else if (page_idx == 4) {
    a = "Back";
    b = "Probe −";
    c = "Probe +";
  } else {
    a = "Back";
    b = "";
    c = "";
  }
  const char *labs[] = {a, b, c};
  for (int i = 0; i < 3; i++) {
    if (!soft_btns[i]) continue;
    lv_obj_t *lbl = lv_obj_get_child(soft_btns[i], 0);
    if (lbl) lv_label_set_text(lbl, labs[i]);
    if (labs[i][0] == '\0') lv_obj_add_flag(soft_btns[i], LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(soft_btns[i], LV_OBJ_FLAG_HIDDEN);
  }
}

static void apply_override(grbl_ov_t cmd) {
  if (grbl_link_online()) {
    grbl_link_override(cmd);
    return;
  }
  // Offline demo: mirror typical grblHAL step sizes locally
  switch (cmd) {
    case GRBL_OV_FEED_RESET:
      feed_ov = 100;
      break;
    case GRBL_OV_FEED_COARSE_PLUS:
      feed_ov = clamp_ov(feed_ov + 10, 10, 200);
      break;
    case GRBL_OV_FEED_COARSE_MINUS:
      feed_ov = clamp_ov(feed_ov - 10, 10, 200);
      break;
    case GRBL_OV_FEED_FINE_PLUS:
      feed_ov = clamp_ov(feed_ov + 1, 10, 200);
      break;
    case GRBL_OV_FEED_FINE_MINUS:
      feed_ov = clamp_ov(feed_ov - 1, 10, 200);
      break;
    case GRBL_OV_RAPID_RESET:
      rapid_ov = 100;
      break;
    case GRBL_OV_RAPID_MEDIUM:
      rapid_ov = 50;
      break;
    case GRBL_OV_RAPID_LOW:
      rapid_ov = 25;
      break;
    case GRBL_OV_SPINDLE_RESET:
      speed_ov = 100;
      break;
    case GRBL_OV_SPINDLE_COARSE_PLUS:
      speed_ov = clamp_ov(speed_ov + 10, 10, 200);
      break;
    case GRBL_OV_SPINDLE_COARSE_MINUS:
      speed_ov = clamp_ov(speed_ov - 10, 10, 200);
      break;
    case GRBL_OV_SPINDLE_FINE_PLUS:
      speed_ov = clamp_ov(speed_ov + 1, 10, 200);
      break;
    case GRBL_OV_SPINDLE_FINE_MINUS:
      speed_ov = clamp_ov(speed_ov - 1, 10, 200);
      break;
    case GRBL_OV_COOLANT_FLOOD:
      coolant_flood = !coolant_flood;
      break;
    case GRBL_OV_COOLANT_MIST:
      coolant_mist = !coolant_mist;
      break;
  }
  refresh_override_ui();
  refresh_telemetry();
}

static void on_ov_btn(lv_event_t *e) {
  apply_override((grbl_ov_t)(intptr_t)lv_event_get_user_data(e));
}

static void on_pf_axis(lv_event_t *e) {
  active_axis = (int)(intptr_t)lv_event_get_user_data(e);
  refresh_power_feed_ui();
  refresh_axes();
}

static void on_pf_feed_bump(lv_event_t *e) {
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  power_feed += (float)d;
  if (power_feed < 10) power_feed = 10;
  if (power_feed > 10000) power_feed = 10000;
  refresh_power_feed_ui();
}

static void refresh_transport_ui() {
  if (!transport_lbl) return;
  lv_label_set_text(transport_lbl, grbl_link_framed() ? "Framed" : "Plain");
  if (transport_btn) {
    lv_obj_set_style_bg_color(transport_btn,
                              grbl_link_framed() ? lv_color_hex(0x3d5a80) : lv_color_hex(0x2f2f2f),
                              0);
  }
}

static void on_transport_toggle(lv_event_t *e) {
  (void)e;
  grbl_link_set_framed(!grbl_link_framed());
  refresh_transport_ui();
}

static void refresh_gcode_list();
static void refresh_probe_ui();

static void show_page(int idx) {
  page_idx = idx;
  lv_label_set_text(page_lbl, k_pages[idx]);
  lv_obj_add_flag(mpg_panel, LV_OBJ_FLAG_HIDDEN);
  if (ov_panel) lv_obj_add_flag(ov_panel, LV_OBJ_FLAG_HIDDEN);
  if (feed_panel) lv_obj_add_flag(feed_panel, LV_OBJ_FLAG_HIDDEN);
  if (gcode_panel) lv_obj_add_flag(gcode_panel, LV_OBJ_FLAG_HIDDEN);
  if (probe_panel) lv_obj_add_flag(probe_panel, LV_OBJ_FLAG_HIDDEN);
  if (settings_panel) lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
  if (other_panel) lv_obj_add_flag(other_panel, LV_OBJ_FLAG_HIDDEN);

  if (idx == 0) {
    lv_obj_clear_flag(mpg_panel, LV_OBJ_FLAG_HIDDEN);
  } else if (idx == 1 && ov_panel) {
    lv_obj_clear_flag(ov_panel, LV_OBJ_FLAG_HIDDEN);
    refresh_override_ui();
  } else if (idx == 2 && feed_panel) {
    lv_obj_clear_flag(feed_panel, LV_OBJ_FLAG_HIDDEN);
    refresh_power_feed_ui();
  } else if (idx == 3 && gcode_panel) {
    lv_obj_clear_flag(gcode_panel, LV_OBJ_FLAG_HIDDEN);
    refresh_gcode_list();
  } else if (idx == 4 && probe_panel) {
    lv_obj_clear_flag(probe_panel, LV_OBJ_FLAG_HIDDEN);
    refresh_probe_ui();
  } else if (idx == 5 && settings_panel) {
    lv_obj_clear_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);
    refresh_transport_ui();
  } else if (other_panel) {
    lv_obj_clear_flag(other_panel, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(lv_obj_get_child(other_panel, 0), k_pages[idx]);
  }
  refresh_soft_labels();
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

static void go_mpg_tab() {
  show_page(0);
  uint32_t n = lv_obj_get_child_cnt(header);
  for (uint32_t i = 0; i < n; i++) {
    style_chrome_btn(lv_obj_get_child(header, i), i == 0);
  }
}

static void run_probe(int dir) {
  // dir −1 / +1 along active_axis by probe_travel (G38.2/3 toward target)
  float target = wpos[active_axis] + (float)dir * probe_travel;
  if (grbl_link_online()) {
    bool ok = grbl_link_probe_toward(active_axis, target, probe_feed, probe_strict);
    if (!ok && state_lbl) lv_label_set_text(state_lbl, "Probe blocked (Idle+MPG?)");
  } else {
    wpos[active_axis] = target;
    if (probe_result_lbl) {
      char buf[96];
      snprintf(buf, sizeof(buf), "Demo PRB  M %.3f,%.3f,%.3f  ok", wpos[0] + wco[0],
               wpos[1] + wco[1], wpos[2] + wco[2]);
      lv_label_set_text(probe_result_lbl, buf);
    }
    if (probe_contact_lbl) lv_label_set_text(probe_contact_lbl, "Contact: demo");
    refresh_axes();
  }
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
  } else if (page_idx == 1) {
    if (which == 0) go_mpg_tab();
    else if (which == 1) apply_override(GRBL_OV_FEED_COARSE_MINUS);
    else if (which == 2) apply_override(GRBL_OV_FEED_COARSE_PLUS);
  } else if (page_idx == 2) {
    if (which == 0) {
      if (grbl_link_online()) grbl_link_jog_cancel();
      else go_mpg_tab();
      return;
    }
    float delta = (which == 1) ? -5.0f : 5.0f;
    if (grbl_link_online()) {
      grbl_link_jog(active_axis, delta, power_feed);
    } else {
      wpos[active_axis] += delta;
      refresh_axes();
    }
  } else if (page_idx == 3) {
    if (which == 0) go_mpg_tab();
    else if (which == 1) refresh_gcode_list();
    else if (which == 2) {
      load_sample_job();
      apply_loaded_job();
      if (gcode_status_lbl) {
        char buf[80];
        snprintf(buf, sizeof(buf), "Loaded %s · %u segs", job_name, job_path_len);
        lv_label_set_text(gcode_status_lbl, buf);
      }
    }
  } else if (page_idx == 4) {
    if (which == 0) go_mpg_tab();
    else if (which == 1) run_probe(-1);
    else if (which == 2) run_probe(+1);
  } else if (which == 0) {
    go_mpg_tab();
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

static void refresh_prog_ui(void) {
  if (gcode_prog_lbl) {
    char buf[128];
    uint32_t sent = program_sender_sent_lines();
    uint32_t total = program_sender_total_lines();
    if (total > 0) {
      snprintf(buf, sizeof(buf), "%s\n%lu / %lu lines", program_sender_message(),
               (unsigned long)sent, (unsigned long)total);
    } else {
      snprintf(buf, sizeof(buf), "%s\n%lu lines sent", program_sender_message(),
               (unsigned long)sent);
    }
    lv_label_set_text(gcode_prog_lbl, buf);
  }
  prog_state_t st = program_sender_state();
  bool active = (st == PROG_RUNNING || st == PROG_HOLD || st == PROG_DRAINING);
  if (gcode_run_btn) style_chrome_btn(gcode_run_btn, st == PROG_RUNNING || st == PROG_DRAINING);
  if (gcode_hold_btn) style_chrome_btn(gcode_hold_btn, st == PROG_HOLD);
  if (gcode_stop_btn) style_chrome_btn(gcode_stop_btn, st == PROG_ERROR);
  (void)active;
}

static void on_gcode_run(lv_event_t *e) {
  (void)e;
  prog_state_t st = program_sender_state();
  if (st == PROG_HOLD) {
    program_sender_resume();
  } else {
    if (!program_sender_start()) {
      // message already set
    }
  }
  refresh_prog_ui();
}

static void on_gcode_hold(lv_event_t *e) {
  (void)e;
  program_sender_hold();
  refresh_prog_ui();
}

static void on_gcode_stop(lv_event_t *e) {
  (void)e;
  program_sender_stop();
  refresh_prog_ui();
}

static void on_gcode_item(lv_event_t *e) {
  unsigned idx = (unsigned)(uintptr_t)lv_event_get_user_data(e);
  if (idx >= gcode_entry_count) return;
  const job_entry_t *ent = &gcode_entries[idx];
  bool ok = load_job_from_store(ent->path);
  if (gcode_status_lbl) {
    char buf[96];
    if (ok) {
      const char *src = ent->src == JOB_SRC_SD         ? "SD"
                        : ent->src == JOB_SRC_LITTLEFS ? "LFS"
                                                       : "ROM";
      snprintf(buf, sizeof(buf), "Loaded %s (%s) · %u segs", job_name, src, job_path_len);
    } else {
      snprintf(buf, sizeof(buf), "Failed: %s", ent->name);
    }
    lv_label_set_text(gcode_status_lbl, buf);
  }
  refresh_prog_ui();
}

static void refresh_gcode_list() {
  if (!gcode_list) return;
  lv_obj_clean(gcode_list);
  gcode_entry_count = job_store_list(gcode_entries, JOB_STORE_MAX_FILES);

  for (unsigned i = 0; i < gcode_entry_count; i++) {
    const job_entry_t *ent = &gcode_entries[i];
    lv_obj_t *btn = lv_btn_create(gcode_list);
    lv_obj_set_width(btn, LV_PCT(100));
    lv_obj_set_height(btn, 44);
    style_chrome_btn(btn, strcmp(ent->name, job_name) == 0);
    lv_obj_t *lbl = lv_label_create(btn);
    char line[80];
    const char *src = ent->src == JOB_SRC_SD         ? "SD"
                      : ent->src == JOB_SRC_LITTLEFS ? "LFS"
                                                     : "ROM";
    snprintf(line, sizeof(line), "%s  ·  %s  ·  %u B", ent->name, src, (unsigned)ent->size);
    lv_label_set_text(lbl, line);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 8, 0);
    lv_obj_add_event_cb(btn, on_gcode_item, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
  }

  if (gcode_status_lbl) {
    char buf[96];
    snprintf(buf, sizeof(buf), "Active: %s · %u segs · LFS %s · SD %s", job_name, job_path_len,
             job_store_lfs_ok() ? "ok" : "—", job_store_sd_ok() ? "ok" : "—");
    lv_label_set_text(gcode_status_lbl, buf);
  }
}

static void refresh_probe_ui() {
  for (int i = 0; i < 3; i++) {
    if (probe_axis_btns[i]) style_chrome_btn(probe_axis_btns[i], i == active_axis);
  }
  if (probe_feed_lbl) {
    char buf[48];
    snprintf(buf, sizeof(buf), "F %0.0f   travel %0.0f mm   %s", probe_feed, probe_travel,
             probe_strict ? "G38.2" : "G38.3");
    lv_label_set_text(probe_feed_lbl, buf);
  }
}

static void on_probe_axis(lv_event_t *e) {
  active_axis = (int)(intptr_t)lv_event_get_user_data(e);
  refresh_probe_ui();
  refresh_axes();
}

static void on_probe_feed_bump(lv_event_t *e) {
  int d = (int)(intptr_t)lv_event_get_user_data(e);
  probe_feed += (float)d;
  if (probe_feed < 10) probe_feed = 10;
  if (probe_feed > 2000) probe_feed = 2000;
  refresh_probe_ui();
}

static void on_probe_strict(lv_event_t *e) {
  (void)e;
  probe_strict = !probe_strict;
  refresh_probe_ui();
}

static void on_probe_zero(lv_event_t *e) {
  (void)e;
  // Zero WPos of active axis to last probe machine coord (G10 L20)
  if (grbl_link_online()) {
    grbl_link_zero_axis(active_axis);
  } else if (probe_result_lbl) {
    wpos[active_axis] = 0;
    refresh_axes();
    lv_label_set_text(probe_result_lbl, "Demo: WPos zeroed on axis");
  }
}

static void setup_live_map(lv_obj_t *parent, int y, int w, int h) {
  map_w = w;
  map_h = h;
  bed_x_mm = pendant_cfg_get()->bed_x_mm;
  bed_y_mm = pendant_cfg_get()->bed_y_mm;
  if (bed_x_mm < 1) bed_x_mm = MACHINE_BED_X_MM;
  if (bed_y_mm < 1) bed_y_mm = MACHINE_BED_Y_MM;

  lv_obj_t *frame = lv_obj_create(parent);
  lv_obj_set_size(frame, w, h);
  lv_obj_set_pos(frame, 0, y);
  lv_obj_set_style_bg_color(frame, lv_color_hex(0x141414), 0);
  lv_obj_set_style_border_width(frame, 1, 0);
  lv_obj_set_style_border_color(frame, lv_color_hex(0x111111), 0);
  lv_obj_set_style_pad_all(frame, 0, 0);
  lv_obj_set_style_radius(frame, 2, 0);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

  map_hint = lv_label_create(frame);
  lv_obj_set_style_text_font(map_hint, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(map_hint, lv_color_hex(0x888888), 0);
  lv_obj_set_pos(map_hint, 6, 4);
  char hint[48];
  snprintf(hint, sizeof(hint), "WPos map · %.0f×%.0f mm", bed_x_mm, bed_y_mm);
  lv_label_set_text(map_hint, hint);

  size_t buf_sz = LV_CANVAS_BUF_SIZE_TRUE_COLOR(w, h);
  map_cbuf = (lv_color_t *)heap_caps_malloc(buf_sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!map_cbuf) map_cbuf = (lv_color_t *)heap_caps_malloc(buf_sz, MALLOC_CAP_8BIT);
  if (!map_cbuf) {
    Serial.println("[ui] map canvas alloc failed");
    return;
  }

  map_canvas = lv_canvas_create(frame);
  lv_obj_set_size(map_canvas, w, h);
  lv_obj_set_pos(map_canvas, 0, 0);
  lv_canvas_set_buffer(map_canvas, map_cbuf, w, h, LV_IMG_CF_TRUE_COLOR);
  draw_live_map();

  map_head = lv_obj_create(frame);
  lv_obj_set_size(map_head, 14, 14);
  lv_obj_set_style_radius(map_head, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(map_head, lv_color_hex(0x38bdf8), 0);
  lv_obj_set_style_border_width(map_head, 2, 0);
  lv_obj_set_style_border_color(map_head, lv_color_hex(0xffffff), 0);
  lv_obj_clear_flag(map_head, LV_OBJ_FLAG_SCROLLABLE);
  update_heads();
}

static void setup_path_preview(lv_obj_t *parent, int y, int w, int h) {
  path_w = w;
  path_h = h;

  if (job_path_len == 0) load_sample_job();
  recompute_path_bounds(w, h);

  lv_obj_t *frame = lv_obj_create(parent);
  lv_obj_set_size(frame, w, h);
  lv_obj_set_pos(frame, 0, y);
  lv_obj_set_style_bg_color(frame, lv_color_hex(0x1a1a1a), 0);
  lv_obj_set_style_border_width(frame, 1, 0);
  lv_obj_set_style_border_color(frame, lv_color_hex(0x111111), 0);
  lv_obj_set_style_pad_all(frame, 0, 0);
  lv_obj_set_style_radius(frame, 2, 0);
  lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

  path_job_hint = lv_label_create(frame);
  lv_obj_set_style_text_font(path_job_hint, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(path_job_hint, lv_color_hex(0x666666), 0);
  lv_obj_set_pos(path_job_hint, 6, 4);
  refresh_path_job_hint();

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

  path_head = lv_obj_create(frame);
  lv_obj_set_size(path_head, 12, 12);
  lv_obj_set_style_radius(path_head, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(path_head, lv_color_hex(0x38bdf8), 0);
  lv_obj_set_style_border_width(path_head, 2, 0);
  lv_obj_set_style_border_color(path_head, lv_color_hex(0xffffff), 0);
  lv_obj_clear_flag(path_head, LV_OBJ_FLAG_SCROLLABLE);
  update_heads();
}

void ui_pendant_create() {
  const pendant_cfg_t *cfg = pendant_cfg_get();
  laser_feed_pct = cfg->laser_feed_pct;
  laser_s_max = cfg->laser_s_max > 0.0f ? cfg->laser_s_max : 1000.0f;
  bed_x_mm = cfg->bed_x_mm > 0 ? cfg->bed_x_mm : MACHINE_BED_X_MM;
  bed_y_mm = cfg->bed_y_mm > 0 ? cfg->bed_y_mm : MACHINE_BED_Y_MM;
  load_sample_job();

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

  // State + MPG claim sit under the tabs (not above the soft keys)
  status_bar = lv_obj_create(scr);
  lv_obj_set_size(status_bar, UI_H_RES, status_h);
  lv_obj_set_pos(status_bar, 0, SCREEN_HEADER_H);
  lv_obj_set_style_bg_color(status_bar, lv_color_hex(0x555555), 0);
  lv_obj_set_style_border_width(status_bar, 0, 0);
  lv_obj_set_style_pad_all(status_bar, 4, 0);
  lv_obj_set_style_radius(status_bar, 0, 0);

  state_lbl = lv_label_create(status_bar);
  lv_label_set_text(state_lbl, "Idle  ·  Laser  ·  mm");
  lv_obj_set_style_text_color(state_lbl, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_text_font(state_lbl, &lv_font_montserrat_14, 0);
  lv_obj_align(state_lbl, LV_ALIGN_LEFT_MID, 4, 0);

  mpg_btn = lv_btn_create(status_bar);
  lv_obj_set_size(mpg_btn, 72, status_h - 4);
  lv_obj_align(mpg_btn, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_t *mpg_lbl = lv_label_create(mpg_btn);
  lv_label_set_text(mpg_lbl, "MPG");
  lv_obj_center(mpg_lbl);
  lv_obj_add_event_cb(mpg_btn, on_mpg, LV_EVENT_CLICKED, NULL);
  refresh_mpg();

  content = lv_obj_create(scr);
  lv_obj_set_size(content, UI_H_RES, content_h);
  lv_obj_set_pos(content, 0, SCREEN_HEADER_H + status_h);
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

  // Steps sit just above the global Hold / ±Jog soft row; two macro rows above steps
  const int step_h = 44;
  const int macro_h = 44;
  const int macro_rows = (MACRO_MAX + MACRO_COLS - 1) / MACRO_COLS;
  const int macro_gap = 3;
  const int step_y = (content_h - 8) - step_h;
  const int macro_block_h = macro_rows * macro_h + (macro_rows - 1) * macro_gap;
  const int macro_y0 = step_y - macro_block_h - 4;
  const int map_h_px = 240;
  setup_live_map(mpg_panel, 0, UI_H_RES - 12, map_h_px);

  // Denser 3-column DRO under the live map
  const int dro_y = map_h_px + 6;
  const int row_h = 72;
  const int col_w = (UI_H_RES - 12 - 8) / 3;
  for (int i = 0; i < 3; i++) {
    axis_rows[i] = lv_btn_create(mpg_panel);
    lv_obj_set_size(axis_rows[i], col_w, row_h);
    lv_obj_set_pos(axis_rows[i], i * (col_w + 4), dro_y);
    style_chrome_btn(axis_rows[i], i == 0);
    lv_obj_add_event_cb(axis_rows[i], on_axis, LV_EVENT_CLICKED, (void *)(intptr_t)i);

    lv_obj_t *name = lv_label_create(axis_rows[i]);
    lv_label_set_text(name, k_axis[i]);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(0xf0c040), 0);
    lv_obj_align(name, LV_ALIGN_TOP_LEFT, 4, 4);

    axis_wpos[i] = lv_label_create(axis_rows[i]);
    lv_obj_set_style_text_font(axis_wpos[i], &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(axis_wpos[i], lv_color_hex(0xffffff), 0);
    lv_obj_align(axis_wpos[i], LV_ALIGN_TOP_LEFT, 28, 4);

    axis_mpos[i] = lv_label_create(axis_rows[i]);
    lv_obj_set_style_text_font(axis_mpos[i], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(axis_mpos[i], lv_color_hex(0xa0c4e8), 0);
    lv_obj_align(axis_mpos[i], LV_ALIGN_BOTTOM_LEFT, 4, -4);

    lv_obj_t *zero = lv_btn_create(axis_rows[i]);
    lv_obj_set_size(zero, 40, 26);
    lv_obj_align(zero, LV_ALIGN_BOTTOM_RIGHT, -2, -2);
    style_chrome_btn(zero, false);
    lv_obj_t *zl = lv_label_create(zero);
    lv_label_set_text(zl, "0");
    lv_obj_set_style_text_font(zl, &lv_font_montserrat_14, 0);
    lv_obj_center(zl);
    lv_obj_add_event_cb(zero, on_zero, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  const int tele_y = dro_y + row_h + 6;
  lv_obj_t *tele = lv_obj_create(mpg_panel);
  lv_obj_set_size(tele, UI_H_RES - 12, 64);
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

  // Confirm / run — cycle start after job is loaded on the controller
  const int start_y = tele_y + 70;
  start_btn = lv_btn_create(mpg_panel);
  lv_obj_set_size(start_btn, UI_H_RES - 12, 48);
  lv_obj_set_pos(start_btn, 0, start_y);
  lv_obj_set_style_radius(start_btn, 2, 0);
  lv_obj_set_style_border_width(start_btn, 1, 0);
  lv_obj_set_style_border_color(start_btn, lv_color_hex(0x065f46), 0);
  lv_obj_set_style_bg_color(start_btn, lv_color_hex(0x059669), 0);
  lv_obj_set_style_text_color(start_btn, lv_color_hex(0xffffff), 0);
  lv_obj_t *start_lbl = lv_label_create(start_btn);
  lv_label_set_text(start_lbl, "START");
  lv_obj_set_style_text_font(start_lbl, &lv_font_montserrat_20, 0);
  lv_obj_center(start_lbl);
  lv_obj_add_event_cb(start_btn, on_start, LV_EVENT_CLICKED, NULL);

  // Laser TEST toggle — K+ enable only (PWM/power lives on FEED tab)
  const int laser_y = start_y + 52;
  laser_test_btn = lv_btn_create(mpg_panel);
  lv_obj_set_size(laser_test_btn, UI_H_RES - 12, 44);
  lv_obj_set_pos(laser_test_btn, 0, laser_y);
  lv_obj_set_style_radius(laser_test_btn, 2, 0);
  lv_obj_set_style_text_color(laser_test_btn, lv_color_hex(0xffffff), 0);
  laser_test_lbl = lv_label_create(laser_test_btn);
  lv_obj_set_style_text_font(laser_test_lbl, &lv_font_montserrat_14, 0);
  lv_obj_center(laser_test_lbl);
  lv_obj_add_event_cb(laser_test_btn, on_laser_test, LV_EVENT_CLICKED, NULL);
  refresh_laser_enable_ui();

  // Soft macros from /macros.cfg — long-press to teach WPos, tap to run (2×4)
  {
    int mbw = (UI_H_RES - 12 - (MACRO_COLS - 1) * 3) / MACRO_COLS;
    for (unsigned i = 0; i < MACRO_MAX; i++) {
      unsigned row = i / MACRO_COLS;
      unsigned col = i % MACRO_COLS;
      const macro_t *m = macros_get(i);
      macro_btns[i] = lv_btn_create(mpg_panel);
      lv_obj_set_size(macro_btns[i], mbw, macro_h);
      lv_obj_set_pos(macro_btns[i], (int)col * (mbw + 3),
                     macro_y0 + (int)row * (macro_h + macro_gap));
      lv_obj_t *lbl = lv_label_create(macro_btns[i]);
      lv_label_set_text(lbl, m ? m->name : "?");
      lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
      lv_obj_center(lbl);
      lv_obj_add_event_cb(macro_btns[i], on_macro, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
      lv_obj_add_event_cb(macro_btns[i], on_macro_long, LV_EVENT_LONG_PRESSED,
                          (void *)(uintptr_t)i);
    }
    refresh_macro_btns();
  }

  for (int i = 0; i < 4; i++) {
    step_btns[i] = lv_btn_create(mpg_panel);
    int bw = (UI_H_RES - 12 - 9) / 4;
    lv_obj_set_size(step_btns[i], bw, step_h);
    lv_obj_set_pos(step_btns[i], i * (bw + 3), step_y);
    char buf[16];
    snprintf(buf, sizeof(buf), "%g", k_steps[i]);
    lv_obj_t *lbl = lv_label_create(step_btns[i]);
    lv_label_set_text(lbl, buf);
    lv_obj_center(lbl);
    style_chrome_btn(step_btns[i], i == step_idx);
    lv_obj_add_event_cb(step_btns[i], on_step, LV_EVENT_CLICKED, (void *)(intptr_t)i);
  }

  // ----- OVERRIDE panel -----
  ov_panel = lv_obj_create(content);
  lv_obj_set_size(ov_panel, UI_H_RES - 12, content_h - 8);
  lv_obj_set_pos(ov_panel, 0, 0);
  lv_obj_set_style_bg_opa(ov_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ov_panel, 0, 0);
  lv_obj_set_style_pad_all(ov_panel, 4, 0);
  lv_obj_clear_flag(ov_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ov_panel, LV_OBJ_FLAG_HIDDEN);

  auto make_ov_row = [&](lv_obj_t *parent, int y, const char *title, lv_obj_t **val_out,
                         grbl_ov_t m10, grbl_ov_t m1, grbl_ov_t reset, grbl_ov_t p1,
                         grbl_ov_t p10) {
    lv_obj_t *cap = lv_label_create(parent);
    lv_label_set_text(cap, title);
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(cap, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_pos(cap, 4, y);

    *val_out = lv_label_create(parent);
    lv_obj_set_style_text_font(*val_out, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(*val_out, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(*val_out, 4, y + 18);
    lv_label_set_text(*val_out, "100%");

    const char *labs[] = {"−10", "−1", "100", "+1", "+10"};
    const grbl_ov_t cmds[] = {m10, m1, reset, p1, p10};
    int bw = (UI_H_RES - 12 - 20) / 5;
    for (int i = 0; i < 5; i++) {
      lv_obj_t *b = lv_btn_create(parent);
      lv_obj_set_size(b, bw - 2, 40);
      lv_obj_set_pos(b, 4 + i * bw, y + 56);
      style_chrome_btn(b, i == 2);
      lv_obj_t *l = lv_label_create(b);
      lv_label_set_text(l, labs[i]);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
      lv_obj_center(l);
      lv_obj_add_event_cb(b, on_ov_btn, LV_EVENT_CLICKED, (void *)(intptr_t)cmds[i]);
    }
  };

  make_ov_row(ov_panel, 4, "FEED OVERRIDE", &ov_feed_val, GRBL_OV_FEED_COARSE_MINUS,
              GRBL_OV_FEED_FINE_MINUS, GRBL_OV_FEED_RESET, GRBL_OV_FEED_FINE_PLUS,
              GRBL_OV_FEED_COARSE_PLUS);
  make_ov_row(ov_panel, 120, "LASER / SPINDLE", &ov_speed_val, GRBL_OV_SPINDLE_COARSE_MINUS,
              GRBL_OV_SPINDLE_FINE_MINUS, GRBL_OV_SPINDLE_RESET, GRBL_OV_SPINDLE_FINE_PLUS,
              GRBL_OV_SPINDLE_COARSE_PLUS);

  {
    lv_obj_t *cap = lv_label_create(ov_panel);
    lv_label_set_text(cap, "RAPID");
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(cap, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_pos(cap, 4, 236);
    ov_rapid_val = lv_label_create(ov_panel);
    lv_obj_set_style_text_font(ov_rapid_val, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ov_rapid_val, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(ov_rapid_val, 4, 254);
    lv_label_set_text(ov_rapid_val, "100%");
    const char *rlabs[] = {"25%", "50%", "100%"};
    const grbl_ov_t rcmds[] = {GRBL_OV_RAPID_LOW, GRBL_OV_RAPID_MEDIUM, GRBL_OV_RAPID_RESET};
    int bw = (UI_H_RES - 12 - 16) / 3;
    for (int i = 0; i < 3; i++) {
      lv_obj_t *b = lv_btn_create(ov_panel);
      lv_obj_set_size(b, bw - 2, 40);
      lv_obj_set_pos(b, 4 + i * bw, 290);
      style_chrome_btn(b, i == 2);
      lv_obj_t *l = lv_label_create(b);
      lv_label_set_text(l, rlabs[i]);
      lv_obj_center(l);
      lv_obj_add_event_cb(b, on_ov_btn, LV_EVENT_CLICKED, (void *)(intptr_t)rcmds[i]);
    }
    ov_flood_btn = lv_btn_create(ov_panel);
    lv_obj_set_size(ov_flood_btn, bw - 2, 40);
    lv_obj_set_pos(ov_flood_btn, 4, 340);
    style_chrome_btn(ov_flood_btn, false);
    lv_obj_t *fl = lv_label_create(ov_flood_btn);
    lv_label_set_text(fl, "Flood");
    lv_obj_center(fl);
    lv_obj_add_event_cb(ov_flood_btn, on_ov_btn, LV_EVENT_CLICKED,
                        (void *)(intptr_t)GRBL_OV_COOLANT_FLOOD);
    ov_mist_btn = lv_btn_create(ov_panel);
    lv_obj_set_size(ov_mist_btn, bw - 2, 40);
    lv_obj_set_pos(ov_mist_btn, 4 + bw, 340);
    style_chrome_btn(ov_mist_btn, false);
    lv_obj_t *ml = lv_label_create(ov_mist_btn);
    lv_label_set_text(ml, "Mist");
    lv_obj_center(ml);
    lv_obj_add_event_cb(ov_mist_btn, on_ov_btn, LV_EVENT_CLICKED,
                        (void *)(intptr_t)GRBL_OV_COOLANT_MIST);
  }

  // ----- POWER FEED panel -----
  feed_panel = lv_obj_create(content);
  lv_obj_set_size(feed_panel, UI_H_RES - 12, content_h - 8);
  lv_obj_set_pos(feed_panel, 0, 0);
  lv_obj_set_style_bg_opa(feed_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(feed_panel, 0, 0);
  lv_obj_set_style_pad_all(feed_panel, 4, 0);
  lv_obj_clear_flag(feed_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(feed_panel, LV_OBJ_FLAG_HIDDEN);

  {
    lv_obj_t *cap = lv_label_create(feed_panel);
    lv_label_set_text(cap, "POWER FEED");
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cap, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_pos(cap, 4, 8);

    lv_obj_t *axis_cap = lv_label_create(feed_panel);
    lv_label_set_text(axis_cap, "AXIS");
    lv_obj_set_style_text_font(axis_cap, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(axis_cap, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(axis_cap, 4, 40);
    int abw = (UI_H_RES - 12 - 16) / 3;
    for (int i = 0; i < 3; i++) {
      pf_axis_btns[i] = lv_btn_create(feed_panel);
      lv_obj_set_size(pf_axis_btns[i], abw - 2, 56);
      lv_obj_set_pos(pf_axis_btns[i], 4 + i * abw, 60);
      style_chrome_btn(pf_axis_btns[i], i == active_axis);
      lv_obj_t *l = lv_label_create(pf_axis_btns[i]);
      lv_label_set_text(l, k_axis[i]);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
      lv_obj_center(l);
      lv_obj_add_event_cb(pf_axis_btns[i], on_pf_axis, LV_EVENT_CLICKED, (void *)(intptr_t)i);
    }

    lv_obj_t *fc = lv_label_create(feed_panel);
    lv_label_set_text(fc, "FEED mm/min");
    lv_obj_set_style_text_font(fc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(fc, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(fc, 4, 140);
    pf_feed_val = lv_label_create(feed_panel);
    lv_obj_set_style_text_font(pf_feed_val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(pf_feed_val, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(pf_feed_val, 4, 160);
    lv_label_set_text(pf_feed_val, "300");

    const int bumps[] = {-100, -10, 10, 100};
    const char *blabs[] = {"−100", "−10", "+10", "+100"};
    int bbw = (UI_H_RES - 12 - 20) / 4;
    for (int i = 0; i < 4; i++) {
      lv_obj_t *b = lv_btn_create(feed_panel);
      lv_obj_set_size(b, bbw - 2, 44);
      lv_obj_set_pos(b, 4 + i * bbw, 210);
      style_chrome_btn(b, false);
      lv_obj_t *l = lv_label_create(b);
      lv_label_set_text(l, blabs[i]);
      lv_obj_center(l);
      lv_obj_add_event_cb(b, on_pf_feed_bump, LV_EVENT_CLICKED, (void *)(intptr_t)bumps[i]);
    }

    lv_obj_t *hint = lv_label_create(feed_panel);
    lv_label_set_text(hint, "Soft −/+ Feed jogs 5 mm at this rate.\nStop cancels an active jog.");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(hint, 4, 270);

    // Laser power (%) + ON/OFF — K+ enable with M3 S from percent × $30
    lv_obj_t *lc = lv_label_create(feed_panel);
    lv_label_set_text(lc, "LASER POWER  %");
    lv_obj_set_style_text_font(lc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lc, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(lc, 4, 320);
    pf_laser_val = lv_label_create(feed_panel);
    lv_obj_set_style_text_font(pf_laser_val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(pf_laser_val, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(pf_laser_val, 4, 340);

    // Bumps in tenths of a percent: −10, −1, −0.1, +0.1, +1, +10
    const int lbumps[] = {-100, -10, -1, 1, 10, 100};
    const char *llabs[] = {"−10", "−1%", "−0.1", "+0.1", "+1%", "+10"};
    int lbw6 = (UI_H_RES - 12 - 24) / 6;
    for (int i = 0; i < 6; i++) {
      lv_obj_t *b = lv_btn_create(feed_panel);
      lv_obj_set_size(b, lbw6 - 2, 40);
      lv_obj_set_pos(b, 4 + i * lbw6, 386);
      style_chrome_btn(b, false);
      lv_obj_t *l = lv_label_create(b);
      lv_label_set_text(l, llabs[i]);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
      lv_obj_center(l);
      lv_obj_add_event_cb(b, on_pf_laser_bump, LV_EVENT_CLICKED, (void *)(intptr_t)lbumps[i]);
    }

    int lbw = (UI_H_RES - 12 - 16) / 2;
    feed_laser_on_btn = lv_btn_create(feed_panel);
    lv_obj_set_size(feed_laser_on_btn, lbw - 2, 48);
    lv_obj_set_pos(feed_laser_on_btn, 4, 440);
    lv_obj_set_style_radius(feed_laser_on_btn, 2, 0);
    lv_obj_set_style_text_color(feed_laser_on_btn, lv_color_hex(0xffffff), 0);
    lv_obj_t *lon = lv_label_create(feed_laser_on_btn);
    lv_label_set_text(lon, "LASER ON");
    lv_obj_set_style_text_font(lon, &lv_font_montserrat_16, 0);
    lv_obj_center(lon);
    lv_obj_add_event_cb(feed_laser_on_btn, on_feed_laser_on, LV_EVENT_CLICKED, NULL);

    feed_laser_off_btn = lv_btn_create(feed_panel);
    lv_obj_set_size(feed_laser_off_btn, lbw - 2, 48);
    lv_obj_set_pos(feed_laser_off_btn, 4 + lbw, 440);
    style_chrome_btn(feed_laser_off_btn, true);
    lv_obj_t *loff = lv_label_create(feed_laser_off_btn);
    lv_label_set_text(loff, "LASER OFF");
    lv_obj_set_style_text_font(loff, &lv_font_montserrat_16, 0);
    lv_obj_center(loff);
    lv_obj_add_event_cb(feed_laser_off_btn, on_feed_laser_off, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lh = lv_label_create(feed_panel);
    lv_label_set_long_mode(lh, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lh, UI_H_RES - 28);
    lv_label_set_text(lh, "ON = K+ (M64) + M3 from %. OFF = M65 + M5.\nMPG LASER TEST toggles K+ only.");
    lv_obj_set_style_text_font(lh, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lh, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(lh, 4, 500);
    refresh_laser_enable_ui();
  }

  // ----- SETTINGS panel -----
  settings_panel = lv_obj_create(content);
  lv_obj_set_size(settings_panel, UI_H_RES - 12, content_h - 8);
  lv_obj_set_pos(settings_panel, 0, 0);
  lv_obj_set_style_bg_opa(settings_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(settings_panel, 0, 0);
  lv_obj_set_style_pad_all(settings_panel, 8, 0);
  lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(settings_panel, LV_OBJ_FLAG_HIDDEN);

  {
    lv_obj_t *cap = lv_label_create(settings_panel);
    lv_label_set_text(cap, "SETTINGS");
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(cap, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_pos(cap, 4, 8);

    lv_obj_t *tc = lv_label_create(settings_panel);
    lv_label_set_text(tc, "UART TRANSPORT");
    lv_obj_set_style_text_font(tc, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(tc, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(tc, 4, 48);

    transport_btn = lv_btn_create(settings_panel);
    lv_obj_set_size(transport_btn, UI_H_RES - 28, 52);
    lv_obj_set_pos(transport_btn, 4, 72);
    style_chrome_btn(transport_btn, false);
    transport_lbl = lv_label_create(transport_btn);
    lv_label_set_text(transport_lbl, "Plain");
    lv_obj_set_style_text_font(transport_lbl, &lv_font_montserrat_20, 0);
    lv_obj_center(transport_lbl);
    lv_obj_add_event_cb(transport_btn, on_transport_toggle, LV_EVENT_CLICKED, NULL);

    lv_obj_t *hint = lv_label_create(settings_panel);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, UI_H_RES - 28);
    lv_label_set_text(hint,
                      "Plain: stock grblHAL ASCII @ 115200.\n"
                      "Framed: Plugin_mpg_transport (SOF/CRC/ACK).\n"
                      "Both ends must use the same mode.");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(hint, 4, 140);

    const pendant_cfg_t *c = pendant_cfg_get();

    lv_obj_t *board = lv_label_create(settings_panel);
    char bbuf[200];
    snprintf(bbuf, sizeof(bbuf),
             "BOARD  %s\n%s\nUART IO%d→TX IO%d→RX @ %d\nLaser dout P%d  $30=S%.0f\ncfg: %s",
             c->board_id, c->board_name, c->uart_tx, c->uart_rx, c->uart_baud,
             c->laser_enable_dout, c->laser_s_max, c->source);
    lv_label_set_text(board, bbuf);
    lv_obj_set_style_text_font(board, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(board, lv_color_hex(0xdddddd), 0);
    lv_obj_set_pos(board, 4, 220);

    lv_obj_t *hw = lv_label_create(settings_panel);
    char hwbuf[160];
    if (c->handwheel_a >= 0 && c->handwheel_b >= 0) {
      snprintf(hwbuf, sizeof(hwbuf), "HANDWHEEL GPIO\nA=IO%d  B=IO%d\n%s", c->handwheel_a,
               c->handwheel_b, handwheel_hw_present() ? "ISR armed" : "not armed");
    } else {
      snprintf(hwbuf, sizeof(hwbuf),
               "HANDWHEEL GPIO\nOff (handwheel.a/b −1)\nEdit SD:/settings.cfg · reboot");
    }
    lv_label_set_text(hw, hwbuf);
    lv_obj_set_style_text_font(hw, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hw, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_pos(hw, 4, 330);

    lv_obj_t *exp = lv_label_create(settings_panel);
    char expbuf[160];
    if (c->i2c_mpg_enable) {
      snprintf(expbuf, sizeof(expbuf), "I2C MPG  0x%02X\n%s\nP0/P1 wheel  P2–P4 axis  P5 step",
               c->i2c_mpg_addr, i2c_mpg_present() ? "present" : "not found");
    } else {
      snprintf(expbuf, sizeof(expbuf),
               "I2C MPG expander\nOff (i2c_mpg.enable 0)\nPCF8574 on SDA15/SCL16");
    }
    lv_label_set_text(exp, expbuf);
    lv_obj_set_style_text_font(exp, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(exp, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_pos(exp, 4, 400);

    lv_obj_t *sd = lv_label_create(settings_panel);
    char sdbuf[160];
    snprintf(sdbuf, sizeof(sdbuf),
             "JOB STORE\nLittleFS %s\nSD/TF %s\nEdit pins: SD:/settings.cfg",
             job_store_lfs_ok() ? "ok" : "—", job_store_sd_ok() ? "mounted" : "—");
    lv_label_set_text(sd, sdbuf);
    lv_obj_set_style_text_font(sd, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(sd, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_pos(sd, 4, 480);
    refresh_transport_ui();
  }

  // ----- GCODE panel -----
  gcode_panel = lv_obj_create(content);
  lv_obj_set_size(gcode_panel, UI_H_RES - 12, content_h - 8);
  lv_obj_set_pos(gcode_panel, 0, 0);
  lv_obj_set_style_bg_opa(gcode_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(gcode_panel, 0, 0);
  lv_obj_set_style_pad_all(gcode_panel, 4, 0);
  lv_obj_clear_flag(gcode_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(gcode_panel, LV_OBJ_FLAG_HIDDEN);

  {
    const int path_prev_h = 140;
    setup_path_preview(gcode_panel, 0, UI_H_RES - 12, path_prev_h);

    lv_obj_t *cap = lv_label_create(gcode_panel);
    lv_label_set_text(cap, "GCODE / JOBS");
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cap, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_pos(cap, 4, path_prev_h + 4);

    gcode_status_lbl = lv_label_create(gcode_panel);
    lv_obj_set_style_text_font(gcode_status_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(gcode_status_lbl, lv_color_hex(0xdddddd), 0);
    lv_label_set_long_mode(gcode_status_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(gcode_status_lbl, UI_H_RES - 28);
    lv_obj_set_pos(gcode_status_lbl, 4, path_prev_h + 24);
    lv_label_set_text(gcode_status_lbl, "");

    const int btn_h = 52;
    const int btn_y = content_h - 8 - btn_h - 56;
    int bw = (UI_H_RES - 12 - 20) / 3;
    gcode_run_btn = lv_btn_create(gcode_panel);
    lv_obj_set_size(gcode_run_btn, bw - 2, btn_h);
    lv_obj_set_pos(gcode_run_btn, 4, btn_y);
    style_chrome_btn(gcode_run_btn, false);
    lv_obj_set_style_bg_color(gcode_run_btn, lv_color_hex(0x1b5e20), 0);
    {
      lv_obj_t *l = lv_label_create(gcode_run_btn);
      lv_label_set_text(l, "RUN");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
      lv_obj_center(l);
    }
    lv_obj_add_event_cb(gcode_run_btn, on_gcode_run, LV_EVENT_CLICKED, NULL);

    gcode_hold_btn = lv_btn_create(gcode_panel);
    lv_obj_set_size(gcode_hold_btn, bw - 2, btn_h);
    lv_obj_set_pos(gcode_hold_btn, 4 + bw, btn_y);
    style_chrome_btn(gcode_hold_btn, false);
    {
      lv_obj_t *l = lv_label_create(gcode_hold_btn);
      lv_label_set_text(l, "HOLD");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
      lv_obj_center(l);
    }
    lv_obj_add_event_cb(gcode_hold_btn, on_gcode_hold, LV_EVENT_CLICKED, NULL);

    gcode_stop_btn = lv_btn_create(gcode_panel);
    lv_obj_set_size(gcode_stop_btn, bw - 2, btn_h);
    lv_obj_set_pos(gcode_stop_btn, 4 + 2 * bw, btn_y);
    style_chrome_btn(gcode_stop_btn, false);
    lv_obj_set_style_bg_color(gcode_stop_btn, lv_color_hex(0x6b1b1b), 0);
    {
      lv_obj_t *l = lv_label_create(gcode_stop_btn);
      lv_label_set_text(l, "STOP");
      lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
      lv_obj_center(l);
    }
    lv_obj_add_event_cb(gcode_stop_btn, on_gcode_stop, LV_EVENT_CLICKED, NULL);

    gcode_prog_lbl = lv_label_create(gcode_panel);
    lv_obj_set_style_text_font(gcode_prog_lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(gcode_prog_lbl, lv_color_hex(0xaad4aa), 0);
    lv_label_set_long_mode(gcode_prog_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(gcode_prog_lbl, UI_H_RES - 28);
    lv_obj_set_pos(gcode_prog_lbl, 4, btn_y + btn_h + 6);
    lv_label_set_text(gcode_prog_lbl, "Ready");

    const int list_y = path_prev_h + 48;
    gcode_list = lv_obj_create(gcode_panel);
    lv_obj_set_size(gcode_list, UI_H_RES - 20, btn_y - list_y - 4);
    lv_obj_set_pos(gcode_list, 0, list_y);
    lv_obj_set_style_bg_color(gcode_list, lv_color_hex(0x2a2a2a), 0);
    lv_obj_set_style_border_width(gcode_list, 1, 0);
    lv_obj_set_style_border_color(gcode_list, lv_color_hex(0x111111), 0);
    lv_obj_set_style_pad_all(gcode_list, 4, 0);
    lv_obj_set_style_pad_row(gcode_list, 4, 0);
    lv_obj_set_flex_flow(gcode_list, LV_FLEX_FLOW_COLUMN);
    refresh_prog_ui();
  }

  // ----- PROBE panel -----
  probe_panel = lv_obj_create(content);
  lv_obj_set_size(probe_panel, UI_H_RES - 12, content_h - 8);
  lv_obj_set_pos(probe_panel, 0, 0);
  lv_obj_set_style_bg_opa(probe_panel, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(probe_panel, 0, 0);
  lv_obj_set_style_pad_all(probe_panel, 4, 0);
  lv_obj_clear_flag(probe_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(probe_panel, LV_OBJ_FLAG_HIDDEN);

  {
    lv_obj_t *cap = lv_label_create(probe_panel);
    lv_label_set_text(cap, "PROBE");
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cap, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_pos(cap, 4, 4);

    lv_obj_t *ac = lv_label_create(probe_panel);
    lv_label_set_text(ac, "AXIS");
    lv_obj_set_style_text_font(ac, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(ac, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(ac, 4, 32);
    int abw = (UI_H_RES - 12 - 16) / 3;
    for (int i = 0; i < 3; i++) {
      probe_axis_btns[i] = lv_btn_create(probe_panel);
      lv_obj_set_size(probe_axis_btns[i], abw - 2, 52);
      lv_obj_set_pos(probe_axis_btns[i], 4 + i * abw, 52);
      style_chrome_btn(probe_axis_btns[i], i == active_axis);
      lv_obj_t *l = lv_label_create(probe_axis_btns[i]);
      lv_label_set_text(l, k_axis[i]);
      lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
      lv_obj_center(l);
      lv_obj_add_event_cb(probe_axis_btns[i], on_probe_axis, LV_EVENT_CLICKED,
                          (void *)(intptr_t)i);
    }

    probe_feed_lbl = lv_label_create(probe_panel);
    lv_obj_set_style_text_font(probe_feed_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(probe_feed_lbl, lv_color_hex(0xffffff), 0);
    lv_obj_set_pos(probe_feed_lbl, 4, 120);

    const int bumps[] = {-50, -10, 10, 50};
    const char *blabs[] = {"−50", "−10", "+10", "+50"};
    int bbw = (UI_H_RES - 12 - 20) / 4;
    for (int i = 0; i < 4; i++) {
      lv_obj_t *b = lv_btn_create(probe_panel);
      lv_obj_set_size(b, bbw - 2, 40);
      lv_obj_set_pos(b, 4 + i * bbw, 150);
      style_chrome_btn(b, false);
      lv_obj_t *l = lv_label_create(b);
      lv_label_set_text(l, blabs[i]);
      lv_obj_center(l);
      lv_obj_add_event_cb(b, on_probe_feed_bump, LV_EVENT_CLICKED, (void *)(intptr_t)bumps[i]);
    }

    lv_obj_t *strict_btn = lv_btn_create(probe_panel);
    lv_obj_set_size(strict_btn, (UI_H_RES - 20) / 2 - 4, 44);
    lv_obj_set_pos(strict_btn, 4, 204);
    style_chrome_btn(strict_btn, false);
    lv_obj_t *sl = lv_label_create(strict_btn);
    lv_label_set_text(sl, "G38.2 / G38.3");
    lv_obj_set_style_text_font(sl, &lv_font_montserrat_14, 0);
    lv_obj_center(sl);
    lv_obj_add_event_cb(strict_btn, on_probe_strict, LV_EVENT_CLICKED, NULL);

    lv_obj_t *zero_btn = lv_btn_create(probe_panel);
    lv_obj_set_size(zero_btn, (UI_H_RES - 20) / 2 - 4, 44);
    lv_obj_set_pos(zero_btn, 4 + (UI_H_RES - 20) / 2, 204);
    style_chrome_btn(zero_btn, false);
    lv_obj_t *zl = lv_label_create(zero_btn);
    lv_label_set_text(zl, "Zero WPos");
    lv_obj_set_style_text_font(zl, &lv_font_montserrat_14, 0);
    lv_obj_center(zl);
    lv_obj_add_event_cb(zero_btn, on_probe_zero, LV_EVENT_CLICKED, NULL);

    probe_contact_lbl = lv_label_create(probe_panel);
    lv_obj_set_style_text_font(probe_contact_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(probe_contact_lbl, lv_color_hex(0xaaaaaa), 0);
    lv_obj_set_pos(probe_contact_lbl, 4, 268);
    lv_label_set_text(probe_contact_lbl, "Contact: —");

    probe_result_lbl = lv_label_create(probe_panel);
    lv_obj_set_style_text_font(probe_result_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(probe_result_lbl, lv_color_hex(0xdddddd), 0);
    lv_label_set_long_mode(probe_result_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(probe_result_lbl, UI_H_RES - 28);
    lv_obj_set_pos(probe_result_lbl, 4, 300);
    lv_label_set_text(probe_result_lbl, "Last [PRB:]: none");

    lv_obj_t *hint = lv_label_create(probe_panel);
    lv_label_set_long_mode(hint, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(hint, UI_H_RES - 28);
    lv_label_set_text(hint,
                      "Soft Probe −/+ sends G38 toward WPos ± travel.\n"
                      "Requires Idle + MPG. [PRB:] is machine coords.");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_set_pos(hint, 4, 360);
    refresh_probe_ui();
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
  lv_obj_add_flag(other_panel, LV_OBJ_FLAG_HIDDEN);

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

static void apply_handwheel() {
  if (!mpg_on || page_idx != 0) {
    (void)handwheel_take_delta();  // discard while not in MPG control
    return;
  }
  int32_t d = handwheel_take_delta();
  if (!d) return;
  // One detent ≈ one step size on the active axis
  float dist = (float)d * k_steps[step_idx];
  if (grbl_link_online()) {
    grbl_link_jog(active_axis, dist, jog_feed_for_step());
  } else {
    wpos[active_axis] += dist;
    refresh_axes();
  }
}

static void apply_i2c_buttons() {
  int ax = i2c_mpg_take_axis();
  if (ax >= 0 && ax < 3) {
    active_axis = ax;
    refresh_axes();
    if (page_idx == 2) refresh_power_feed_ui();
    if (page_idx == 4) refresh_probe_ui();
  }
  if (i2c_mpg_take_step_cycle()) {
    step_idx = (step_idx + 1) % 4;
    refresh_steps();
  }
}

void ui_pendant_tick() {
  apply_handwheel();
  apply_i2c_buttons();

  static uint32_t last_ui_ms = 0;
  const uint32_t now = millis();
  if (now - last_ui_ms < 100) return;
  last_ui_ms = now;

  if (page_idx == 3) refresh_prog_ui();

  grbl_status_t st;
  if (!grbl_link_get_status(&st) || !st.online) {
    char offline[64];
    const char *hw = handwheel_hw_present() ? "MPG hw"
                     : i2c_mpg_present()      ? "I2C MPG"
                                              : "mm";
    snprintf(offline, sizeof(offline), "Offline  ·  demo  ·  %s", hw);
    lv_label_set_text(state_lbl, offline);
    if (page_idx == 4 && probe_contact_lbl) {
      lv_label_set_text(probe_contact_lbl, "Contact: — (offline)");
    }
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
  coolant_flood = st.coolant_flood;
  coolant_mist = st.coolant_mist;

  if (mpg_on != st.mpg_mode) {
    mpg_on = st.mpg_mode;
    refresh_mpg();
  }

  if (page_idx == 4) {
    if (probe_contact_lbl) {
      lv_label_set_text(probe_contact_lbl,
                        st.probe_triggered ? "Contact: TRIGGERED" : "Contact: open");
      lv_obj_set_style_text_color(
          probe_contact_lbl, st.probe_triggered ? lv_color_hex(0xf59e0b) : lv_color_hex(0xaaaaaa),
          0);
    }
    if (st.probe_fresh && probe_result_lbl) {
      char buf[96];
      snprintf(buf, sizeof(buf), "Last [PRB:]  %.3f, %.3f, %.3f  %s", st.probe_mpos[0],
               st.probe_mpos[1], st.probe_mpos[2], st.probe_ok ? "ok" : "FAIL");
      lv_label_set_text(probe_result_lbl, buf);
      grbl_link_probe_ack();
    }
  }

  char buf[48];
  snprintf(buf, sizeof(buf), "%s  ·  Laser  ·  mm", st.state[0] ? st.state : "?");
  lv_label_set_text(state_lbl, buf);

  refresh_axes();
  refresh_telemetry();
  if (page_idx == 1) refresh_override_ui();
  if (page_idx == 2) refresh_power_feed_ui();
}
