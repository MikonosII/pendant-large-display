#include "pendant_cfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// Compile-time fallbacks when user_settings.h is available (firmware).
// Host unit tests pass -DPENDANT_CFG_HOST_TEST and supply their own seeds.
#ifndef PENDANT_CFG_HOST_TEST
#include "user_settings.h"
#else
#ifndef GRBL_UART_NUM
#define GRBL_UART_NUM 1
#define GRBL_UART_TX 20
#define GRBL_UART_RX 19
#define GRBL_UART_BAUD 115200
#define LASER_ENABLE_DOUT 0
#define LASER_FEED_PCT 5.0f
#define LASER_S_MAX 1000.0f
#define MACHINE_BED_X_MM 400.0f
#define MACHINE_BED_Y_MM 300.0f
#define HANDWHEEL_A_GPIO -1
#define HANDWHEEL_B_GPIO -1
#define HANDWHEEL_PPR 100
#define I2C_MPG_ENABLE 0
#define I2C_MPG_ADDR 0x20
#define JOB_STORE_SD 1
#define SD_SPI_MOSI 6
#define SD_SPI_MISO 4
#define SD_SPI_SCK 5
#define SD_SPI_CS -1
#define SD_SPI_DUMMY_CS 8
#endif
#endif

void pendant_cfg_set_defaults(pendant_cfg_t *c) {
  if (!c) return;
  memset(c, 0, sizeof(*c));
  strncpy(c->board_id, "crowpanel-advance-5", sizeof(c->board_id) - 1);
  strncpy(c->board_name, "CrowPanel Advance 5\" (ST7262)", sizeof(c->board_name) - 1);
  strncpy(c->source, "defaults", sizeof(c->source) - 1);

  c->uart_num = GRBL_UART_NUM;
  c->uart_tx = GRBL_UART_TX;
  c->uart_rx = GRBL_UART_RX;
  c->uart_baud = GRBL_UART_BAUD;

  c->laser_enable_dout = LASER_ENABLE_DOUT;
  c->laser_feed_pct = LASER_FEED_PCT;
  c->laser_s_max = LASER_S_MAX;

  c->bed_x_mm = MACHINE_BED_X_MM;
  c->bed_y_mm = MACHINE_BED_Y_MM;

  c->handwheel_a = HANDWHEEL_A_GPIO;
  c->handwheel_b = HANDWHEEL_B_GPIO;
  c->handwheel_ppr = HANDWHEEL_PPR;

  c->i2c_mpg_enable = I2C_MPG_ENABLE;
  c->i2c_mpg_addr = I2C_MPG_ADDR;

  c->sd_enable = JOB_STORE_SD;
  c->sd_mosi = SD_SPI_MOSI;
  c->sd_miso = SD_SPI_MISO;
  c->sd_sck = SD_SPI_SCK;
  c->sd_cs = SD_SPI_CS;
  c->sd_dummy_cs = SD_SPI_DUMMY_CS;
}

static const char *kBoardCrowpanel5 =
    "name CrowPanel Advance 5\" (ST7262)\n"
    "uart.num 1\n"
    "uart.tx 20\n"
    "uart.rx 19\n"
    "uart.baud 115200\n"
    "laser.enable_dout 0\n"
    "laser.feed_pct 5.0\n"
    "laser.s_max 1000\n"
    "machine.bed_x 400\n"
    "machine.bed_y 300\n"
    "handwheel.a -1\n"
    "handwheel.b -1\n"
    "handwheel.ppr 100\n"
    "i2c_mpg.enable 0\n"
    "i2c_mpg.addr 0x20\n"
    "sd.enable 1\n"
    "sd.mosi 6\n"
    "sd.miso 4\n"
    "sd.sck 5\n"
    "sd.cs -1\n"
    "sd.dummy_cs 8\n";

static const char *kBoardCrowpanel7 =
    "name CrowPanel Advance 7\" (GW1076-i2)\n"
    "uart.num 1\n"
    "uart.tx 20\n"
    "uart.rx 19\n"
    "uart.baud 115200\n"
    "laser.enable_dout 0\n"
    "laser.feed_pct 5.0\n"
    "laser.s_max 1000\n"
    "machine.bed_x 400\n"
    "machine.bed_y 300\n"
    "handwheel.a -1\n"
    "handwheel.b -1\n"
    "handwheel.ppr 100\n"
    "i2c_mpg.enable 0\n"
    "i2c_mpg.addr 0x20\n"
    "sd.enable 1\n"
    "sd.mosi 6\n"
    "sd.miso 4\n"
    "sd.sck 5\n"
    "sd.cs -1\n"
    "sd.dummy_cs 8\n";

static char *trim(char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\r') s++;
  char *e = s + strlen(s);
  while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) {
    *--e = 0;
  }
  return s;
}

static bool parse_int(const char *v, int *out) {
  if (!v || !*v || !out) return false;
  char *end = nullptr;
  long n = strtol(v, &end, 0);
  if (end == v) return false;
  *out = (int)n;
  return true;
}

static bool parse_float(const char *v, float *out) {
  if (!v || !*v || !out) return false;
  char *end = nullptr;
  float f = strtof(v, &end);
  if (end == v) return false;
  *out = f;
  return true;
}

