#include "grbl_link.h"
#include "pendant_cfg.h"
#include "mpg_framed.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// Realtime / legacy bytes (grblHAL protocol.c)
static const uint8_t kCmdStatus = '?';
static const uint8_t kCmdFeedHold = '!';
static const uint8_t kCmdCycleStart = '~';
static const uint8_t kCmdJogCancel = 0x85;
static const uint8_t kCmdMpgToggle = 0x8B;
static const uint8_t kResyncMarker = 0x18;

static const uint8_t kOvBytes[] = {
    0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x99,
    0x9A, 0x9B, 0x9C, 0x9D, 0xA0, 0xA1,
};

static HardwareSerial *GrblSerial = nullptr;

static grbl_status_t g_status;
static char rx_line[192];
static size_t rx_len = 0;
static uint32_t last_status_rx_ms = 0;
static uint32_t last_status_tx_ms = 0;
static uint32_t last_mpg_tx_ms = 0;
static bool want_mpg = true;
static bool ever_seen = false;
static bool use_framed = false;
static mpg_framed_t *framed = nullptr;
static grbl_rx_evt_t g_rx_evt = GRBL_RX_NONE;

// --- framed HW glue ---
static bool hw_read(void *ctx, uint8_t *byte) {
  (void)ctx;
  if (!GrblSerial || GrblSerial->available() <= 0) return false;
  int c = GrblSerial->read();
  if (c < 0) return false;
  *byte = (uint8_t)c;
  return true;
}

static bool hw_write(void *ctx, const uint8_t *data, uint32_t len) {
  (void)ctx;
  if (!GrblSerial) return false;
  return GrblSerial->write(data, len) == len;
}

static bool hw_tx_idle(void *ctx) {
  (void)ctx;
  return true;  // HardwareSerial buffers; treat as always ready
}

static uint32_t hw_now(void *ctx) {
  (void)ctx;
  return millis();
}

static void status_reset_defaults() {
  memset(&g_status, 0, sizeof(g_status));
  strncpy(g_status.state, "Offline", sizeof(g_status.state) - 1);
  g_status.feed_ov = 100;
  g_status.rapid_ov = 100;
  g_status.speed_ov = 100;
  g_status.use_wpos = true;
}

static void handle_line(char *line) {
  if (!line || !line[0]) return;

  if (line[0] == '<') {
    grbl_status_t next = g_status;
    if (grbl_parse_status_line(line, &next)) {
      g_status = next;
      last_status_rx_ms = millis();
      ever_seen = true;
    }
    return;
  }

  if (line[0] == '[' && !strncmp(line, "[PRB:", 5)) {
    if (grbl_parse_probe_line(line, &g_status)) {
      Serial.printf("[grbl] PRB %.3f,%.3f,%.3f ok=%d\n", g_status.probe_mpos[0],
                    g_status.probe_mpos[1], g_status.probe_mpos[2], (int)g_status.probe_ok);
    }
    return;
  }

  if (!strcmp(line, "ok")) {
    g_rx_evt = GRBL_RX_OK;
    Serial.printf("[grbl] %s\n", line);
    return;
  }
  if (!strncmp(line, "error:", 6)) {
    g_rx_evt = GRBL_RX_ERROR;
    Serial.printf("[grbl] %s\n", line);
    return;
  }
  if (!strncmp(line, "ALARM:", 6)) {
    g_rx_evt = GRBL_RX_ALARM;
    Serial.printf("[grbl] %s\n", line);
    return;
  }
  if (!strncmp(line, "Grbl", 4)) {
    Serial.printf("[grbl] %s\n", line);
  }
}

static void feed_byte(uint8_t c) {
  if (c == kResyncMarker) {
    rx_len = 0;  // drop partial line after framed gap
    return;
  }
  if (c == '\n' || c == '\r') {
    if (rx_len > 0) {
      rx_line[rx_len] = '\0';
      handle_line(rx_line);
      rx_len = 0;
    }
    return;
  }
  if (rx_len + 1 < sizeof(rx_line)) {
    rx_line[rx_len++] = (char)c;
  } else {
    rx_len = 0;
  }
}

static void drain_rx() {
  if (use_framed && framed) {
    mpg_framed_service(framed);
    uint8_t b;
    while (mpg_framed_read(framed, &b)) feed_byte(b);
    return;
  }
  if (!GrblSerial) return;
  while (GrblSerial->available() > 0) {
    int c = GrblSerial->read();
    if (c < 0) break;
    feed_byte((uint8_t)c);
  }
}

