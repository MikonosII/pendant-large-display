#include "gcode_path.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *skip_ws(const char *p) {
  while (*p && (*p == ' ' || *p == '\t')) p++;
  return p;
}

static bool read_word(const char **pp, char *letter, float *value) {
  const char *p = skip_ws(*pp);
  if (!*p || !isalpha((unsigned char)*p)) return false;
  *letter = (char)toupper((unsigned char)*p++);
  p = skip_ws(p);
  char *end = nullptr;
  *value = strtof(p, &end);
  if (end == p) return false;
  *pp = end;
  return true;
}

static void strip_comment(char *line) {
  for (char *p = line; *p; p++) {
    if (*p == ';' || *p == '(') {
      *p = '\0';
      return;
    }
  }
}

static void push_seg(path_seg_t *out, unsigned max_out, unsigned *count, float x, float y,
                     bool cut, float *cx, float *cy, bool *started) {
  if (*count >= max_out) return;
  if (!*started) {
    out[(*count)++] = {x, y, false};
    *started = true;
    *cx = x;
    *cy = y;
    return;
  }
  if (fabsf(x - *cx) < 1e-6f && fabsf(y - *cy) < 1e-6f) return;
  out[(*count)++] = {x, y, cut};
  *cx = x;
  *cy = y;
}

static void emit_arc(path_seg_t *out, unsigned max_out, unsigned *count, float x0, float y0,
                     float x1, float y1, float i, float j, bool cw, bool cut, float *cx,
                     float *cy, bool *started) {
  const float acx = x0 + i;
  const float acy = y0 + j;
  const float r0 = hypotf(x0 - acx, y0 - acy);
  if (!(r0 > 1e-6f)) {
    push_seg(out, max_out, count, x1, y1, cut, cx, cy, started);
    return;
  }

  float a0 = atan2f(y0 - acy, x0 - acx);
  float a1 = atan2f(y1 - acy, x1 - acx);
  float da = a1 - a0;
  if (cw) {
    while (da >= 0) da -= (float)(M_PI * 2);
  } else {
    while (da <= 0) da += (float)(M_PI * 2);
  }
  if (hypotf(x1 - x0, y1 - y0) < 1e-6f && hypotf(i, j) > 1e-6f) {
    da = cw ? (float)(-M_PI * 2) : (float)(M_PI * 2);
  }

  const float chord = 0.5f;
  const float arc_len = fabsf(da) * r0;
  int n = (int)ceilf(arc_len / chord);
  if (n < 1) n = 1;
  if (n > 180) n = 180;

  for (int k = 1; k <= n; k++) {
    const float a = a0 + (da * (float)k) / (float)n;
    float px = acx + r0 * cosf(a);
    float py = acy + r0 * sinf(a);
    if (k == n) {
      px = x1;
      py = y1;
    }
    push_seg(out, max_out, count, px, py, cut, cx, cy, started);
    if (*count >= max_out) return;
  }
}

unsigned gcode_parse_toolpath(const char *nc, path_seg_t *out, unsigned max_out) {
  if (!nc || !out || max_out == 0) return 0;

  float x = 0, y = 0;
  bool absolute = true;
  float unit = 1.0f;
  int motion = 0;
  float s = 0;
  bool spindle_on = false;
  bool started = false;
  unsigned count = 0;

  const char *p = nc;
  char line[256];

  while (*p && count < max_out) {
    size_t n = 0;
    while (*p && *p != '\n' && *p != '\r' && n + 1 < sizeof(line)) {
      line[n++] = *p++;
    }
    line[n] = '\0';
    while (*p == '\n' || *p == '\r') p++;

    strip_comment(line);
    const char *lp = skip_ws(line);
    if (!*lp) continue;

    float nx = x, ny = y, i_off = 0, j_off = 0;
    bool has_x = false, has_y = false, has_ij = false;
    char letter;
    float value;

    while (read_word(&lp, &letter, &value)) {
      switch (letter) {
        case 'G': {
          int g = (int)lroundf(value);
          if (g == 0 || g == 1 || g == 2 || g == 3) motion = g;
          else if (g == 90) absolute = true;
          else if (g == 91) absolute = false;
          else if (g == 20) unit = 25.4f;
          else if (g == 21) unit = 1.0f;
          break;
        }
        case 'M': {
          int m = (int)lroundf(value);
          if (m == 3 || m == 4) spindle_on = true;
          else if (m == 5) spindle_on = false;
          break;
        }
        case 'S':
          s = value;
          break;
        case 'X':
          nx = absolute ? value * unit : x + value * unit;
          has_x = true;
          break;
        case 'Y':
          ny = absolute ? value * unit : y + value * unit;
          has_y = true;
          break;
        case 'I':
          i_off = value * unit;
          has_ij = true;
          break;
        case 'J':
          j_off = value * unit;
          has_ij = true;
          break;
        default:
          break;
      }
    }

    const bool moving = has_x || has_y || (has_ij && (motion == 2 || motion == 3));
    if (!moving) continue;

    const bool cutting = (motion != 0) && spindle_on && (s > 0.0f);

    if (motion == 2 || motion == 3) {
      if (!has_ij) {
        push_seg(out, max_out, &count, nx, ny, cutting, &x, &y, &started);
      } else {
        emit_arc(out, max_out, &count, x, y, nx, ny, i_off, j_off, motion == 2, cutting, &x, &y,
                 &started);
      }
    } else {
      push_seg(out, max_out, &count, nx, ny, cutting, &x, &y, &started);
    }
  }

  return count;
}
