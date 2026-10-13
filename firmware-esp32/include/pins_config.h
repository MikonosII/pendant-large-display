#pragma once

// Board-fixed layout for CrowPanel Advance. Tunable wiring / laser / UART /
// handwheel / SD live in user_settings.h — edit that file, not this one.
#include "user_settings.h"

// CrowPanel Advance 5″ (ST7262) and 7″ (GW1076-i2) share native 800×480 RGB.
// Select panel timing via PANEL_ST7262 / PANEL_GW1076 in platformio.ini.
#define LCD_H_RES 800
#define LCD_V_RES 480

// After setRotation(1) / PORTRAIT_MODE the LVGL canvas is 480×800
#if PORTRAIT_MODE
#define UI_H_RES 480
#define UI_V_RES 800
#else
#define UI_H_RES LCD_H_RES
#define UI_V_RES LCD_V_RES
#endif

// Touch I2C (shared with STC8H1K28 backlight MCU) — fixed on Advance boards
#define TOUCH_SDA 15
#define TOUCH_SCL 16
#define TOUCH_I2C_ADDR 0x5D
#define BL_I2C_ADDR 0x30

// Chrome metrics (match Application/ScreenConfig.h profile 2)
#define SCREEN_HEADER_H 56
#define SCREEN_TABS_H 48
#define SCREEN_SOFT_BTN_H 48
