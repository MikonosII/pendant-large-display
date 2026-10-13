#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
  GRBL_AXIS_X = 0,
  GRBL_AXIS_Y = 1,
  GRBL_AXIS_Z = 2,
  GRBL_AXIS_CNT = 3,
};

typedef struct {
  char state[16];
  float wpos[GRBL_AXIS_CNT];
  float mpos[GRBL_AXIS_CNT];
  float wco[GRBL_AXIS_CNT];
  float feed;
  float laser_s_cmd;
  float laser_s_act;
  int feed_ov;
  int rapid_ov;
  int speed_ov;
  bool coolant_flood;
  bool coolant_mist;
  bool probe_triggered;  // Pn: contains P
  bool mpg_mode;
  bool use_wpos;
  bool online;
  uint32_t status_age_ms;
  // Last [PRB:…] report (machine coords)
  float probe_mpos[GRBL_AXIS_CNT];
  bool probe_ok;
  bool probe_fresh;
} grbl_status_t;

/** Parse one status line into `out` (leading `<` optional). Pure / host-testable. */
bool grbl_parse_status_line(const char *line, grbl_status_t *out);

/** Parse `[PRB:x,y,z:ok]` into probe fields. */
bool grbl_parse_probe_line(const char *line, grbl_status_t *out);

#ifdef __cplusplus
}
#endif
