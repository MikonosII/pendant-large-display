#pragma once

#include <stdint.h>

// STC8H1K28 backlight MCU on CrowPanel Advance (I2C 0x30).
void backlight_init();
void backlight_set_percent(uint8_t percent);  // 0–100
