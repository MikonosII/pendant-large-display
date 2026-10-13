#include "macros.h"

#include <string.h>

static char *trim(char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\r') s++;
  char *e = s + strlen(s);
  while (e > s && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) {
    *--e = 0;
  }
  return s;
}

unsigned macros_parse_text(const char *text, macro_t *out, unsigned max_out) {
  if (!text || !out || max_out == 0) return 0;
  unsigned n = 0;
  const char *p = text;
  char line[160];

  while (*p && n < max_out) {
    size_t i = 0;
    while (*p && *p != '\n' && i + 1 < sizeof(line)) line[i++] = *p++;
    line[i] = 0;
    if (*p == '\n') p++;

    char *s = trim(line);
    if (!s[0] || s[0] == '#' || s[0] == ';') continue;

    char *eq = strchr(s, '=');
    if (!eq) continue;
    *eq = 0;
    char *name = trim(s);
    char *gcode = trim(eq + 1);
    if (!name[0]) continue;

    memset(&out[n], 0, sizeof(out[n]));
    strncpy(out[n].name, name, MACRO_NAME_LEN - 1);
    if (gcode[0]) strncpy(out[n].gcode, gcode, MACRO_GCODE_LEN - 1);
    n++;
  }
  return n;
}
