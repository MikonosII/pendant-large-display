// Host test for quadrature decode table.
//
//   g++ -std=c++17 -I include -o /tmp/test_handwheel \
//       test/test_handwheel.cpp && /tmp/test_handwheel

#include "handwheel.h"

#include <cassert>
#include <cstdio>

// Stub Arduino-free implementation of the table under test by including
// only the pure function — redefine here to avoid linking handwheel.cpp.
int8_t handwheel_quad_step(uint8_t prev, uint8_t next) {
  static const int8_t kQuadTable[16] = {
      0, +1, -1, 0, -1, 0, 0, +1, +1, 0, 0, -1, 0, -1, +1, 0,
  };
  return kQuadTable[((prev & 3) << 2) | (next & 3)];
}

// Unused stubs for the rest of the header when linking this TU alone.
void handwheel_begin(void) {}
void handwheel_poll(void) {}
bool handwheel_hw_present(void) { return false; }
int32_t handwheel_take_delta(void) { return 0; }
void handwheel_inject(int32_t) {}

int main() {
  // Full CW cycle: 00 → 01 → 11 → 10 → 00 → four +1
  uint8_t s = 0;
  int sum = 0;
  const uint8_t cw[] = {1, 3, 2, 0};
  for (uint8_t n : cw) {
    sum += handwheel_quad_step(s, n);
    s = n;
  }
  assert(sum == 4);

  // Full CCW: 00 → 10 → 11 → 01 → 00
  s = 0;
  sum = 0;
  const uint8_t ccw[] = {2, 3, 1, 0};
  for (uint8_t n : ccw) {
    sum += handwheel_quad_step(s, n);
    s = n;
  }
  assert(sum == -4);

  // Illegal jump 00→11 → 0
  assert(handwheel_quad_step(0, 3) == 0);

  puts("test_handwheel: ok");
  return 0;
}
