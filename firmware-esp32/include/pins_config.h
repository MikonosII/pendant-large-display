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

// Handwheel quadrature A/B (3.3 V TTL, internal pull-ups enabled).
// CrowPanel Advance has almost no free GPIO after the RGB panel + UART1.
// Leave −1 until wired. Prefer I2C_MPG_ENABLE (PCF8574) when using the TF card
// — IO4/IO5/IO6 are claimed by SD in TF DIP mode.
// Direct GPIO reclaim when TF/speaker are unused: IO4 = A, IO5 = B.
#ifndef HANDWHEEL_A_GPIO
#define HANDWHEEL_A_GPIO -1
#endif
#ifndef HANDWHEEL_B_GPIO
#define HANDWHEEL_B_GPIO -1
#endif
// Counts per detent after 4× decode; stock pendant is ~100 PPR.
#ifndef HANDWHEEL_PPR
#define HANDWHEEL_PPR 100
#endif

// Optional PCF8574 on the touch I2C bus (SDA15/SCL16) for wheel + axis buttons.
#ifndef I2C_MPG_ENABLE
#define I2C_MPG_ENABLE 0
#endif
#ifndef I2C_MPG_ADDR
#define I2C_MPG_ADDR 0x20
#endif

// CrowPanel Advance TF / microSD (SPI). Shares IO4–IO6 with speaker I2S —
// set rear DIP switches to a TF-card mode before mounting.
#ifndef JOB_STORE_SD
#define JOB_STORE_SD 1
#endif
#ifndef SD_SPI_MOSI
#define SD_SPI_MOSI 6
#endif
#ifndef SD_SPI_MISO
#define SD_SPI_MISO 4
#endif
#ifndef SD_SPI_SCK
#define SD_SPI_SCK 5
#endif
// Many Advance boards hard-tie SD CS to 3.3 V (−1). Library still needs a
// GPIO number; SD_SPI_DUMMY_CS is driven high and is not the card's CS.
#ifndef SD_SPI_CS
#define SD_SPI_CS -1
#endif
#ifndef SD_SPI_DUMMY_CS
#define SD_SPI_DUMMY_CS 8
#endif

// Chrome metrics (match Application/ScreenConfig.h profile 2)
#define SCREEN_HEADER_H 56
#define SCREEN_TABS_H 48
#define SCREEN_SOFT_BTN_H 48
