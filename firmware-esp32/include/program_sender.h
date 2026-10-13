#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PROG_IDLE = 0,
  PROG_RUNNING,
  PROG_HOLD,
  PROG_DRAINING,  // all lines sent; waiting for controller Idle
  PROG_DONE,
  PROG_ERROR,
} prog_state_t;

/**
 * Prepare a job for streaming (memory if it fits, else line-by-line from FS).
 * Does not start sending. Path is job_store style (`embedded:sample` or `/jobs/…`).
 */
bool program_sender_load(const char *path);

/** Begin streaming from the start of the loaded job. Needs Idle (+ MPG when online). */
bool program_sender_start(void);

/** Feed hold (`!`) and pause sending. */
void program_sender_hold(void);

/** Cycle start (`~`) and resume sending. */
void program_sender_resume(void);

/** Stop sending + feed hold. Does not soft-reset the controller. */
void program_sender_stop(void);

/** Drive the state machine — call from the main loop. */
void program_sender_poll(void);

prog_state_t program_sender_state(void);
uint32_t program_sender_sent_lines(void);
uint32_t program_sender_total_lines(void);  // 0 if unknown (streaming file)
const char *program_sender_job_name(void);
const char *program_sender_message(void);

/**
 * Host-testable: strip `;` comments, trim, optionally prepend `N<n>`.
 * Returns false if nothing should be sent (empty / comment-only).
 * `line_no` 0 → do not prepend N.
 */
bool program_sender_prepare_line(const char *raw, uint32_t line_no, char *out, size_t out_sz);

#ifdef __cplusplus
}
#endif
