#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 100 PPR-style quadrature handwheel for MPG jog.
 *
 * CrowPanel Advance has almost no spare GPIO after the RGB panel. Pins default
 * to disabled (−1). When you wire A/B (3.3 V), set HANDWHEEL_A_GPIO /
 * HANDWHEEL_B_GPIO in pins_config.h — typical reclaim when TF/speaker are off:
 * IO4 / IO5 (see DIP switch table on the board back).
 *
 * Without pins, handwheel_inject() still works for lab / host tests.
 */

/** Init GPIO + ISR (no-op if pins disabled). Call once after Serial. */
void handwheel_begin(void);

/** Poll / latch ISR counter. Call from the main loop. */
void handwheel_poll(void);

/** True when A/B GPIOs are configured. */
bool handwheel_hw_present(void);

/**
 * Consume accumulated quadrature counts since last call.
 * Positive = CW (convention: +jog on active axis).
 */
int32_t handwheel_take_delta(void);

/** Inject counts without hardware (simulator / host tests). */
void handwheel_inject(int32_t counts);

/**
 * Pure quadrature step: given previous 2-bit state and new 2-bit state
 * (bit0=A, bit1=B), return −1 / 0 / +1. Host-testable.
 */
int8_t handwheel_quad_step(uint8_t prev, uint8_t next);

#ifdef __cplusplus
}
#endif
