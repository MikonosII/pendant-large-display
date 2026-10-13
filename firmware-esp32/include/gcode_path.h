#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float x;
  float y;
  bool cut;
} path_seg_t;

#ifndef GCODE_PATH_MAX_SEGS
#define GCODE_PATH_MAX_SEGS 4096
#endif

typedef struct {
  path_seg_t *segs;
  unsigned count;
  unsigned capacity;
} gcode_path_t;

/** Parse GRBL/LightBurn-ish NC into XY segments (mm). Returns segment count. */
unsigned gcode_parse_toolpath(const char *nc, path_seg_t *out, unsigned max_out);

#ifdef __cplusplus
}
#endif
