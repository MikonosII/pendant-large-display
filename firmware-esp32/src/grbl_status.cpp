#include "grbl_status.h"

#include <stdlib.h>
#include <string.h>

static void parse_csv_floats(const char *data, float *out, int n) {
  for (int i = 0; i < n; i++) out[i] = 0;
  const char *p = data;
  for (int i = 0; i < n && p && *p; i++) {
    out[i] = strtof(p, nullptr);
    const char *comma = strchr(p, ',');
    if (!comma) break;
    p = comma + 1;
  }
}

static void parse_csv_ints(const char *data, int *a, int *b, int *c) {
  *a = 100;
  *b = 100;
  *c = 100;
  if (!data) return;
  char buf[48];
  strncpy(buf, data, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';
  char *save = nullptr;
  char *tok = strtok_r(buf, ",", &save);
  if (tok) *a = atoi(tok);
  tok = strtok_r(nullptr, ",", &save);
  if (tok) *b = atoi(tok);
  tok = strtok_r(nullptr, ",", &save);
  if (tok) *c = atoi(tok);
}

bool grbl_parse_status_line(const char *line_in, grbl_status_t *out) {
  if (!line_in || !out) return false;

  char line[192];
  strncpy(line, line_in, sizeof(line) - 1);
  line[sizeof(line) - 1] = '\0';

  size_t n = strlen(line);
  while (n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r' || line[n - 1] == ' ')) {
    line[--n] = '\0';
  }
  if (n > 0 && line[n - 1] == '>') line[--n] = '\0';

  char *p = line;
  if (*p == '<') p++;
  if (!*p) return false;

  char *save = nullptr;
  char *field = strtok_r(p, "|", &save);
  if (!field) return false;

  strncpy(out->state, field, sizeof(out->state) - 1);
  out->state[sizeof(out->state) - 1] = '\0';
  char *colon = strchr(out->state, ':');
  if (colon) *colon = '\0';

  bool got_pos = false;
  float pos[GRBL_AXIS_CNT] = {0, 0, 0};

  while ((field = strtok_r(nullptr, "|", &save)) != nullptr) {
    if (!strncmp(field, "WPos:", 5)) {
      parse_csv_floats(field + 5, pos, GRBL_AXIS_CNT);
      out->use_wpos = true;
      got_pos = true;
    } else if (!strncmp(field, "MPos:", 5)) {
      parse_csv_floats(field + 5, pos, GRBL_AXIS_CNT);
      out->use_wpos = false;
      got_pos = true;
    } else if (!strncmp(field, "WCO:", 4)) {
      parse_csv_floats(field + 4, out->wco, GRBL_AXIS_CNT);
    } else if (!strncmp(field, "FS:", 3)) {
      float fs[3] = {0, 0, 0};
      parse_csv_floats(field + 3, fs, 3);
      out->feed = fs[0];
      out->laser_s_cmd = fs[1];
      out->laser_s_act = fs[2];
    } else if (!strncmp(field, "F:", 2)) {
      out->feed = strtof(field + 2, nullptr);
    } else if (!strncmp(field, "Ov:", 3)) {
      parse_csv_ints(field + 3, &out->feed_ov, &out->rapid_ov, &out->speed_ov);
    } else if (!strncmp(field, "MPG:", 4)) {
      out->mpg_mode = (field[4] == '1');
    }
  }

  if (got_pos) {
    if (out->use_wpos) {
      for (int i = 0; i < GRBL_AXIS_CNT; i++) {
        out->wpos[i] = pos[i];
        out->mpos[i] = pos[i] + out->wco[i];
      }
    } else {
      for (int i = 0; i < GRBL_AXIS_CNT; i++) {
        out->mpos[i] = pos[i];
        out->wpos[i] = pos[i] - out->wco[i];
      }
    }
  }

  out->online = true;
  return true;
}
