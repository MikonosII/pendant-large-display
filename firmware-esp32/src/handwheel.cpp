#include "handwheel.h"
#include "pins_config.h"

#include <Arduino.h>

// Gray-code quadrature (bit0=A, bit1=B). +1 when A leads B (CW).
static const int8_t kQuadTable[16] = {
    0, +1, -1, 0,  // 00→ 01 / 10
    -1, 0, 0, +1,  // 01→
    +1, 0, 0, -1,  // 10→
    0, -1, +1, 0,  // 11→
};

static volatile int32_t g_counts = 0;
static volatile uint8_t g_state = 0;
static bool g_hw = false;

int8_t handwheel_quad_step(uint8_t prev, uint8_t next) {
  prev &= 3;
  next &= 3;
  return kQuadTable[(prev << 2) | next];
}

#if (HANDWHEEL_A_GPIO >= 0) && (HANDWHEEL_B_GPIO >= 0)

static void IRAM_ATTR on_edge() {
  uint8_t a = (uint8_t)digitalRead(HANDWHEEL_A_GPIO);
  uint8_t b = (uint8_t)digitalRead(HANDWHEEL_B_GPIO);
  uint8_t next = (uint8_t)((b << 1) | a);
  int8_t d = handwheel_quad_step(g_state, next);
  if (d) {
    g_counts += d;
    g_state = next;
  }
}

void handwheel_begin(void) {
  pinMode(HANDWHEEL_A_GPIO, INPUT_PULLUP);
  pinMode(HANDWHEEL_B_GPIO, INPUT_PULLUP);
  uint8_t a = (uint8_t)digitalRead(HANDWHEEL_A_GPIO);
  uint8_t b = (uint8_t)digitalRead(HANDWHEEL_B_GPIO);
  g_state = (uint8_t)((b << 1) | a);
  g_counts = 0;
  attachInterrupt(digitalPinToInterrupt(HANDWHEEL_A_GPIO), on_edge, CHANGE);
  attachInterrupt(digitalPinToInterrupt(HANDWHEEL_B_GPIO), on_edge, CHANGE);
  g_hw = true;
  Serial.printf("[mpg] handwheel A=%d B=%d (pull-up, 3.3V)\n", HANDWHEEL_A_GPIO,
                HANDWHEEL_B_GPIO);
}

#else

void handwheel_begin(void) {
  g_hw = false;
  g_counts = 0;
  g_state = 0;
  Serial.println("[mpg] handwheel pins disabled — set HANDWHEEL_A/B_GPIO to wire");
}

#endif

void handwheel_poll(void) {
  // GPIO path is ISR-driven; I2C expander path is polled from i2c_mpg_poll().
}

bool handwheel_hw_present(void) { return g_hw; }

int32_t handwheel_take_delta(void) {
  noInterrupts();
  int32_t d = g_counts;
  g_counts = 0;
  interrupts();
  return d;
}

void handwheel_inject(int32_t counts) {
  if (!counts) return;
  noInterrupts();
  g_counts += counts;
  interrupts();
}
