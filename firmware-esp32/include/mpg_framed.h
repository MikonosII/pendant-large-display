#pragma once

// ESP32 port of stock FramedUart — stop-and-wait framed MPG transport.
// Byte stream API matches how grbl_link uses UART (1-byte = Rlt, ≥2 = Dat).

#include "mpg_transport.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  uint32_t tx_data;
  uint32_t tx_retry;
  uint32_t rx_ack;
  uint32_t rx_nak;
  uint32_t rx_data;
  uint32_t rx_dup;
  uint32_t tx_ack;
  uint32_t tx_nak;
  uint32_t rx_crc_err;
  uint32_t rx_no_room;
  uint32_t tx_dropped;
  uint32_t link_downs;
} mpg_framed_stats_t;

typedef struct mpg_framed mpg_framed_t;

/** Hardware callbacks: return true on success / byte available. */
typedef bool (*mpg_hw_read_fn)(void *ctx, uint8_t *byte);
typedef bool (*mpg_hw_write_fn)(void *ctx, const uint8_t *data, uint32_t len);
typedef bool (*mpg_hw_tx_idle_fn)(void *ctx);
typedef uint32_t (*mpg_now_ms_fn)(void *ctx);

typedef struct {
  void *ctx;
  mpg_hw_read_fn read;
  mpg_hw_write_fn write;
  mpg_hw_tx_idle_fn tx_idle;  // may be NULL → always idle
  mpg_now_ms_fn now_ms;
} mpg_framed_hw_t;

mpg_framed_t *mpg_framed_create(const mpg_framed_hw_t *hw);
void mpg_framed_destroy(mpg_framed_t *f);
void mpg_framed_reset(mpg_framed_t *f);
void mpg_framed_set_baud(mpg_framed_t *f, uint32_t baud);
void mpg_framed_set_attempts(mpg_framed_t *f, uint32_t attempts);

/** Service RX/TX/ACK; call every poll tick. */
void mpg_framed_service(mpg_framed_t *f);

/** Pop one unframed payload byte. Returns false if empty. */
bool mpg_framed_read(mpg_framed_t *f, uint8_t *byte);

/**
 * Write payload. size==1 → Rlt channel; size≥2 → Dat.
 * Returns false if channel busy (caller should retry).
 */
bool mpg_framed_write(mpg_framed_t *f, const uint8_t *data, uint32_t size);

bool mpg_framed_link_up(const mpg_framed_t *f);
const mpg_framed_stats_t *mpg_framed_stats(const mpg_framed_t *f);

#ifdef __cplusplus
}
#endif
