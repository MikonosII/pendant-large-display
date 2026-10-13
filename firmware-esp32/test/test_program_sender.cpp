// Host unit test for program_sender_prepare_line (no Arduino).
#include "program_sender.h"

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
  char out[160];

  CHECK(!program_sender_prepare_line("; comment only", 1, out, sizeof(out)));
  CHECK(!program_sender_prepare_line("   ", 1, out, sizeof(out)));

  CHECK(program_sender_prepare_line("G1 X10 ; cut", 3, out, sizeof(out)));
  CHECK(std::strcmp(out, "N3G1 X10") == 0);

  CHECK(program_sender_prepare_line("G0 X0", 0, out, sizeof(out)));
  CHECK(std::strcmp(out, "G0 X0") == 0);

  CHECK(program_sender_prepare_line("$H", 5, out, sizeof(out)));
  CHECK(std::strcmp(out, "$H") == 0);

  CHECK(program_sender_prepare_line("G1 X1 (msg) Y2;trail", 9, out, sizeof(out)));
  CHECK(std::strcmp(out, "N9G1 X1 (msg) Y2") == 0);

  std::printf("ok test_program_sender\n");
  return 0;
}
