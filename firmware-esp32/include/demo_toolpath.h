#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  float x;
  float y;
  bool cut;
} path_seg_t;

// Demo laser job (mm) — replaced later by a parsed NC/LightBurn export.
static const path_seg_t DEMO_TOOLPATH[] = {
    {5, 5, false},  {5, 5, true},   {75, 5, true},  {75, 45, true},
    {5, 45, true},  {5, 5, true},   {40, 10, false}, {55, 25, true},
    {40, 40, true}, {25, 25, true}, {40, 10, true}, {30, 25, false},
    {50, 25, true}, {40, 15, false}, {40, 35, true},
};

static const unsigned DEMO_TOOLPATH_LEN =
    sizeof(DEMO_TOOLPATH) / sizeof(DEMO_TOOLPATH[0]);