static void send_rt(uint8_t b) {
  if (use_framed && framed) {
    // Retry a few times if Rlt channel is busy
    for (int i = 0; i < 8; i++) {
      if (mpg_framed_write(framed, &b, 1)) return;
      mpg_framed_service(framed);
      delay(1);
    }
    Serial.println("[grbl] framed RT write busy");
    return;
  }
  if (GrblSerial) GrblSerial->write(b);
}

static void send_line(const char *s) {
  char buf[96];
  size_t n = strlen(s);
  if (n + 2 > sizeof(buf)) return;
  memcpy(buf, s, n);
  if (n == 0 || buf[n - 1] != '\r') buf[n++] = '\r';
  buf[n] = '\0';

  if (use_framed && framed) {
    for (int i = 0; i < 16; i++) {
      if (mpg_framed_write(framed, (const uint8_t *)buf, (uint32_t)n)) return;
      mpg_framed_service(framed);
      delay(2);
    }
    Serial.println("[grbl] framed Dat write busy");
    return;
  }
  if (GrblSerial) GrblSerial->write((const uint8_t *)buf, n);
}

static void ensure_framed(bool enable) {
  if (enable == use_framed && (!enable || framed)) return;
  use_framed = enable;
  rx_len = 0;
  if (enable) {
    if (!framed) {
      mpg_framed_hw_t hw = {nullptr, hw_read, hw_write, hw_tx_idle, hw_now};
      framed = mpg_framed_create(&hw);
      if (!framed) {
        Serial.println("[grbl] framed alloc failed — staying plain");
        use_framed = false;
        return;
      }
      mpg_framed_set_baud(framed, (uint32_t)pendant_cfg_get()->uart_baud);
    } else {
      mpg_framed_reset(framed);
    }
    Serial.println("[grbl] transport: FRAMED (Plugin_mpg_transport)");
  } else {
    Serial.println("[grbl] transport: PLAIN ASCII");
  }
}

void grbl_link_begin(void) {
  status_reset_defaults();
  const pendant_cfg_t *c = pendant_cfg_get();
  if (!GrblSerial) GrblSerial = new HardwareSerial(c->uart_num);
  GrblSerial->begin(c->uart_baud, SERIAL_8N1, c->uart_rx, c->uart_tx);
  delay(20);
  while (GrblSerial->available()) GrblSerial->read();

#if defined(GRBL_TRANSPORT_FRAMED) && GRBL_TRANSPORT_FRAMED
  ensure_framed(true);
#else
  ensure_framed(false);
#endif

  Serial.printf("[grbl] UART%d TX=%d RX=%d @ %d  mode=%s\n", c->uart_num, c->uart_tx, c->uart_rx,
                c->uart_baud, use_framed ? "framed" : "plain");
  last_status_tx_ms = 0;
  last_status_rx_ms = 0;
  want_mpg = true;
}

void grbl_link_poll(void) {
  drain_rx();

  const uint32_t now = millis();
  // In framed mode, also require link_up once we've tried
  bool link_ok = !use_framed || !framed || mpg_framed_link_up(framed) || ever_seen;
  g_status.online = ever_seen && link_ok && (now - last_status_rx_ms) < 800u;
  g_status.status_age_ms = ever_seen ? (now - last_status_rx_ms) : 0xFFFFFFFFu;

  if (now - last_status_tx_ms >= 100u) {
    send_rt(kCmdStatus);
    last_status_tx_ms = now;
  }

  if (want_mpg && g_status.online && !g_status.mpg_mode) {
    if (now - last_mpg_tx_ms >= 300u) {
      send_rt(kCmdMpgToggle);
      last_mpg_tx_ms = now;
      Serial.println("[grbl] MPG toggle (claim)");
    }
  }
}

bool grbl_link_get_status(grbl_status_t *out) {
  if (!out) return false;
  *out = g_status;
  return ever_seen;
}

bool grbl_link_online(void) { return g_status.online; }

bool grbl_link_framed(void) { return use_framed; }

void grbl_link_set_framed(bool framed_on) {
  if (framed_on == use_framed) return;
  // Flush UART junk when switching modes
  if (GrblSerial) {
    while (GrblSerial->available()) GrblSerial->read();
  }
  ensure_framed(framed_on);
  ever_seen = false;
  last_status_rx_ms = 0;
  status_reset_defaults();
}

void grbl_link_set_mpg(bool want_control) {
  want_mpg = want_control;
  if (!grbl_link_online()) return;
  if (want_control != g_status.mpg_mode) {
    send_rt(kCmdMpgToggle);
    last_mpg_tx_ms = millis();
  }
}

void grbl_link_feed_hold(void) { send_rt(kCmdFeedHold); }

