#include "mpg_framed.h"

#include <stdlib.h>
#include <string.h>

#ifndef MPG_FRAMED_RX_BUF
#define MPG_FRAMED_RX_BUF 256
#endif

static const uint8_t RESYNC_MARKER = 0x18;
static const uint8_t CMD_LOST_MARKER = 0x15;
static const uint32_t DEFAULT_ATTEMPTS = 4;
static const uint32_t ACK_TIMEOUT_MARGIN_MS = 10;
static const uint32_t ACK_TIMEOUT_MIN_MS = 20;
static const uint32_t CTRL_QUEUE_SIZE = 4;
static const uint32_t RT_FRAME_SIZE = 1 + MPG_TRANSPORT_OVERHEAD;
static const uint32_t CTRL_FRAME_SIZE = 1 + MPG_TRANSPORT_OVERHEAD;

enum rx_state_t { RX_SOF, RX_SEQ, RX_TYPE, RX_LEN, RX_PAYLOAD, RX_CRC_LO, RX_CRC_HI };

typedef struct {
  uint8_t *frame;
  uint32_t capacity;
  uint32_t size;
  uint8_t seq;
  uint8_t type;
  bool report_loss;
  uint32_t tx_timestamp;
  uint32_t attempts;
  bool in_flight;
  bool pending_tx;
} tx_channel_t;

typedef struct {
  uint8_t seq;
  uint8_t type;
  uint8_t payload;
} ctrl_frame_t;

struct mpg_framed {
  mpg_framed_hw_t hw;
  tx_channel_t cmd_ch;
  tx_channel_t rt_ch;
  uint8_t cmd_frame[MPG_TRANSPORT_MAX_FRAME];
  uint8_t rt_frame[RT_FRAME_SIZE];

  rx_state_t rx_state;
  uint8_t rx_seq, rx_type, rx_len;
  uint8_t rx_payload[MPG_TRANSPORT_MAX_PAYLOAD];
  uint32_t rx_payload_idx;
  uint16_t rx_crc_calc, rx_crc_frame;
  uint8_t rx_last_seq;
  bool rx_last_seq_valid;
  uint32_t attempts_limit;
  bool cmd_loss_pending;
  bool rx_resync_pending;

  uint8_t rx_data_buf[MPG_FRAMED_RX_BUF];
  uint32_t rx_data_head, rx_data_tail;

  ctrl_frame_t ctrl_queue[CTRL_QUEUE_SIZE];
  uint32_t ctrl_head, ctrl_tail;
  uint8_t ctrl_frame[CTRL_FRAME_SIZE];

  bool link_up;
  uint32_t baud;
  uint32_t ack_timeout_ms;
  mpg_framed_stats_t stats;
};

static uint8_t next_seq(uint8_t seq) {
  seq++;
  if (!seq) seq++;
  return seq;
}

static uint32_t calc_ack_timeout(uint32_t baud) {
  if (baud < 1200) baud = 1200;
  uint32_t frame_ms = ((MPG_TRANSPORT_MAX_FRAME * 10u * 1000u) / baud) + 1u;
  uint32_t t = (frame_ms * 2u) + ACK_TIMEOUT_MARGIN_MS;
  if (t < ACK_TIMEOUT_MIN_MS) t = ACK_TIMEOUT_MIN_MS;
  return t;
}

static uint32_t now_ms(mpg_framed_t *f) { return f->hw.now_ms ? f->hw.now_ms(f->hw.ctx) : 0; }

static bool tx_idle(mpg_framed_t *f) {
  return !f->hw.tx_idle || f->hw.tx_idle(f->hw.ctx);
}

static uint32_t rx_free(mpg_framed_t *f) {
  uint32_t used = (f->rx_data_head - f->rx_data_tail + MPG_FRAMED_RX_BUF) % MPG_FRAMED_RX_BUF;
  return MPG_FRAMED_RX_BUF - used - 1u;
}

