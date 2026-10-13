#include "i2c_mpg_io.h"
#include "handwheel.h"
#include "pendant_cfg.h"

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
static int g_addr = 0x20;

int8_t i2c_mpg_quad_from_port(uint8_t prev, uint8_t next) {
  uint8_t pa = (uint8_t)((((prev >> BIT_B) & 1) << 1) | ((prev >> BIT_A) & 1));
  uint8_t na = (uint8_t)((((next >> BIT_B) & 1) << 1) | ((next >> BIT_A) & 1));
  return handwheel_quad_step(pa, na);
}

int i2c_mpg_axis_edge(uint8_t prev, uint8_t next) {
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

static bool read_port(uint8_t *out) {
  Wire.beginTransmission(g_addr);
  if (Wire.endTransmission() != 0) return false;
  if (Wire.requestFrom(g_addr, 1) != 1) return false;
  *out = (uint8_t)Wire.read();
  return true;
}

void i2c_mpg_begin(void) {
  g_present = false;
  g_prev = 0xFF;
  g_axis_latched = -1;
  g_step_latched = false;
  g_addr = pendant_cfg_get()->i2c_mpg_addr;

  if (!pendant_cfg_get()->i2c_mpg_enable) {
    Serial.println("[mpg] I2C expander disabled — set i2c_mpg.enable 1 in settings.cfg");
    return;
  }

  Wire.beginTransmission(g_addr);
  Wire.write(0xFF);
  if (Wire.endTransmission() != 0) {
    Serial.printf("[mpg] I2C expander 0x%02X not found\n", g_addr);
    return;
  }
  uint8_t v = 0xFF;
  if (!read_port(&v)) {
    Serial.printf("[mpg] I2C expander 0x%02X read failed\n", g_addr);
    return;
  }
  g_prev = v;
  g_present = true;
  Serial.printf("[mpg] I2C expander 0x%02X ready (port=0x%02X)\n", g_addr, v);
}

void i2c_mpg_poll(void) {
  if (!g_present) return;
  uint8_t v = 0;
  if (!read_port(&v)) return;

  int8_t d = i2c_mpg_quad_from_port(g_prev, v);
  if (d) handwheel_inject((int32_t)d);

  int ax = i2c_mpg_axis_edge(g_prev, v);
  if (ax >= 0) g_axis_latched = ax;
  if (i2c_mpg_step_edge(g_prev, v)) g_step_latched = true;

  g_prev = v;
}

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
