#pragma once

#include "grbl_status.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Open UART1 @ 115200 (CrowPanel UART1-OUT). Safe to call once. */
void grbl_link_begin(void);

/** Drain RX, parse lines, poll `?`, retry MPG claim. Call from loop. */
void grbl_link_poll(void);

/** Copy latest status snapshot. Returns true if a report arrived since boot. */
bool grbl_link_get_status(grbl_status_t *out);

/** True when a status report was seen within the last ~800 ms. */
bool grbl_link_online(void);

/** Request / release MPG mode via realtime 0x8B. */
void grbl_link_set_mpg(bool want_control);

/** Soft feed hold (`!`). */
void grbl_link_feed_hold(void);

/** Cancel jog (0x85). */
void grbl_link_jog_cancel(void);

/** Relative jog on one axis. Sends `$J=G21G91…` when Idle/Jog + MPG. */
bool grbl_link_jog(int axis, float distance_mm, float feed_mm_min);

/** Zero work position for one axis: `G90G10L20P0X0`. */
bool grbl_link_zero_axis(int axis);

/** grblHAL realtime override bytes (0x90–0xA1). */
typedef enum {
  GRBL_OV_FEED_RESET = 0,
  GRBL_OV_FEED_COARSE_PLUS,   // +10%
  GRBL_OV_FEED_COARSE_MINUS,  // −10%
  GRBL_OV_FEED_FINE_PLUS,     // +1%
  GRBL_OV_FEED_FINE_MINUS,    // −1%
  GRBL_OV_RAPID_RESET,        // 100%
  GRBL_OV_RAPID_MEDIUM,       // 50%
  GRBL_OV_RAPID_LOW,          // 25%
  GRBL_OV_SPINDLE_RESET,
  GRBL_OV_SPINDLE_COARSE_PLUS,
  GRBL_OV_SPINDLE_COARSE_MINUS,
  GRBL_OV_SPINDLE_FINE_PLUS,
  GRBL_OV_SPINDLE_FINE_MINUS,
  GRBL_OV_COOLANT_FLOOD,
  GRBL_OV_COOLANT_MIST,
} grbl_ov_t;

/** Send one realtime override command. */
void grbl_link_override(grbl_ov_t cmd);

#ifdef __cplusplus
}
#endif