static bool push_rx(mpg_framed_t *f, const uint8_t *data, uint32_t size, bool resync) {
  if ((size + (resync ? 1u : 0u)) > rx_free(f)) return false;
  if (resync) {
    f->rx_data_buf[f->rx_data_head] = RESYNC_MARKER;
    f->rx_data_head = (f->rx_data_head + 1u) % MPG_FRAMED_RX_BUF;
  }
  for (uint32_t i = 0; i < size; i++) {
    f->rx_data_buf[f->rx_data_head] = data[i];
    f->rx_data_head = (f->rx_data_head + 1u) % MPG_FRAMED_RX_BUF;
  }
  return true;
}

static void pump_tx(mpg_framed_t *f);
static void retransmit_or_drop(mpg_framed_t *f, tx_channel_t *ch);

static void send_ctrl(mpg_framed_t *f, uint8_t type, uint8_t seq, uint8_t payload) {
  uint32_t used = (f->ctrl_head - f->ctrl_tail + CTRL_QUEUE_SIZE) % CTRL_QUEUE_SIZE;
  if ((used + 1u) >= CTRL_QUEUE_SIZE) return;
  f->ctrl_queue[f->ctrl_head].seq = seq;
  f->ctrl_queue[f->ctrl_head].type = type;
  f->ctrl_queue[f->ctrl_head].payload = payload;
  f->ctrl_head = (f->ctrl_head + 1u) % CTRL_QUEUE_SIZE;
  if (type == MpgFrame_Ack) f->stats.tx_ack++;
  else f->stats.tx_nak++;
  pump_tx(f);
}

static void handle_ack_nak(mpg_framed_t *f, tx_channel_t *ch, bool is_ack, uint8_t seq) {
  if (!ch->in_flight) return;
  if (is_ack) {
    if (seq == ch->seq) {
      ch->in_flight = false;
      ch->seq = next_seq(ch->seq);
    }
  } else {
    retransmit_or_drop(f, ch);
    pump_tx(f);
  }
}

static bool resync_needed(const mpg_framed_t *f, uint8_t seq) {
  return f->rx_resync_pending || (f->rx_last_seq_valid && (seq != next_seq(f->rx_last_seq)));
}

static void process_rx_frame(mpg_framed_t *f) {
  if (f->rx_crc_frame != f->rx_crc_calc) {
    f->stats.rx_crc_err++;
    send_ctrl(f, MpgFrame_Nak, 0, MpgFrame_Unk);
    return;
  }
  f->link_up = true;
  switch (f->rx_type) {
    case MpgFrame_Dat:
      if (f->rx_last_seq_valid && f->rx_seq == f->rx_last_seq) {
        f->stats.rx_dup++;
        send_ctrl(f, MpgFrame_Ack, f->rx_seq, MpgFrame_Dat);
      } else if (push_rx(f, f->rx_payload, f->rx_len, resync_needed(f, f->rx_seq))) {
        f->rx_last_seq = f->rx_seq;
        f->rx_last_seq_valid = true;
        f->rx_resync_pending = false;
        f->stats.rx_data++;
        send_ctrl(f, MpgFrame_Ack, f->rx_seq, MpgFrame_Dat);
      } else {
        f->stats.rx_no_room++;
        send_ctrl(f, MpgFrame_Nak, f->rx_seq, MpgFrame_Dat);
      }
      break;
    case MpgFrame_Ack:
    case MpgFrame_Nak: {
      uint8_t acked = (f->rx_len == 1) ? f->rx_payload[0] : (uint8_t)MpgFrame_Unk;
      bool is_ack = (f->rx_type == MpgFrame_Ack);
      if (is_ack) f->stats.rx_ack++;
      else f->stats.rx_nak++;
      if (acked == MpgFrame_Rlt) handle_ack_nak(f, &f->rt_ch, is_ack, f->rx_seq);
      else if (acked == MpgFrame_Dat) handle_ack_nak(f, &f->cmd_ch, is_ack, f->rx_seq);
      else if (!is_ack) {
        handle_ack_nak(f, &f->cmd_ch, false, f->cmd_ch.seq);
        handle_ack_nak(f, &f->rt_ch, false, f->rt_ch.seq);
      }
      break;
    }
    default:
      break;
  }
}

