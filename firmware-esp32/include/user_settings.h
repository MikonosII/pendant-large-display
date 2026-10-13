#pragma once
// =============================================================================
// COMPILE-TIME FALLBACKS only — used if LittleFS/SD settings.cfg is missing.
//
// End users: do NOT edit this to change pins. Edit the runtime file instead:
//
//   SD card (preferred):   /settings.cfg
//   or LittleFS:           /settings.cfg  (also under data/ for uploadfs)
//   board pin maps:        /boards/<id>.cfg
//
// See WIRING.md and data/settings.cfg. Reboot after edits — no rebuild.
// =============================================================================

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

#ifndef LASER_ENABLE_DOUT
#define LASER_ENABLE_DOUT 0
#endif
#ifndef LASER_FEED_PCT
#define LASER_FEED_PCT 5.0f
#endif
#ifndef LASER_S_MAX
#define LASER_S_MAX 1000.0f
#endif

// Work envelope (mm) for the MPG live WPos map
#ifndef MACHINE_BED_X_MM
#define MACHINE_BED_X_MM 400.0f
#endif
#ifndef MACHINE_BED_Y_MM
#define MACHINE_BED_Y_MM 300.0f
#endif

#ifndef HANDWHEEL_A_GPIO
#define HANDWHEEL_A_GPIO -1
#endif
#ifndef HANDWHEEL_B_GPIO
#define HANDWHEEL_B_GPIO -1
#endif
#ifndef HANDWHEEL_PPR
#define HANDWHEEL_PPR 100
#endif

#ifndef I2C_MPG_ENABLE
#define I2C_MPG_ENABLE 0
#endif
#ifndef I2C_MPG_ADDR
#define I2C_MPG_ADDR 0x20
#endif

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
