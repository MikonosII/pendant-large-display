// Host unit test for runtime settings.cfg parser (no Arduino).
#include "pendant_cfg.h"

#include <cmath>
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
  pendant_cfg_t c;
  pendant_cfg_set_defaults(&c);
  CHECK(c.uart_tx == 20);
  CHECK(c.laser_enable_dout == 0);

  CHECK(pendant_cfg_apply_board(&c, "crowpanel-advance-7"));
  CHECK(std::strcmp(c.board_id, "crowpanel-advance-7") == 0);
  CHECK(c.uart_rx == 19);

  const char *ov =
      "board crowpanel-advance-5\n"
      "laser.enable_dout 3\n"
      "laser.feed_pct 12.5\n"
      "laser.s_max 1000\n"
      "uart.baud 230400\n"
      "i2c_mpg.enable 1\n"
      "i2c_mpg.addr 0x21\n"
      "handwheel.a 4\n"
      "handwheel.b 5\n"
      "# comment\n";

  int n = pendant_cfg_parse_text(&c, ov);
  CHECK(n >= 8);
  CHECK(std::strcmp(c.board_id, "crowpanel-advance-5") == 0);
  CHECK(c.laser_enable_dout == 3);
  CHECK(std::fabs(c.laser_feed_pct - 12.5f) < 0.01f);
  CHECK(c.uart_baud == 230400);
  CHECK(c.i2c_mpg_enable == 1);
  CHECK(c.i2c_mpg_addr == 0x21);
  CHECK(c.handwheel_a == 4);
  CHECK(c.handwheel_b == 5);

  std::printf("ok test_pendant_cfg\n");
  return 0;
}