void grbl_link_cycle_start(void) { send_rt(kCmdCycleStart); }

void grbl_link_jog_cancel(void) { send_rt(kCmdJogCancel); }

void grbl_link_override(grbl_ov_t cmd) {
  if ((unsigned)cmd >= sizeof(kOvBytes) / sizeof(kOvBytes[0])) return;
  send_rt(kOvBytes[cmd]);
}

bool grbl_link_jog(int axis, float distance_mm, float feed_mm_min) {
  if (axis < 0 || axis >= GRBL_AXIS_CNT) return false;
  if (!grbl_link_online()) return false;
  if (!g_status.mpg_mode) return false;
  if (strcmp(g_status.state, "Idle") != 0 && strcmp(g_status.state, "Jog") != 0) return false;
  if (!(feed_mm_min > 0.0f)) feed_mm_min = 500.0f;

  static const char *axes = "XYZ";
  char cmd[64];
  snprintf(cmd, sizeof(cmd), "$J=G21G91%c%.3fF%.2f\r", axes[axis], distance_mm, feed_mm_min);
  send_line(cmd);
  return true;
}

bool grbl_link_zero_axis(int axis) {
  if (axis < 0 || axis >= GRBL_AXIS_CNT) return false;
  if (!grbl_link_online() || !g_status.mpg_mode) return false;
  static const char *axes = "XYZ";
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "G90G10L20P0%c0\r", axes[axis]);
  send_line(cmd);
  return true;
}

static bool send_probe(int axis, float target_mm, float feed_mm_min, const char *g38) {
  if (axis < 0 || axis >= GRBL_AXIS_CNT) return false;
  if (!grbl_link_online() || !g_status.mpg_mode) return false;
  if (strcmp(g_status.state, "Idle") != 0 && strcmp(g_status.state, "Hold") != 0) return false;
  if (!(feed_mm_min > 0.0f)) feed_mm_min = 100.0f;

  static const char *axes = "XYZ";
  g_status.probe_fresh = false;
  char cmd[72];
  snprintf(cmd, sizeof(cmd), "G90G21%s%c%.3fF%.2f\r", g38, axes[axis], target_mm, feed_mm_min);
  send_line(cmd);
  Serial.printf("[grbl] probe %s\n", cmd);
  return true;
}

bool grbl_link_probe_toward(int axis, float target_mm, float feed_mm_min, bool strict) {
  return send_probe(axis, target_mm, feed_mm_min, strict ? "G38.2" : "G38.3");
}

bool grbl_link_probe_away(int axis, float target_mm, float feed_mm_min) {
  return send_probe(axis, target_mm, feed_mm_min, "G38.4");
}

void grbl_link_probe_ack(void) { g_status.probe_fresh = false; }

grbl_rx_evt_t grbl_link_take_rx_evt(void) {
  grbl_rx_evt_t e = g_rx_evt;
  g_rx_evt = GRBL_RX_NONE;
  return e;
}

void grbl_link_clear_rx_evt(void) { g_rx_evt = GRBL_RX_NONE; }

bool grbl_link_send_line(const char *line) {
  if (!line || !line[0]) return false;
  send_line(line);
  return true;
}

bool grbl_link_laser_enable(void) {
  if (!grbl_link_online() || !g_status.mpg_mode) return false;
  if (strcmp(g_status.state, "Idle") != 0) return false;
  // Immediate digital-out on: controller should pull PSU K+ to GND when active.
  char cmd[24];
  snprintf(cmd, sizeof(cmd), "M64P%d\r", pendant_cfg_get()->laser_enable_dout);
  send_line(cmd);
  Serial.printf("[grbl] laser enable K+  %s", cmd);
  return true;
}

bool grbl_link_laser_on(float s_cmd) {
  if (!grbl_link_laser_enable()) return false;
  if (!(s_cmd > 0.0f)) s_cmd = 50.0f;
  char cmd[32];
  snprintf(cmd, sizeof(cmd), "M3S%.0f\r", s_cmd);
  send_line(cmd);
  Serial.printf("[grbl] laser on PWM  %s", cmd);
  return true;
}

bool grbl_link_laser_off(void) {
  if (!grbl_link_online()) return false;
  char cmd[24];
  int dout = pendant_cfg_get()->laser_enable_dout;
  snprintf(cmd, sizeof(cmd), "M65P%d\r", dout);
  send_line(cmd);  // open K+ enable (inhibit)
  send_line("M5\r");  // also drop spindle/PWM if armed
  Serial.printf("[grbl] laser inhibit K+  M65P%d + M5\n", dout);
  return true;
}
