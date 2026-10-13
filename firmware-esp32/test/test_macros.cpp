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
      "XY0 = G90 G0 X0 Y0\n"
      "Park = G90 G0 X10 Y20 ; G0 Z5\n"
      "\n"
      "BadLine\n"
      "Unlock = $X\n";

  unsigned n = macros_parse_text(text, m, 4);
  CHECK(n == 3);
  CHECK(std::strcmp(m[0].name, "XY0") == 0);
  CHECK(std::strcmp(m[0].gcode, "G90 G0 X0 Y0") == 0);
  CHECK(std::strcmp(m[1].name, "Park") == 0);
  CHECK(std::strstr(m[1].gcode, "G0 Z5") != nullptr);
  CHECK(std::strcmp(m[2].name, "Unlock") == 0);

  std::printf("ok test_macros\n");
  return 0;
}
