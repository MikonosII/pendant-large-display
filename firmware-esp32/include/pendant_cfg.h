#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Runtime board / pin / laser settings. Loaded from filesystem — no rebuild. */
typedef struct {
  char board_id[48];
  char board_name[64];
  char source[48];  // e.g. "sd:/settings.cfg" or "defaults"

  int uart_num;
  int uart_tx;
  int uart_rx;
  int uart_baud;

  int laser_enable_dout;
  float laser_feed_pct;
  float laser_s_max;

  float bed_x_mm;  // work envelope for MPG live map
  float bed_y_mm;

  int handwheel_a;
  int handwheel_b;
  int handwheel_ppr;

  int i2c_mpg_enable;
  int i2c_mpg_addr;  // 7-bit

  int sd_enable;
  int sd_mosi;
  int sd_miso;
  int sd_sck;
  int sd_cs;
  int sd_dummy_cs;
} pendant_cfg_t;

/** Seed compile-time fallbacks (also used by host tests). */
void pendant_cfg_set_defaults(pendant_cfg_t *cfg);

/** Compile-time fallbacks, then board profile, then settings.cfg (SD wins). */
void pendant_cfg_begin(void);

const pendant_cfg_t *pendant_cfg_get(void);

/** True after pendant_cfg_begin mounted these filesystems. */
bool pendant_cfg_lfs_mounted(void);
bool pendant_cfg_sd_mounted(void);

/**
 * Parse a settings / board .cfg text into `cfg` (merge: only keys present change).
 * Host-testable. Returns number of keys applied.
 */
int pendant_cfg_parse_text(pendant_cfg_t *cfg, const char *text);

/** Apply a named built-in board profile. Returns false if unknown. */
bool pendant_cfg_apply_board(pendant_cfg_t *cfg, const char *board_id);

#ifdef __cplusplus
}
#endif
