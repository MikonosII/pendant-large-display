#include "pendant_cfg.h"

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include <SD.h>
#include <SPI.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static pendant_cfg_t g_cfg;
static bool g_lfs = false;
static SPIClass *g_sd_spi = nullptr;
static bool g_sd = false;

static bool read_file_text(fs::FS &fs, const char *path, char *buf, size_t buf_sz) {
  if (!buf || buf_sz < 2) return false;
  File f = fs.open(path, "r");
  if (!f) return false;
  size_t n = f.readBytes(buf, buf_sz - 1);
  f.close();
  buf[n] = 0;
  return n > 0;
}

static bool load_board_file(fs::FS &fs, const char *board_id, pendant_cfg_t *cfg) {
  char path[80];
  snprintf(path, sizeof(path), "/boards/%s.cfg", board_id);
  char *buf = (char *)malloc(4096);
  if (!buf) return false;
  bool ok = read_file_text(fs, path, buf, 4096);
  if (ok) {
    pendant_cfg_parse_text(cfg, buf);
    Serial.printf("[cfg] board file %s\n", path);
  }
  free(buf);
  return ok;
}

static bool mount_sd_with(const pendant_cfg_t *c) {
  if (!c->sd_enable) return false;
  if (!g_sd_spi) {
    g_sd_spi = new SPIClass(HSPI);
  }
  g_sd_spi->begin(c->sd_sck, c->sd_miso, c->sd_mosi, c->sd_cs >= 0 ? c->sd_cs : -1);
  bool ok = false;
  if (c->sd_cs >= 0) {
    ok = SD.begin((uint8_t)c->sd_cs, *g_sd_spi);
  } else {
    pinMode(c->sd_dummy_cs, OUTPUT);
    digitalWrite(c->sd_dummy_cs, HIGH);
    ok = SD.begin((uint8_t)c->sd_dummy_cs, *g_sd_spi);
  }
  return ok;
}

void pendant_cfg_begin(void) {
  pendant_cfg_set_defaults(&g_cfg);

  g_lfs = LittleFS.begin(true);
  if (!g_lfs) {
    Serial.println("[cfg] LittleFS mount failed — using compile defaults");
    pendant_cfg_apply_board(&g_cfg, g_cfg.board_id);
    return;
  }

  // Seed settings.cfg into LittleFS on first boot if missing
  if (!LittleFS.exists("/settings.cfg")) {
    File f = LittleFS.open("/settings.cfg", "w");
    if (f) {
      f.print(
          "# SmartPendant runtime settings — edit and reboot (no rebuild).\n"
          "# Prefer SD card copy at /settings.cfg (wins over this file).\n"
          "# Board pin maps: /boards/<id>.cfg or built-in profiles.\n"
          "\n"
          "board crowpanel-advance-5\n"
          "\n"
          "# Optional overrides (uncomment to change):\n"
          "# laser.enable_dout 0\n"
          "# laser.feed_pct 5.0\n"
          "# laser.s_max 1000\n"
          "# uart.baud 115200\n");
      f.close();
      Serial.println("[cfg] wrote default /settings.cfg to LittleFS");
    }
  }
  if (!LittleFS.exists("/boards")) LittleFS.mkdir("/boards");

  char board_want[48];
  strncpy(board_want, g_cfg.board_id, sizeof(board_want) - 1);
  board_want[sizeof(board_want) - 1] = 0;

  char *setbuf = (char *)malloc(4096);
  if (setbuf && read_file_text(LittleFS, "/settings.cfg", setbuf, 4096)) {
    pendant_cfg_t tmp;
    pendant_cfg_set_defaults(&tmp);
    pendant_cfg_parse_text(&tmp, setbuf);
    if (tmp.board_id[0]) {
      strncpy(board_want, tmp.board_id, sizeof(board_want) - 1);
      board_want[sizeof(board_want) - 1] = 0;
    }
  }

  if (!pendant_cfg_apply_board(&g_cfg, board_want)) {
    Serial.printf("[cfg] unknown board '%s' — keeping crowpanel-advance-5\n", board_want);
    pendant_cfg_apply_board(&g_cfg, "crowpanel-advance-5");
  }
  load_board_file(LittleFS, g_cfg.board_id, &g_cfg);

  if (setbuf) {
    if (setbuf[0]) {
      pendant_cfg_parse_text(&g_cfg, setbuf);
      strncpy(g_cfg.source, "lfs:/settings.cfg", sizeof(g_cfg.source) - 1);
      Serial.println("[cfg] loaded lfs:/settings.cfg");
    }
    free(setbuf);
  }

  g_sd = mount_sd_with(&g_cfg);
  if (g_sd && SD.exists("/settings.cfg")) {
    int prev_sck = g_cfg.sd_sck, prev_miso = g_cfg.sd_miso, prev_mosi = g_cfg.sd_mosi;
    int prev_cs = g_cfg.sd_cs;

    char *sdbuf = (char *)malloc(4096);
    if (sdbuf && read_file_text(SD, "/settings.cfg", sdbuf, 4096)) {
      pendant_cfg_t scratch = g_cfg;
      pendant_cfg_parse_text(&scratch, sdbuf);
      if (strcasecmp(scratch.board_id, g_cfg.board_id) != 0) {
        pendant_cfg_apply_board(&g_cfg, scratch.board_id);
        load_board_file(LittleFS, g_cfg.board_id, &g_cfg);
        load_board_file(SD, g_cfg.board_id, &g_cfg);
      }
      pendant_cfg_parse_text(&g_cfg, sdbuf);
      strncpy(g_cfg.source, "sd:/settings.cfg", sizeof(g_cfg.source) - 1);
      Serial.println("[cfg] SD /settings.cfg applied (wins)");
    }
    free(sdbuf);

    if (g_cfg.sd_sck != prev_sck || g_cfg.sd_miso != prev_miso || g_cfg.sd_mosi != prev_mosi ||
        g_cfg.sd_cs != prev_cs) {
      SD.end();
      g_sd = mount_sd_with(&g_cfg);
      Serial.printf("[cfg] SD remounted after pin change → %s\n", g_sd ? "ok" : "fail");
    }
  } else if (!g_sd) {
    Serial.println("[cfg] SD not mounted — LittleFS/settings only");
  }

  Serial.printf("[cfg] board=%s  uart=IO%d/IO%d@%d  laser dout=P%d  src=%s\n", g_cfg.board_id,
                g_cfg.uart_tx, g_cfg.uart_rx, g_cfg.uart_baud, g_cfg.laser_enable_dout,
                g_cfg.source);
}

const pendant_cfg_t *pendant_cfg_get(void) { return &g_cfg; }

bool pendant_cfg_lfs_mounted(void) { return g_lfs; }
bool pendant_cfg_sd_mounted(void) { return g_sd; }
