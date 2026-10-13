#pragma once
// =============================================================================
// USER SETTINGS — edit this file, then rebuild (pio run -e crowpanel-advance-5)
// See WIRING.md for how these map to CrowPanel / grblHAL / laser PSU terminals.
// Board-fixed pins (LCD, touch) live in pins_config.h — leave those alone.
// =============================================================================

// -----------------------------------------------------------------------------
// UART ↔ grblHAL controller (CrowPanel Advance UART1-OUT, PH2.0-4P)
// ESP32 TX → controller RX, ESP32 RX ← controller TX. 3.3 V TTL only.
// -----------------------------------------------------------------------------
#ifndef GRBL_UART_NUM
#define GRBL_UART_NUM 1
#endif
#ifndef GRBL_UART_TX
#define GRBL_UART_TX 20
#endif
#ifndef GRBL_UART_RX
#define GRBL_UART_RX 19
#endif
#ifndef GRBL_UART_BAUD
#define GRBL_UART_BAUD 115200
#endif

// -----------------------------------------------------------------------------
// Laser PSU — controller side (pendant never wires to the PSU directly)
//
//   Controller spindle PWM  →  PSU IN   (power)
//   Controller digital out  →  closes K+ to GND when ACTIVE (enable / arm)
//   Controller GND          →  PSU G
//
// LASER_ENABLE_DOUT = grblHAL digital-output index n for M64 Pn / M65 Pn.
// Map that n to a physical pin (relay / open-drain FET) in your grblHAL board
// map so ACTIVE shorts PSU K+ to G (or K−). M65 opens the path (inhibit).
// -----------------------------------------------------------------------------
#ifndef LASER_ENABLE_DOUT
#define LASER_ENABLE_DOUT 0
#endif

// Default FEED-tab laser power in percent (0.1% resolution in the UI).
#ifndef LASER_FEED_PCT
#define LASER_FEED_PCT 5.0f
#endif

// Matches grblHAL $30 (max spindle/laser S). 100% on the FEED tab → this S.
#ifndef LASER_S_MAX
#define LASER_S_MAX 1000.0f
#endif

// -----------------------------------------------------------------------------
// Handwheel quadrature A/B (3.3 V TTL, pull-ups on). −1 = unused.
// CrowPanel has almost no free GPIO after RGB + UART1. Prefer I2C expander
// when the TF card claims IO4/IO5/IO6. Direct reclaim if TF/speaker unused:
//   HANDWHEEL_A_GPIO 4, HANDWHEEL_B_GPIO 5
// -----------------------------------------------------------------------------
#ifndef HANDWHEEL_A_GPIO
#define HANDWHEEL_A_GPIO -1
#endif
#ifndef HANDWHEEL_B_GPIO
#define HANDWHEEL_B_GPIO -1
#endif
// Counts per detent after 4× decode; stock pendant ~100 PPR.
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

// -----------------------------------------------------------------------------
// Job store — CrowPanel TF / microSD (SPI). Rear DIP must be in a TF mode.
// Shares IO4–IO6 with speaker I2S. Many boards hard-tie SD CS to 3.3 V (−1).
// -----------------------------------------------------------------------------
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
#ifndef SD_SPI_CS
#define SD_SPI_CS -1
#endif
#ifndef SD_SPI_DUMMY_CS
#define SD_SPI_DUMMY_CS 8
#endif
