#include "program_sender.h"
#include "grbl_link.h"
#include "job_store.h"
#include "sample_job_nc.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>
#include <esp_heap_caps.h>
#include <stdio.h>
#include <string.h>

static const size_t kMemCap = 96 * 1024;
static const uint32_t kAckTimeoutMs = 8000;
static const uint32_t kDemoAckMs = 40;

static prog_state_t g_state = PROG_IDLE;
static char g_path[JOB_STORE_PATH_LEN] = "embedded:sample";
static char g_name[JOB_STORE_NAME_LEN] = "sample-badge.nc";
static char g_msg[96] = "Idle";

static char *g_mem = nullptr;
static size_t g_mem_len = 0;
static size_t g_mem_pos = 0;
static bool g_mem_owned = false;

static File g_file;
static bool g_file_open = false;

static uint32_t g_line_no = 0;       // program line index (1-based when sending)
static uint32_t g_sent = 0;          // lines actually transmitted
static uint32_t g_total = 0;         // counted when loaded into memory
static bool g_awaiting_ack = false;
static uint32_t g_ack_deadline = 0;
static bool g_demo = false;

static void set_msg(const char *m) {
  if (!m) return;
  strncpy(g_msg, m, sizeof(g_msg) - 1);
  g_msg[sizeof(g_msg) - 1] = 0;
}

static void close_sources(void) {
  if (g_file_open) {
    g_file.close();
    g_file_open = false;
  }
  if (g_mem && g_mem_owned) {
    free(g_mem);
  }
  g_mem = nullptr;
  g_mem_owned = false;
  g_mem_len = 0;
  g_mem_pos = 0;
}

static uint32_t count_lines(const char *text) {
  if (!text || !text[0]) return 0;
  uint32_t n = 0;
  for (const char *p = text; *p; p++) {
    if (*p == '\n') n++;
  }
  // Last line without trailing newline
  size_t len = strlen(text);
  if (len > 0 && text[len - 1] != '\n') n++;
  return n;
}

static void set_name_from_path(const char *path) {
  const char *base = strrchr(path, '/');
  base = base ? base + 1 : path;
  if (!strncmp(path, "embedded:", 9)) base = SAMPLE_JOB_NAME;
  strncpy(g_name, base, sizeof(g_name) - 1);
  g_name[sizeof(g_name) - 1] = 0;
}

static bool open_file_stream(const char *path) {
  if (LittleFS.exists(path)) {
    g_file = LittleFS.open(path, FILE_READ);
  } else if (SD.exists(path)) {
    g_file = SD.open(path, FILE_READ);
  } else {
    return false;
  }
  if (!g_file) return false;
  g_file_open = true;
  g_total = 0;
  return true;
}

bool program_sender_load(const char *path) {
  if (!path || !path[0]) return false;
  if (g_state == PROG_RUNNING || g_state == PROG_HOLD || g_state == PROG_DRAINING) {
    set_msg("Stop job before loading");
    return false;
  }

  close_sources();
  strncpy(g_path, path, sizeof(g_path) - 1);
  g_path[sizeof(g_path) - 1] = 0;
  set_name_from_path(path);
  g_line_no = 0;
  g_sent = 0;
  g_total = 0;
  g_awaiting_ack = false;
  g_state = PROG_IDLE;

  // Embedded or small file → memory
  char *buf = (char *)heap_caps_malloc(kMemCap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!buf) buf = (char *)malloc(kMemCap);
  if (buf) {
    size_t len = 0;
    if (job_store_read(path, buf, kMemCap, &len)) {
      g_mem = buf;
      g_mem_owned = true;
      g_mem_len = len;
      g_mem_pos = 0;
      g_total = count_lines(g_mem);
      char m[96];
      snprintf(m, sizeof(m), "Ready · %s · %lu lines", g_name, (unsigned long)g_total);
      set_msg(m);
      Serial.printf("[prog] loaded mem %s (%u bytes, %lu lines)\n", g_path, (unsigned)len,
                    (unsigned long)g_total);
      return true;
    }
    free(buf);
  }

  // Large / unreadable into RAM → stream from FS (not for embedded)
  if (!strncmp(path, "embedded:", 9)) {
    set_msg("Load failed");
    return false;
  }
  if (!open_file_stream(path)) {
    set_msg("Open failed");
    return false;
  }
  set_msg("Ready · streaming from card");
  Serial.printf("[prog] streaming file %s\n", g_path);
  return true;
}

