#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "gcode_path.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef JOB_STORE_MAX_FILES
#define JOB_STORE_MAX_FILES 32
#endif

#ifndef JOB_STORE_NAME_LEN
#define JOB_STORE_NAME_LEN 48
#endif

#ifndef JOB_STORE_PATH_LEN
#define JOB_STORE_PATH_LEN 96
#endif

typedef enum {
  JOB_SRC_EMBEDDED = 0,
  JOB_SRC_LITTLEFS,
  JOB_SRC_SD,
} job_src_t;

typedef struct {
  char name[JOB_STORE_NAME_LEN];
  char path[JOB_STORE_PATH_LEN];  // full path or "embedded:sample"
  job_src_t src;
  uint32_t size;
} job_entry_t;

/** Mount LittleFS (+ optional SD). Safe to call once from setup. */
void job_store_begin(void);

bool job_store_lfs_ok(void);
bool job_store_sd_ok(void);

/**
 * List available jobs: embedded sample + /jobs on LittleFS + /jobs on SD.
 * Returns count written to `out` (≤ max_out).
 */
unsigned job_store_list(job_entry_t *out, unsigned max_out);

/**
 * Load NC text into `buf`. Returns true on success; `*out_len` is bytes used
 * (not including a forced trailing NUL — buf is always NUL-terminated).
 */
bool job_store_read(const char *path, char *buf, size_t buf_sz, size_t *out_len);

/**
 * Load and parse a job into a toolpath buffer. Sets name_out when non-null.
 * Returns segment count (0 on failure).
 */
unsigned job_store_load_path(const char *path, path_seg_t *segs, unsigned max_segs,
                             char *name_out, size_t name_sz);

#ifdef __cplusplus
}
#endif
