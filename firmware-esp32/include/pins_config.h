#pragma once

// CrowPanel Advance 7" — native RGB panel size (GW1076-i2)
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

// Touch I2C (shared with STC8H1K28 backlight MCU)
#define TOUCH_SDA 15
#define TOUCH_SCL 16
#define TOUCH_I2C_ADDR 0x5D
#define BL_I2C_ADDR 0x30

// grblHAL link — CrowPanel Advance UART1-OUT (PH2.0-4P)
// ESP32 TX → controller RX, ESP32 RX ← controller TX (3.3 V TTL)
#define GRBL_UART_NUM 1
#define GRBL_UART_TX 20
#define GRBL_UART_RX 19
#define GRBL_UART_BAUD 115200

// Chrome metrics (match Application/ScreenConfig.h profile 2)
#define SCREEN_HEADER_H 56
#define SCREEN_TABS_H 48
#define SCREEN_SOFT_BTN_H 48