static bool rewind_source(void) {
  g_mem_pos = 0;
  g_line_no = 0;
  g_sent = 0;
  g_awaiting_ack = false;
  if (g_mem) return true;
  if (g_file_open) {
    g_file.seek(0);
    return true;
  }
  // Reload
  return program_sender_load(g_path);
}

static bool read_raw_line(char *out, size_t out_sz) {
  if (!out || out_sz < 2) return false;
  out[0] = 0;

  if (g_mem) {
    if (g_mem_pos >= g_mem_len) return false;
    size_t i = 0;
    while (g_mem_pos < g_mem_len && i + 1 < out_sz) {
      char c = g_mem[g_mem_pos++];
      if (c == '\n') break;
      if (c == '\r') continue;
      out[i++] = c;
    }
    out[i] = 0;
    return true;
  }

  if (g_file_open && g_file) {
    if (!g_file.available()) return false;
    size_t i = 0;
    while (g_file.available() && i + 1 < out_sz) {
      int c = g_file.read();
      if (c < 0) break;
      if (c == '\n') break;
      if (c == '\r') continue;
      out[i++] = (char)c;
    }
    out[i] = 0;
    return true;
  }
  return false;
}

static bool controller_can_stream(void) {
  if (!grbl_link_online()) return true;  // demo pacing
  grbl_status_t st;
  if (!grbl_link_get_status(&st)) return false;
  if (!st.mpg_mode) return false;
  return !strcmp(st.state, "Idle") || !strcmp(st.state, "Run") || !strcmp(st.state, "Hold");
}

bool program_sender_start(void) {
  if (g_state == PROG_RUNNING || g_state == PROG_DRAINING) return false;
  if (g_state == PROG_HOLD) {
    program_sender_resume();
    return g_state == PROG_RUNNING;
  }

  if (!g_mem && !g_file_open) {
    if (!program_sender_load(g_path)) return false;
  }
  if (!rewind_source()) {
    set_msg("Rewind failed");
    return false;
  }

  g_demo = !grbl_link_online();
  if (!g_demo) {
    grbl_link_set_mpg(true);
    if (!controller_can_stream()) {
      set_msg("Need Idle + MPG");
      return false;
    }
  }

  grbl_link_clear_rx_evt();
  g_state = PROG_RUNNING;
  set_msg(g_demo ? "Streaming · demo" : "Streaming…");
  Serial.printf("[prog] START %s demo=%d\n", g_name, (int)g_demo);
  return true;
}

void program_sender_hold(void) {
  if (g_state != PROG_RUNNING && g_state != PROG_DRAINING) return;
  grbl_link_feed_hold();
  g_state = PROG_HOLD;
  set_msg("Hold");
}

void program_sender_resume(void) {
  if (g_state != PROG_HOLD) return;
  grbl_link_cycle_start();
  g_state = PROG_RUNNING;
  set_msg(g_demo ? "Streaming · demo" : "Streaming…");
}

void program_sender_stop(void) {
  if (g_state == PROG_IDLE || g_state == PROG_DONE) {
    grbl_link_feed_hold();
    return;
  }
  g_awaiting_ack = false;
  grbl_link_feed_hold();
  grbl_link_jog_cancel();
  g_state = PROG_IDLE;
  set_msg("Stopped");
  Serial.println("[prog] STOP");
}

static void finish_ok(void) {
  g_awaiting_ack = false;
  g_state = PROG_DONE;
  char m[96];
  snprintf(m, sizeof(m), "Done · %lu lines", (unsigned long)g_sent);
  set_msg(m);
  Serial.printf("[prog] DONE sent=%lu\n", (unsigned long)g_sent);
}

