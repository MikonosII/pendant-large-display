#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef MACRO_MAX
#define MACRO_MAX 8
#endif
#ifndef MACRO_COLS
#define MACRO_COLS 4
#endif
#ifndef MACRO_NAME_LEN
#define MACRO_NAME_LEN 14
#endif
#ifndef MACRO_GCODE_LEN
#define MACRO_GCODE_LEN 120
#endif

typedef struct {
  char name[MACRO_NAME_LEN];   // edit in /macros.cfg (left of '=')
  char gcode[MACRO_GCODE_LEN]; // taught position or hand-written g-code
} macro_t;

/** Load /macros.cfg from SD (preferred) or LittleFS; seed defaults if missing. */
void macros_begin(void);

/** Always MACRO_MAX after begin (empty slots padded). */
unsigned macros_count(void);
const macro_t *macros_get(unsigned index);

/** Start sending macro `index` (Idle + MPG when online). Empty gcode → false. */
bool macros_run(unsigned index);

/** Pace multi-command macros on ok. Call from the main loop. */
void macros_poll(void);

bool macros_busy(void);

/**
 * Teach mode: long-press a button → jog → tap same button to capture WPos.
 * Saves `G90 G0 X… Y… Z…` under that slot's name into /macros.cfg.
 */
bool macros_teach_begin(unsigned index);
bool macros_teach_capture(float x, float y, float z);
void macros_teach_cancel(void);
int macros_teach_slot(void);  // −1 if not teaching

/** Persist current slots to SD (preferred) and/or LittleFS. */
bool macros_save(void);

/**
 * Host-testable parser. `text` is full file contents.
 * Returns number of macros stored (≤ max_out). Empty gcode after '=' is OK.
 */
unsigned macros_parse_text(const char *text, macro_t *out, unsigned max_out);

#ifdef __cplusplus
}
#endif
