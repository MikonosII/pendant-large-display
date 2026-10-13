// Host tests for framed MPG transport (no Arduino).
//
//   g++ -std=c++17 -I include -o /tmp/test_mpg_framed \
//       test/test_mpg_framed.cpp src/mpg_framed.cpp && /tmp/test_mpg_framed

#include "mpg_framed.h"
#include "mpg_transport.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <deque>
#include <vector>

struct FakeWire {
  std::deque<uint8_t> a_to_b;
  std::deque<uint8_t> b_to_a;
  uint32_t now = 0;
};

static bool read_ab(void *ctx, uint8_t *b) {
  auto *w = (FakeWire *)ctx;
  if (w->a_to_b.empty()) return false;
  *b = w->a_to_b.front();
  w->a_to_b.pop_front();
  return true;
}
static bool write_ab(void *ctx, const uint8_t *d, uint32_t n) {
  auto *w = (FakeWire *)ctx;
  w->a_to_b.insert(w->a_to_b.end(), d, d + n);
  return true;
}
static bool read_ba(void *ctx, uint8_t *b) {
  auto *w = (FakeWire *)ctx;
  if (w->b_to_a.empty()) return false;
  *b = w->b_to_a.front();
  w->b_to_a.pop_front();
  return true;
}
static bool write_ba(void *ctx, const uint8_t *d, uint32_t n) {
  auto *w = (FakeWire *)ctx;
  w->b_to_a.insert(w->b_to_a.end(), d, d + n);
  return true;
}
static uint32_t now_fn(void *ctx) { return ((FakeWire *)ctx)->now; }

/** Minimal peer: ACK every Dat/Rlt, deliver payload to out. */
static void peer_service(FakeWire *w, std::vector<uint8_t> *out) {
  // Parse frames from a_to_b (pendant → peer), write ACK on b_to_a
  while (!w->a_to_b.empty()) {
    if (w->a_to_b.front() != MPG_TRANSPORT_SOF) {
      w->a_to_b.pop_front();
      continue;
    }
    if (w->a_to_b.size() < 6) break;
    uint8_t seq = w->a_to_b[1];
    uint8_t type = w->a_to_b[2];
    uint8_t len = w->a_to_b[3];
    if (w->a_to_b.size() < (size_t)(6 + len)) break;
    std::vector<uint8_t> frame(w->a_to_b.begin(), w->a_to_b.begin() + 6 + len);
    w->a_to_b.erase(w->a_to_b.begin(), w->a_to_b.begin() + 6 + len);
    uint16_t crc = mpg_frame_crc16(frame.data() + 1, 3 + len);
    uint16_t got = (uint16_t)frame[4 + len] | ((uint16_t)frame[5 + len] << 8);
    assert(crc == got);
    if (type == MpgFrame_Dat || type == MpgFrame_Rlt) {
      out->insert(out->end(), frame.begin() + 4, frame.begin() + 4 + len);
      uint8_t ack[16];
      uint8_t pl = type;
      size_t n = mpg_frame_build(ack, seq, MpgFrame_Ack, &pl, 1);
      w->b_to_a.insert(w->b_to_a.end(), ack, ack + n);
    }
  }
}

int main() {
  // CRC / build smoke
  uint8_t buf[32];
  const uint8_t payload[] = {'?', };
  size_t n = mpg_frame_build(buf, 0, MpgFrame_Rlt, payload, 1);
  assert(n == 7);
  assert(buf[0] == 0x02 && buf[1] == 0 && buf[2] == MpgFrame_Rlt && buf[3] == 1);
  assert(mpg_frame_crc16(buf + 1, 4) ==
         ((uint16_t)buf[5] | ((uint16_t)buf[6] << 8)));

  FakeWire wire;
  mpg_framed_hw_t hw = {&wire, read_ba, write_ab, nullptr, now_fn};
  mpg_framed_t *f = mpg_framed_create(&hw);
  assert(f);

  // Send realtime '?'
  assert(mpg_framed_write(f, payload, 1));
  std::vector<uint8_t> delivered;
  peer_service(&wire, &delivered);
  assert(delivered.size() == 1 && delivered[0] == '?');
  mpg_framed_service(f);  // consume ACK — any CRC-valid frame marks link up
  assert(mpg_framed_link_up(f));
  // Send a Dat status line from peer → pendant
  {
    const uint8_t line[] = {'<', 'I', 'd', 'l', 'e', '>', '\n'};
    uint8_t frame[64];
    size_t fn = mpg_frame_build(frame, 0, MpgFrame_Dat, line, sizeof(line));
    wire.b_to_a.insert(wire.b_to_a.end(), frame, frame + fn);
  }
  mpg_framed_service(f);
  uint8_t b;
  char got[16] = {};
  size_t gi = 0;
  while (mpg_framed_read(f, &b) && gi + 1 < sizeof(got)) got[gi++] = (char)b;
  assert(strcmp(got, "<Idle>\n") == 0);

  mpg_framed_destroy(f);
  puts("test_mpg_framed: ok");
  return 0;
}