static bool send_next(void) {
  char raw[128];
  char cmd[160];

  // Skip comment-only lines (still advance program line numbers)
  for (int skip = 0; skip < 64; skip++) {
    if (!read_raw_line(raw, sizeof(raw))) {
      g_state = PROG_DRAINING;
      set_msg("Draining planner…");
      return false;
    }
    g_line_no++;
    if (program_sender_prepare_line(raw, g_line_no, cmd, sizeof(cmd))) {
      if (!grbl_link_send_line(cmd)) {
        g_state = PROG_ERROR;
        set_msg("Send failed");
        return false;
      }
      g_awaiting_ack = true;
      g_ack_deadline = millis() + (g_demo ? kDemoAckMs : kAckTimeoutMs);
      Serial.printf("[prog] > %s\n", cmd);
      return true;
    }
  }
  return false;
}

void program_sender_poll(void) {
  if (g_state != PROG_RUNNING && g_state != PROG_DRAINING && g_state != PROG_HOLD) return;

  // Hold: do not send; still watch for errors
  if (g_state == PROG_HOLD) {
    grbl_rx_evt_t e = grbl_link_take_rx_evt();
    if (e == GRBL_RX_ERROR || e == GRBL_RX_ALARM) {
      g_state = PROG_ERROR;
      set_msg(e == GRBL_RX_ALARM ? "ALARM — stopped" : "error — stopped");
      g_awaiting_ack = false;
    }
    return;
  }

  if (g_state == PROG_DRAINING) {
    if (g_demo) {
      finish_ok();
      return;
    }
    grbl_status_t st;
    if (grbl_link_get_status(&st) && !strcmp(st.state, "Idle")) {
      finish_ok();
    }
    grbl_rx_evt_t e = grbl_link_take_rx_evt();
    if (e == GRBL_RX_ERROR || e == GRBL_RX_ALARM) {
      g_state = PROG_ERROR;
      set_msg(e == GRBL_RX_ALARM ? "ALARM — stopped" : "error — stopped");
    }
    return;
  }

  // PROG_RUNNING
  if (!g_demo && !controller_can_stream()) {
    // Controller left Idle/Run/Hold — stop safely
    g_state = PROG_ERROR;
    g_awaiting_ack = false;
    set_msg("Controller not ready — stopped");
    return;
  }

  if (g_awaiting_ack) {
    if (g_demo) {
      if ((int32_t)(millis() - g_ack_deadline) >= 0) {
        g_awaiting_ack = false;
        g_sent++;
      }
      return;
    }
    grbl_rx_evt_t e = grbl_link_take_rx_evt();
    if (e == GRBL_RX_OK) {
      g_awaiting_ack = false;
      g_sent++;
    } else if (e == GRBL_RX_ERROR || e == GRBL_RX_ALARM) {
      g_awaiting_ack = false;
      g_state = PROG_ERROR;
      set_msg(e == GRBL_RX_ALARM ? "ALARM — stopped" : "error — stopped");
      Serial.printf("[prog] abort on %s\n", e == GRBL_RX_ALARM ? "ALARM" : "error");
    } else if ((int32_t)(millis() - g_ack_deadline) >= 0) {
      g_awaiting_ack = false;
      g_state = PROG_ERROR;
      set_msg("Ack timeout — stopped");
      Serial.println("[prog] ack timeout");
    }
    return;
  }

  if (!controller_can_stream() && !g_demo) return;

  // Don't push new lines while controller is in Hold
  if (!g_demo) {
    grbl_status_t st;
    if (grbl_link_get_status(&st) && !strcmp(st.state, "Hold")) return;
  }

  (void)send_next();
}

prog_state_t program_sender_state(void) { return g_state; }
uint32_t program_sender_sent_lines(void) { return g_sent; }
uint32_t program_sender_total_lines(void) { return g_total; }
const char *program_sender_job_name(void) { return g_name; }
const char *program_sender_message(void) { return g_msg; }
