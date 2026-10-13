// Host-side unit test for grbl_parse_status_line (links src/grbl_status.cpp).
//
//   g++ -std=c++17 -I include -o /tmp/test_grbl_status \
//       test/test_grbl_status.cpp src/grbl_status.cpp && /tmp/test_grbl_status

#include "grbl_status.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>

static bool near(float a, float b) { return fabsf(a - b) < 0.001f; }

int main() {
  grbl_status_t st{};
  assert(grbl_parse_status_line(
      "<Idle|WPos:40.000,25.000,0.000|FS:1200.0,650.0,648.0|Ov:100,100,100|MPG:1>", &st));
  assert(strcmp(st.state, "Idle") == 0);
  assert(near(st.wpos[0], 40.0f) && near(st.wpos[1], 25.0f));
  assert(near(st.feed, 1200.0f));
  assert(near(st.laser_s_cmd, 650.0f));
  assert(near(st.laser_s_act, 648.0f));
  assert(st.feed_ov == 100 && st.rapid_ov == 100 && st.speed_ov == 100);
  assert(st.mpg_mode);
  assert(st.use_wpos);

  memset(&st, 0, sizeof(st));
  assert(grbl_parse_status_line("<Run:1|MPos:10,20,30|WCO:1,2,3|F:100|MPG:0>\n", &st));
  assert(strcmp(st.state, "Run") == 0);
  assert(near(st.mpos[0], 10.0f));
  assert(near(st.wpos[0], 9.0f));
  assert(!st.mpg_mode);

  memset(&st, 0, sizeof(st));
  assert(grbl_parse_status_line("<Idle|WPos:0,0,0|Ov:110,50,90|A:SFM|MPG:1>", &st));
  assert(st.feed_ov == 110 && st.rapid_ov == 50 && st.speed_ov == 90);
  assert(st.coolant_flood && st.coolant_mist);
  assert(!st.probe_triggered);

  memset(&st, 0, sizeof(st));
  assert(grbl_parse_status_line("<Idle|WPos:0,0,0|Pn:P|MPG:1>", &st));
  assert(st.probe_triggered);

  memset(&st, 0, sizeof(st));
  assert(grbl_parse_probe_line("[PRB:12.345,6.700,-1.250:1]", &st));
  assert(near(st.probe_mpos[0], 12.345f));
  assert(near(st.probe_mpos[1], 6.700f));
  assert(near(st.probe_mpos[2], -1.250f));
  assert(st.probe_ok && st.probe_fresh);

  memset(&st, 0, sizeof(st));
  assert(grbl_parse_probe_line("PRB:1,2,3:0\n", &st));
  assert(near(st.probe_mpos[0], 1.0f));
  assert(!st.probe_ok && st.probe_fresh);

  puts("test_grbl_status: ok");
  return 0;
}