static void process_rx_byte(mpg_framed_t *f, uint8_t b) {
  switch (f->rx_state) {
    case RX_SOF:
      if (b == MPG_TRANSPORT_SOF) {
        f->rx_crc_calc = 0xFFFF;
        f->rx_state = RX_SEQ;
      }
      break;
    case RX_SEQ:
      f->rx_seq = b;
      f->rx_crc_calc = mpg_frame_crc16_add(f->rx_crc_calc, b);
      f->rx_state = RX_TYPE;
      break;
    case RX_TYPE:
      f->rx_type = b;
      f->rx_crc_calc = mpg_frame_crc16_add(f->rx_crc_calc, b);
      f->rx_state = RX_LEN;
      break;
    case RX_LEN:
      f->rx_len = b;
      f->rx_crc_calc = mpg_frame_crc16_add(f->rx_crc_calc, b);
      f->rx_payload_idx = 0;
      if (f->rx_len > MPG_TRANSPORT_MAX_PAYLOAD) f->rx_state = RX_SOF;
      else f->rx_state = f->rx_len ? RX_PAYLOAD : RX_CRC_LO;
      break;
    case RX_PAYLOAD:
      f->rx_payload[f->rx_payload_idx++] = b;
      f->rx_crc_calc = mpg_frame_crc16_add(f->rx_crc_calc, b);
      if (f->rx_payload_idx >= f->rx_len) f->rx_state = RX_CRC_LO;
      break;
    case RX_CRC_LO:
      f->rx_crc_frame = b;
      f->rx_state = RX_CRC_HI;
      break;
    case RX_CRC_HI:
      f->rx_crc_frame |= (uint16_t)b << 8;
      process_rx_frame(f);
      f->rx_state = RX_SOF;
      break;
    default:
      f->rx_state = RX_SOF;
      break;
  }
}

static void retransmit_or_drop(mpg_framed_t *f, tx_channel_t *ch) {
  if (ch->attempts < f->attempts_limit) {
    ch->attempts++;
    ch->tx_timestamp = now_ms(f);
    ch->pending_tx = true;
    f->stats.tx_retry++;
  } else {
    ch->in_flight = false;
    ch->pending_tx = false;
    ch->seq = next_seq(ch->seq);
    f->stats.tx_dropped++;
    if (ch->report_loss) f->cmd_loss_pending = true;
    if (f->link_up) {
      f->link_up = false;
      f->stats.link_downs++;
      f->rx_resync_pending = true;
    }
  }
}

static void service_channel(mpg_framed_t *f, tx_channel_t *ch) {
  if (ch->in_flight && (now_ms(f) - ch->tx_timestamp >= f->ack_timeout_ms)) {
    retransmit_or_drop(f, ch);
  }
}

static void pump_tx(mpg_framed_t *f) {
  if (!tx_idle(f)) return;
  if (f->ctrl_tail != f->ctrl_head) {
    uint8_t payload = f->ctrl_queue[f->ctrl_tail].payload;
    uint32_t size = (uint32_t)mpg_frame_build(
        f->ctrl_frame, f->ctrl_queue[f->ctrl_tail].seq,
        (mpg_frame_type_t)f->ctrl_queue[f->ctrl_tail].type, &payload, 1);
    f->ctrl_tail = (f->ctrl_tail + 1u) % CTRL_QUEUE_SIZE;
    f->hw.write(f->hw.ctx, f->ctrl_frame, size);
  } else if (f->rt_ch.pending_tx) {
    f->rt_ch.pending_tx = false;
    f->hw.write(f->hw.ctx, f->rt_ch.frame, f->rt_ch.size);
  } else if (f->cmd_ch.pending_tx) {
    f->cmd_ch.pending_tx = false;
    f->hw.write(f->hw.ctx, f->cmd_ch.frame, f->cmd_ch.size);
  }
}

