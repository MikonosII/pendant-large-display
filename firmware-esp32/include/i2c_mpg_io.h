#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Optional PCF8574 (or compatible) on the CrowPanel touch I2C bus (SDA15/SCL16).
 *
 * CrowPanel Advance has almost no free GPIO after the RGB panel + UART1, and
 * TF-card mode claims IO4/IO5/IO6. An I2C expander is the practical path for a
 * handwheel + axis-select buttons without fighting the DIP mux.
 *
 * Default bit map (active-low inputs with PCF8574 weak pull-ups written high):
 *   P0 = handwheel A
 *   P1 = handwheel B
 *   P2 = axis X select
 *   P3 = axis Y select
 *   P4 = axis Z select
 *   P5 = step-size cycle
 *
 * Disabled by default (`I2C_MPG_ENABLE=0`). Set address / enable in pins_config.
 */

/** Init expander (no-op if disabled or not found). Call after Wire is up. */
void i2c_mpg_begin(void);

/** Poll expander: feed handwheel counts + latched button edges. */
void i2c_mpg_poll(void);

bool i2c_mpg_present(void);

/** Consume axis-select press: returns 0/1/2 or −1 if none. */
int i2c_mpg_take_axis(void);

/** Consume step-cycle press: true once per click. */
bool i2c_mpg_take_step_cycle(void);

/**
 * Pure decode helpers (host-testable).
 * `prev`/`next` are 8-bit port snapshots (1 = released / high).
 */
int8_t i2c_mpg_quad_from_port(uint8_t prev, uint8_t next);
int i2c_mpg_axis_edge(uint8_t prev, uint8_t next);     // −1 none, else 0..2
bool i2c_mpg_step_edge(uint8_t prev, uint8_t next);

#ifdef __cplusplus
}
#endif
