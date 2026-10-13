#include "macros.h"

#include <cstdio>
#include <cstring>

#define CHECK(cond)                                                                        \
  do {                                                                                     \
    if (!(cond)) {                                                                         \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                          \
      return 1;                                                                            \
    }                                                                                      \
  } while (0)

int main() {
  macro_t m[4];
  const char *text =
      "# comment\n"
      "XY0 =\n"
      "Park = G90 G0 X10 Y20\n"
      "Corner =\n"
      "Focus = G90 G0 Z0\n";

  unsigned n = macros_parse_text(text, m, 4);
  CHECK(n == 4);
  CHECK(std::strcmp(m[0].name, "XY0") == 0);
  CHECK(m[0].gcode[0] == 0);
  CHECK(std::strcmp(m[1].name, "Park") == 0);
  CHECK(std::strcmp(m[1].gcode, "G90 G0 X10 Y20") == 0);
  CHECK(std::strcmp(m[2].name, "Corner") == 0);
  CHECK(std::strcmp(m[3].name, "Focus") == 0);

  std::printf("ok test_macros\n");
  return 0;
}