int pendant_cfg_parse_text(pendant_cfg_t *cfg, const char *text) {
  if (!cfg || !text) return 0;
  int applied = 0;
  const char *p = text;
  char line[160];

  while (*p) {
    size_t n = 0;
    while (*p && *p != '\n' && n + 1 < sizeof(line)) line[n++] = *p++;
    line[n] = 0;
    if (*p == '\n') p++;

    char *s = trim(line);
    if (!s[0] || s[0] == '#' || s[0] == ';') continue;

    char *key = s;
    char *val = s;
    while (*val && *val != ' ' && *val != '\t' && *val != '=') val++;
    if (!*val) continue;
    *val++ = 0;
    val = trim(val);
    key = trim(key);
    if (!key[0] || !val[0]) continue;

    int iv = 0;
    float fv = 0.0f;

    if (!strcasecmp(key, "board")) {
      strncpy(cfg->board_id, val, sizeof(cfg->board_id) - 1);
      cfg->board_id[sizeof(cfg->board_id) - 1] = 0;
      applied++;
    } else if (!strcasecmp(key, "name")) {
      strncpy(cfg->board_name, val, sizeof(cfg->board_name) - 1);
      cfg->board_name[sizeof(cfg->board_name) - 1] = 0;
      applied++;
    } else if (!strcasecmp(key, "uart.num") && parse_int(val, &iv)) {
      cfg->uart_num = iv;
      applied++;
    } else if (!strcasecmp(key, "uart.tx") && parse_int(val, &iv)) {
      cfg->uart_tx = iv;
      applied++;
    } else if (!strcasecmp(key, "uart.rx") && parse_int(val, &iv)) {
      cfg->uart_rx = iv;
      applied++;
    } else if (!strcasecmp(key, "uart.baud") && parse_int(val, &iv)) {
      cfg->uart_baud = iv;
      applied++;
    } else if (!strcasecmp(key, "laser.enable_dout") && parse_int(val, &iv)) {
      cfg->laser_enable_dout = iv;
      applied++;
    } else if (!strcasecmp(key, "laser.feed_pct") && parse_float(val, &fv)) {
      cfg->laser_feed_pct = fv;
      applied++;
    } else if (!strcasecmp(key, "laser.s_max") && parse_float(val, &fv)) {
      cfg->laser_s_max = fv;
      applied++;
    } else if (!strcasecmp(key, "machine.bed_x") && parse_float(val, &fv)) {
      cfg->bed_x_mm = fv;
      applied++;
    } else if (!strcasecmp(key, "machine.bed_y") && parse_float(val, &fv)) {
      cfg->bed_y_mm = fv;
      applied++;
    } else if (!strcasecmp(key, "handwheel.a") && parse_int(val, &iv)) {
      cfg->handwheel_a = iv;
      applied++;
    } else if (!strcasecmp(key, "handwheel.b") && parse_int(val, &iv)) {
      cfg->handwheel_b = iv;
      applied++;
    } else if (!strcasecmp(key, "handwheel.ppr") && parse_int(val, &iv)) {
      cfg->handwheel_ppr = iv;
      applied++;
    } else if (!strcasecmp(key, "i2c_mpg.enable") && parse_int(val, &iv)) {
      cfg->i2c_mpg_enable = iv ? 1 : 0;
      applied++;
    } else if (!strcasecmp(key, "i2c_mpg.addr") && parse_int(val, &iv)) {
      cfg->i2c_mpg_addr = iv;
      applied++;
    } else if (!strcasecmp(key, "sd.enable") && parse_int(val, &iv)) {
      cfg->sd_enable = iv ? 1 : 0;
      applied++;
    } else if (!strcasecmp(key, "sd.mosi") && parse_int(val, &iv)) {
      cfg->sd_mosi = iv;
      applied++;
    } else if (!strcasecmp(key, "sd.miso") && parse_int(val, &iv)) {
      cfg->sd_miso = iv;
      applied++;
    } else if (!strcasecmp(key, "sd.sck") && parse_int(val, &iv)) {
      cfg->sd_sck = iv;
      applied++;
    } else if (!strcasecmp(key, "sd.cs") && parse_int(val, &iv)) {
      cfg->sd_cs = iv;
      applied++;
    } else if (!strcasecmp(key, "sd.dummy_cs") && parse_int(val, &iv)) {
      cfg->sd_dummy_cs = iv;
      applied++;
    }
  }
  return applied;
}

bool pendant_cfg_apply_board(pendant_cfg_t *cfg, const char *board_id) {
  if (!cfg || !board_id || !board_id[0]) return false;
  const char *body = nullptr;
  if (!strcasecmp(board_id, "crowpanel-advance-5") || !strcasecmp(board_id, "advance-5") ||
      !strcasecmp(board_id, "5")) {
    body = kBoardCrowpanel5;
    strncpy(cfg->board_id, "crowpanel-advance-5", sizeof(cfg->board_id) - 1);
  } else if (!strcasecmp(board_id, "crowpanel-advance-7") ||
             !strcasecmp(board_id, "crowpanel-advance-7-v13") ||
             !strcasecmp(board_id, "advance-7") || !strcasecmp(board_id, "7")) {
    body = kBoardCrowpanel7;
    strncpy(cfg->board_id, "crowpanel-advance-7", sizeof(cfg->board_id) - 1);
  } else {
    return false;
  }
  pendant_cfg_parse_text(cfg, body);
  return true;
}
