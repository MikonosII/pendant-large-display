#pragma once

// Wire format from grblHAL Plugin_mpg_transport / stock FramedUart.h
// Keep byte-identical to the controller plugin.

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MPG_TRANSPORT_SOF 0x02

#ifndef MPG_TRANSPORT_MAX_PAYLOAD
#define MPG_TRANSPORT_MAX_PAYLOAD 128
#endif

#define MPG_TRANSPORT_OVERHEAD 6
#define MPG_TRANSPORT_MAX_FRAME (MPG_TRANSPORT_MAX_PAYLOAD + MPG_TRANSPORT_OVERHEAD)

typedef enum {
  MpgFrame_Unk = 0x00,
  MpgFrame_Dat = 0x01,
  MpgFrame_Rlt = 0x02,
  MpgFrame_Ack = 0x03,
  MpgFrame_Nak = 0x04
} mpg_frame_type_t;

static inline uint16_t mpg_frame_crc16_add(uint16_t crc, uint8_t c) {
  uint_fast8_t bits = 8;
  crc ^= (uint16_t)c << 8;
  do {
    crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  } while (--bits);
  return crc;
}

static inline uint16_t mpg_frame_crc16(const uint8_t *data, size_t length) {
  uint16_t crc = 0xFFFF;
  while (length--) crc = mpg_frame_crc16_add(crc, *data++);
  return crc;
}

/** Assemble frame into buf; returns total size. CRC over seq..payload. */
static inline size_t mpg_frame_build(uint8_t *buf, uint8_t seq, mpg_frame_type_t type,
                                     const uint8_t *payload, uint8_t len) {
  size_t i;
  uint16_t crc = 0xFFFF;
  buf[0] = MPG_TRANSPORT_SOF;
  buf[1] = seq;
  buf[2] = (uint8_t)type;
  buf[3] = len;
  for (i = 0; i < len; i++) buf[4 + i] = payload[i];
  for (i = 1; i < (size_t)(4 + len); i++) crc = mpg_frame_crc16_add(crc, buf[i]);
  buf[4 + len] = (uint8_t)(crc & 0xFF);
  buf[5 + len] = (uint8_t)(crc >> 8);
  return (size_t)len + MPG_TRANSPORT_OVERHEAD;
}

#ifdef __cplusplus
}
#endif
