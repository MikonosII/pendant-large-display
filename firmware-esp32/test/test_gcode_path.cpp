// Host test for gcode_parse_toolpath.
//
//   g++ -std=c++17 -I include -o /tmp/test_gcode_path \
//       test/test_gcode_path.cpp src/gcode_path.cpp && /tmp/test_gcode_path

#include "gcode_path.h"
#include "sample_job_nc.h"

#include <cassert>
#include <cmath>
#include <cstdio>

static bool near(float a, float b) { return fabsf(a - b) < 0.05f; }

int main() {
  path_seg_t segs[512];
  unsigned n = gcode_parse_toolpath(SAMPLE_JOB_NC, segs, 512);
  assert(n >= 10);

  // Sample starts with rapid to 5,5 then cuts the rectangle to 75,5
  bool saw_cut_75 = false;
  bool saw_rapid = false;
  for (unsigned i = 0; i < n; i++) {
    if (!segs[i].cut) saw_rapid = true;
    if (segs[i].cut && near(segs[i].x, 75.0f) && near(segs[i].y, 5.0f)) {
      saw_cut_75 = true;
    }
  }
  assert(saw_rapid);
  assert(saw_cut_75);

  const char *arc_nc = "G90 G21\nG0 X10 Y0\nM3 S500\nG2 X0 Y10 I-10 J0\nM5\n";
  n = gcode_parse_toolpath(arc_nc, segs, 512);
  assert(n > 4);  // linearized arc

  printf("test_gcode_path: ok (%u sample segs)\n",
         gcode_parse_toolpath(SAMPLE_JOB_NC, segs, 512));
  return 0;
}
