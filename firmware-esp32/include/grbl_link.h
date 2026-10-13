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

/** Relative jog on one axis. Sends `$J=G21G91…` when Idle/Jog + MPG. */
bool grbl_link_jog(int axis, float distance_mm, float feed_mm_min);

/** Zero work position for one axis: `G90G10L20P0X0`. */
bool grbl_link_zero_axis(int axis);

#ifdef __cplusplus
}
#endif
