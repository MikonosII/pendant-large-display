#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MACRO_MAX
#define MACRO_MAX 4
#endif
#ifndef MACRO_NAME_LEN
#define MACRO_NAME_LEN 14
#endif
#ifndef MACRO_GCODE_LEN
#define MACRO_GCODE_LEN 120
#endif

typedef struct {
  char name[MACRO_NAME_LEN];
  char gcode[MACRO_GCODE_LEN];  // one or more cmds separated by ';'
} macro_t;

/** Load /macros.cfg from SD (preferred) or LittleFS; seed defaults if missing. */
void macros_begin(void);

unsigned macros_count(void);
const macro_t *macros_get(unsigned index);

/** Start sending macro `index` (Idle + MPG when online). */
bool macros_run(unsigned index);

/** Pace multi-command macros on ok. Call from the main loop. */
void macros_poll(void);

bool macros_busy(void);

/**
 * Host-testable parser. `text` is full file contents.
 * Returns number of macros stored (≤ max_out).
 */
unsigned macros_parse_text(const char *text, macro_t *out, unsigned max_out);

#ifdef __cplusplus
}
#endif
