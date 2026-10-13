#include "macros.h"
#include "grbl_link.h"
#include "pendant_cfg.h"
#include "program_sender.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>
#include <stdio.h>
#include <string.h>

static const char *kDefaultMacros =
    "# Soft macros — edit on SD as /macros.cfg (wins over LittleFS).\n"
    "# Format:  Name = g-code  (use ; to chain a few short commands)\n"
    "# Needs Idle + MPG when the controller is online.\n"
    "\n"
    "XY0 = G90 G0 X0 Y0\n"
    "Park = G90 G0 X0 Y0\n"
    "Z0 = G90 G0 Z0\n"
    "Unlock = $X\n";

static macro_t g_macros[MACRO_MAX];
static unsigned g_count = 0;

static bool g_busy = false;
static char g_queue[MACRO_GCODE_LEN];
static char *g_qpos = nullptr;
static bool g_await = false;
static uint32_t g_deadline = 0;
static bool g_demo = false;

static bool read_file(fs::FS &fs, const char *path, char *buf, size_t buf_sz) {
  File f = fs.open(path, "r");
  if (!f) return false;
  size_t n = f.readBytes(buf, buf_sz - 1);
  f.close();
  buf[n] = 0;
  return n > 0;
}

static void load_defaults(void) {
  g_count = macros_parse_text(kDefaultMacros, g_macros, MACRO_MAX);
}

void macros_begin(void) {
  load_defaults();

  char *buf = (char *)malloc(2048);
  if (!buf) return;

  bool from_sd = false;
  if (pendant_cfg_sd_mounted() && SD.exists("/macros.cfg")) {
    if (read_file(SD, "/macros.cfg", buf, 2048)) {
      unsigned n = macros_parse_text(buf, g_macros, MACRO_MAX);
      if (n > 0) {
        g_count = n;
        from_sd = true;
        Serial.printf("[macro] loaded sd:/macros.cfg (%u)\n", g_count);
      }
    }
  }

  if (!from_sd && pendant_cfg_lfs_mounted()) {
    if (!LittleFS.exists("/macros.cfg")) {
      File f = LittleFS.open("/macros.cfg", "w");
      if (f) {
        f.print(kDefaultMacros);
        f.close();
        Serial.println("[macro] wrote default lfs:/macros.cfg");
      }
    }
    if (read_file(LittleFS, "/macros.cfg", buf, 2048)) {
      unsigned n = macros_parse_text(buf, g_macros, MACRO_MAX);
      if (n > 0) {
        g_count = n;
        Serial.printf("[macro] loaded lfs:/macros.cfg (%u)\n", g_count);
      }
    }
  }

  free(buf);
}

unsigned macros_count(void) { return g_count; }

const macro_t *macros_get(unsigned index) {
  if (index >= g_count) return nullptr;
  return &g_macros[index];
}

bool macros_busy(void) { return g_busy; }

static bool controller_ready(const char *gcode) {
  if (!grbl_link_online()) return true;
  grbl_status_t st;
  if (!grbl_link_get_status(&st)) return false;
  if (!strcmp(st.state, "Idle") && st.mpg_mode) return true;
  // Unlock-style system cmds from Alarm
  if (!strcmp(st.state, "Alarm") && gcode && gcode[0] == '$') return true;
  return false;
}

static bool send_next_chunk(void) {
  if (!g_qpos || !*g_qpos) {
    g_busy = false;
    g_await = false;
    return false;
  }
  while (*g_qpos == ' ' || *g_qpos == '\t' || *g_qpos == ';') g_qpos++;
  if (!*g_qpos) {
    g_busy = false;
    return false;
  }

  char cmd[96];
  size_t i = 0;
  while (*g_qpos && *g_qpos != ';' && i + 1 < sizeof(cmd)) {
    cmd[i++] = *g_qpos++;
  }
  cmd[i] = 0;
  // trim trailing
  while (i > 0 && (cmd[i - 1] == ' ' || cmd[i - 1] == '\t')) cmd[--i] = 0;
  if (!cmd[0]) return send_next_chunk();

  if (!grbl_link_send_line(cmd)) {
    g_busy = false;
    return false;
  }
  g_await = true;
  g_deadline = millis() + (g_demo ? 40u : 5000u);
  Serial.printf("[macro] > %s\n", cmd);
  return true;
}

bool macros_run(unsigned index) {
  if (g_busy || index >= g_count) return false;
  prog_state_t ps = program_sender_state();
  if (ps == PROG_RUNNING || ps == PROG_HOLD || ps == PROG_DRAINING) return false;
  if (!controller_ready(g_macros[index].gcode) && grbl_link_online()) return false;

  strncpy(g_queue, g_macros[index].gcode, sizeof(g_queue) - 1);
  g_queue[sizeof(g_queue) - 1] = 0;
  g_qpos = g_queue;
  g_busy = true;
  g_await = false;
  g_demo = !grbl_link_online();
  grbl_link_clear_rx_evt();
  if (!g_demo) grbl_link_set_mpg(true);
  Serial.printf("[macro] RUN %s\n", g_macros[index].name);
  return send_next_chunk();
}

void macros_poll(void) {
  if (!g_busy) return;

  if (g_await) {
    if (g_demo) {
      if ((int32_t)(millis() - g_deadline) >= 0) {
        g_await = false;
        if (!send_next_chunk()) g_busy = false;
      }
      return;
    }
    grbl_rx_evt_t e = grbl_link_take_rx_evt();
    if (e == GRBL_RX_OK) {
      g_await = false;
      if (!send_next_chunk()) g_busy = false;
    } else if (e == GRBL_RX_ERROR || e == GRBL_RX_ALARM) {
      g_busy = false;
      g_await = false;
      Serial.println("[macro] stopped on error/ALARM");
    } else if ((int32_t)(millis() - g_deadline) >= 0) {
      g_busy = false;
      g_await = false;
      Serial.println("[macro] ack timeout");
    }
  }
}
