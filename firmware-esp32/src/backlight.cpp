#include "backlight.h"
#include "pins_config.h"
#include <Arduino.h>
#include <Wire.h>

static void bl_write(uint8_t cmd) {
  Wire.beginTransmission(BL_I2C_ADDR);
  Wire.write(cmd);
  Wire.endTransmission();
}

void backlight_init() {
  Wire.setClock(100000);
  delay(20);

#if ADVANCE_BL_V12
  // v1.2: 0x05=off … 0x10=max
  bl_write(0x10);
#else
  // v1.3+: wake, configure, then 0x00=max … 0xF5=off
  bl_write(0x19);
  delay(10);
  bl_write(0x10);
  delay(10);
  bl_write(0x18);
  delay(80);
  bl_write(0x00);
  delay(10);
  bl_write(0xF7);  // buzzer off
#endif
  Serial.println("[bl] STC8H1K28 backlight on");
}

void backlight_set_percent(uint8_t percent) {
  if (percent > 100) percent = 100;
#if ADVANCE_BL_V12
  // Map 0–100 → 0x05–0x10
  uint8_t cmd = 0x05 + (percent * (0x10 - 0x05)) / 100;
  bl_write(cmd);
#else
  // Map 0–100 → 0xF5–0x00 (inverted)
  uint8_t cmd = 0xF5 - (percent * 0xF5) / 100;
  bl_write(cmd);
#endif
}