static bool send_on_channel(mpg_framed_t *f, tx_channel_t *ch, uint8_t type, const uint8_t *payload,
                            uint8_t len) {
  if (ch->in_flight) return false;
  if ((uint32_t)len + MPG_TRANSPORT_OVERHEAD > ch->capacity) return false;
  ch->size = (uint32_t)mpg_frame_build(ch->frame, ch->seq, (mpg_frame_type_t)type, payload, len);
  ch->type = type;
  ch->attempts = 1;
  ch->tx_timestamp = now_ms(f);
  ch->in_flight = true;
  ch->pending_tx = true;
  f->stats.tx_data++;
  pump_tx(f);
  return true;
}

mpg_framed_t *mpg_framed_create(const mpg_framed_hw_t *hw) {
  if (!hw || !hw->read || !hw->write || !hw->now_ms) return nullptr;
  mpg_framed_t *f = (mpg_framed_t *)calloc(1, sizeof(mpg_framed_t));
  if (!f) return nullptr;
  f->hw = *hw;
  f->cmd_ch.frame = f->cmd_frame;
  f->cmd_ch.capacity = sizeof(f->cmd_frame);
  f->cmd_ch.report_loss = true;
  f->rt_ch.frame = f->rt_frame;
  f->rt_ch.capacity = sizeof(f->rt_frame);
  f->attempts_limit = DEFAULT_ATTEMPTS;
  f->baud = 115200;
  f->ack_timeout_ms = calc_ack_timeout(f->baud);
  mpg_framed_reset(f);
  return f;
}

void mpg_framed_destroy(mpg_framed_t *f) { free(f); }

void mpg_framed_reset(mpg_framed_t *f) {
  if (!f) return;
  f->rx_state = RX_SOF;
  f->rx_payload_idx = 0;
  f->rx_last_seq_valid = false;
  f->rx_resync_pending = false;
  f->cmd_loss_pending = false;
  f->rx_data_head = f->rx_data_tail = 0;
  f->ctrl_head = f->ctrl_tail = 0;
  f->cmd_ch.in_flight = f->cmd_ch.pending_tx = false;
  f->cmd_ch.seq = 0;
  f->rt_ch.in_flight = f->rt_ch.pending_tx = false;
  f->rt_ch.seq = 0;
  f->link_up = false;
}

void mpg_framed_set_baud(mpg_framed_t *f, uint32_t baud) {
  if (!f) return;
  f->baud = baud ? baud : 115200;
  f->ack_timeout_ms = calc_ack_timeout(f->baud);
}

void mpg_framed_set_attempts(mpg_framed_t *f, uint32_t attempts) {
  if (!f) return;
  if (attempts < 1) attempts = 1;
  if (attempts > 20) attempts = 20;
  f->attempts_limit = attempts;
}

void mpg_framed_service(mpg_framed_t *f) {
  if (!f) return;
  uint8_t b;
  while (f->hw.read(f->hw.ctx, &b)) process_rx_byte(f, b);
  service_channel(f, &f->cmd_ch);
  service_channel(f, &f->rt_ch);
  if (f->cmd_loss_pending) {
    uint8_t marker = CMD_LOST_MARKER;
    if (push_rx(f, &marker, 1, false)) f->cmd_loss_pending = false;
  }
  pump_tx(f);
}

bool mpg_framed_read(mpg_framed_t *f, uint8_t *byte) {
  if (!f || !byte) return false;
  if (f->rx_data_tail == f->rx_data_head) return false;
  *byte = f->rx_data_buf[f->rx_data_tail];
  f->rx_data_tail = (f->rx_data_tail + 1u) % MPG_FRAMED_RX_BUF;
  return true;
}

bool mpg_framed_write(mpg_framed_t *f, const uint8_t *data, uint32_t size) {
  if (!f || !data || size == 0 || size > MPG_TRANSPORT_MAX_PAYLOAD) return false;
  mpg_framed_service(f);
  if (size == 1) return send_on_channel(f, &f->rt_ch, MpgFrame_Rlt, data, 1);
  return send_on_channel(f, &f->cmd_ch, MpgFrame_Dat, data, (uint8_t)size);
}

bool mpg_framed_link_up(const mpg_framed_t *f) { return f && f->link_up; }

const mpg_framed_stats_t *mpg_framed_stats(const mpg_framed_t *f) {
  return f ? &f->stats : nullptr;
}
