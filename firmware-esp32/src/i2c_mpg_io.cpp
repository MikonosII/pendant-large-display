#include "i2c_mpg_io.h"
#include "handwheel.h"
#include "pins_config.h"

#include <Arduino.h>
#include <Wire.h>

// Bit masks (active-low buttons / open-collector quad)
enum {
  BIT_A = 0,
  BIT_B = 1,
  BIT_AX = 2,
  BIT_AY = 3,
  BIT_AZ = 4,
  BIT_STEP = 5,
};

static bool g_present = false;
static uint8_t g_prev = 0xFF;
static int g_axis_latched = -1;
static bool g_step_latched = false;

int8_t i2c_mpg_quad_from_port(uint8_t prev, uint8_t next) {
  uint8_t pa =
      (uint8_t)((((prev >> BIT_B) & 1) << 1) | ((prev >> BIT_A) & 1));
  uint8_t na =
      (uint8_t)((((next >> BIT_B) & 1) << 1) | ((next >> BIT_A) & 1));
  return handwheel_quad_step(pa, na);
}

int i2c_mpg_axis_edge(uint8_t prev, uint8_t next) {
  // Falling edge (1→0) = press
  const uint8_t bits[3] = {BIT_AX, BIT_AY, BIT_AZ};
  for (int i = 0; i < 3; i++) {
    uint8_t m = (uint8_t)(1u << bits[i]);
    if ((prev & m) && !(next & m)) return i;
  }
  return -1;
}

bool i2c_mpg_step_edge(uint8_t prev, uint8_t next) {
  uint8_t m = (uint8_t)(1u << BIT_STEP);
  return (prev & m) && !(next & m);
}

#if defined(I2C_MPG_ENABLE) && I2C_MPG_ENABLE

static bool read_port(uint8_t *out) {
  Wire.beginTransmission(I2C_MPG_ADDR);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom((int)I2C_MPG_ADDR, 1) != 1) return false;
  *out = (uint8_t)Wire.read();
  return true;
}

void i2c_mpg_begin(void) {
  g_present = false;
  g_prev = 0xFF;
  g_axis_latched = -1;
  g_step_latched = false;

  Wire.beginTransmission(I2C_MPG_ADDR);
  // Write 0xFF so pins are inputs with weak pull-ups
  Wire.write(0xFF);
  if (Wire.endTransmission() != 0) {
    Serial.printf("[mpg] I2C expander 0x%02X not found\n", I2C_MPG_ADDR);
    return;
  }
  uint8_t v = 0xFF;
  if (!read_port(&v)) {
    Serial.printf("[mpg] I2C expander 0x%02X read failed\n", I2C_MPG_ADDR);
    return;
  }
  g_prev = v;
  g_present = true;
  Serial.printf("[mpg] I2C expander 0x%02X ready (port=0x%02X)\n", I2C_MPG_ADDR, v);
}

void i2c_mpg_poll(void) {
  if (!g_present) return;
  uint8_t v = 0;
  if (!read_port(&v)) return;

  // Accumulate quadrature into the handwheel counter (same path as GPIO ISR)
  int8_t d = i2c_mpg_quad_from_port(g_prev, v);
  if (d) handwheel_inject((int32_t)d);

  int ax = i2c_mpg_axis_edge(g_prev, v);
  if (ax >= 0) g_axis_latched = ax;
  if (i2c_mpg_step_edge(g_prev, v)) g_step_latched = true;

  g_prev = v;
}

#else

void i2c_mpg_begin(void) {
  g_present = false;
  Serial.println("[mpg] I2C expander disabled — set I2C_MPG_ENABLE=1 + PCF8574 @ I2C_MPG_ADDR");
}

void i2c_mpg_poll(void) {}

#endif

bool i2c_mpg_present(void) { return g_present; }

int i2c_mpg_take_axis(void) {
  int a = g_axis_latched;
  g_axis_latched = -1;
  return a;
}

bool i2c_mpg_take_step_cycle(void) {
  bool s = g_step_latched;
  g_step_latched = false;
  return s;
}
