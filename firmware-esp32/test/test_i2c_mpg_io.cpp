// Host-side unit test for i2c_mpg pure decode helpers (no Arduino / Wire).
//
//   g++ -std=c++17 -I include -o /tmp/test_i2c_mpg_io \
//       test/test_i2c_mpg_io.cpp && /tmp/test_i2c_mpg_io

#include "i2c_mpg_io.h"
#include "handwheel.h"

#include <cassert>
#include <cstdio>

int8_t handwheel_quad_step(uint8_t prev, uint8_t next) {
  static const int8_t kQuadTable[16] = {
      0, +1, -1, 0, -1, 0, 0, +1, +1, 0, 0, -1, 0, -1, +1, 0,
  };
  return kQuadTable[((prev & 3) << 2) | (next & 3)];
}

// Mirror of src/i2c_mpg_io.cpp pure helpers (keep in sync).
int8_t i2c_mpg_quad_from_port(uint8_t prev, uint8_t next) {
  uint8_t pa = (uint8_t)((((prev >> 1) & 1) << 1) | ((prev >> 0) & 1));
  uint8_t na = (uint8_t)((((next >> 1) & 1) << 1) | ((next >> 0) & 1));
  return handwheel_quad_step(pa, na);
}

int i2c_mpg_axis_edge(uint8_t prev, uint8_t next) {
  const uint8_t bits[3] = {2, 3, 4};
  for (int i = 0; i < 3; i++) {
    uint8_t m = (uint8_t)(1u << bits[i]);
    if ((prev & m) && !(next & m)) return i;
  }
  return -1;
}

bool i2c_mpg_step_edge(uint8_t prev, uint8_t next) {
  uint8_t m = (uint8_t)(1u << 5);
  return (prev & m) && !(next & m);
}

void i2c_mpg_begin(void) {}
void i2c_mpg_poll(void) {}
bool i2c_mpg_present(void) { return false; }
int i2c_mpg_take_axis(void) { return -1; }
bool i2c_mpg_take_step_cycle(void) { return false; }

int main() {
  uint8_t prev = 0xFC;  // A=0 B=0, others high
  uint8_t next = 0xFD;  // A=1 B=0
  assert(i2c_mpg_quad_from_port(prev, next) == +1);

  prev = 0xFF;
  next = (uint8_t)(prev & ~(1u << 2));
  assert(i2c_mpg_axis_edge(prev, next) == 0);

  prev = 0xFF;
  next = (uint8_t)(prev & ~(1u << 5));
  assert(i2c_mpg_step_edge(prev, next));

  prev = 0xFF;
  next = (uint8_t)(prev & ~(1u << 3));
  assert(i2c_mpg_axis_edge(prev, next) == 1);

  puts("test_i2c_mpg_io: ok");
  return 0;
}
