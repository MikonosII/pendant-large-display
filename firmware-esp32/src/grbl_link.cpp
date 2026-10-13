#include "grbl_link.h"
#include "pins_config.h"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

// Realtime / legacy bytes (grblHAL protocol.c)
static const uint8_t kCmdStatus = '?';
static const uint8_t kCmdFeedHold = '!';
static const uint8_t kCmdCycleStart = '~';
static const uint8_t kCmdJogCancel = 0x85;
static const uint8_t kCmdMpgToggle = 0x8B;

static const uint8_t kOvBytes[] = {
    0x90,  // FEED_RESET
    0x91,  // FEED_COARSE_PLUS
    0x92,  // FEED_COARSE_MINUS
    0x93,  // FEED_FINE_PLUS
    0x94,  // FEED_FINE_MINUS
    0x95,  // RAPID_RESET
    0x96,  // RAPID_MEDIUM
    0x97,  // RAPID_LOW
    0x99,  // SPINDLE_RESET
    0x9A,  // SPINDLE_COARSE_PLUS
    0x9B,  // SPINDLE_COARSE_MINUS
    0x9C,  // SPINDLE_FINE_PLUS
    0x9D,  // SPINDLE_FINE_MINUS
    0xA0,  // COOLANT_FLOOD
    0xA1,  // COOLANT_MIST
};

static HardwareSerial GrblSerial(GRBL_UART_NUM);

static grbl_status_t g_status;
static char rx_line[192];
static size_t rx_len = 0;
static uint32_t last_status_rx_ms = 0;
static uint32_t last_status_tx_ms = 0;
static uint32_t last_mpg_tx_ms = 0;
static bool want_mpg = true;
static bool ever_seen = false;

static void send_rt(uint8_t b) { GrblSerial.write(b); }

static void send_line(const char *s) {
  GrblSerial.print(s);
  if (s[0] && s[strlen(s) - 1] != '\r') {
    GrblSerial.write('\r');
  }
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

  if (!strcmp(line, "ok") || !strncmp(line, "error:", 6) || !strncmp(line, "ALARM:", 6) ||
      !strncmp(line, "Grbl", 4)) {
    Serial.printf("[grbl] %s\n", line);
  }
}

static void drain_rx() {
  while (GrblSerial.available() > 0) {
    int c = GrblSerial.read();
    if (c < 0) break;
    if (c == '\n' || c == '\r') {
      if (rx_len > 0) {
        rx_line[rx_len] = '\0';
        handle_line(rx_line);
        rx_len = 0;
      }
      continue;
    }
    if (rx_len + 1 < sizeof(rx_line)) {
      rx_line[rx_len++] = (char)c;
    } else {
      rx_len = 0;
    }
  }
}

void grbl_link_begin(void) {
  status_reset_defaults();
  GrblSerial.begin(GRBL_UART_BAUD, SERIAL_8N1, GRBL_UART_RX, GRBL_UART_TX);
  delay(20);
  while (GrblSerial.available()) GrblSerial.read();
  Serial.printf("[grbl] UART%d TX=%d RX=%d @ %d\n", GRBL_UART_NUM, GRBL_UART_TX, GRBL_UART_RX,
                GRBL_UART_BAUD);
  last_status_tx_ms = 0;
  last_status_rx_ms = 0;
  want_mpg = true;
}

void grbl_link_poll(void) {
  drain_rx();

  const uint32_t now = millis();
  g_status.online = ever_seen && (now - last_status_rx_ms) < 800u;
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
