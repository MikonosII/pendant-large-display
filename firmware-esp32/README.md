# SmartPendant Large — CrowPanel Advance 7″ firmware

ESP32-S3 port of the SmartPendant large-screen fork for the **Elecrow CrowPanel Advance 7.0″ HMI**.

| Item | Value |
|------|--------|
| Board | CrowPanel Advance 7″ |
| MCU | ESP32-S3-WROOM-1-N16R8 |
| Display IC | **GW1076-i2** (IPS RGB 800×480) |
| Touch | GT911 @ I2C `0x5D` |
| Backlight MCU | STC8H1K28 @ I2C `0x30` |
| UI orientation | **Portrait 480×800** (`PORTRAIT_MODE=1`) |

This is a different chip stack than the original STM32F411 + ILI9488 SmartPendant. The STM32 tree at the repo root remains as the upstream-layout reference; **this folder is what you flash to the CrowPanel**.

## Build & flash

```bash
cd firmware-esp32
pio run -e crowpanel-advance-7-v13
pio run -e crowpanel-advance-7-v13 -t upload
pio device monitor -b 115200
```

Use `crowpanel-advance-7-v12` if your board is an older Advance revision (different backlight command range).

### Requirements

* [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
* USB-C data cable to the CrowPanel
* First build downloads the ESP32 Arduino/IDF platform — expect a long first compile

## What works in this slice

* GW1076 RGB panel bring-up via LovyanGFX (Advance pin map / timing)
* GT911 capacitive touch
* STC8H1K28 backlight on
* SmartPendant-style chrome: header tabs, DRO, step sizes, MPG toggle, soft buttons
* **Plain ASCII UART ↔ grblHAL** on CrowPanel **UART1-OUT** (status, MPG claim, jog, hold, zero)
* Offline demo jog/telemetry when no controller answers (status bar shows `Offline · demo`)
* **G-code toolpath preview** — parses embedded `jobs/sample-badge.nc` (G0/G1/G2/G3, M3/M4/M5, S) into the MPG canvas
* **OVERRIDE / FEED tabs** — feed/spindle/rapid realtime overrides (0x90–0x9D), coolant toggles, power-feed jog rate + ±5 mm jogs

### Wire to grblHAL (SKR / controller)

| CrowPanel UART1-OUT | Controller |
|---------------------|------------|
| TX (IO20) | RX |
| RX (IO19) | TX |
| GND | GND |
| 3.3 V logic only | — |

* Baud **115200** 8N1 (same as stock SmartPendant default)
* Protocol: plain Grbl status (`?` / `<Idle\|WPos:…\|FS:…>`) + realtime `0x8B` MPG toggle — **not** the optional framed `Plugin_mpg_transport` yet
* Put the S0/S1 function switches in a mode that leaves UART1 free (not mic/I2S on IO19/IO20)

Host parser smoke tests (no board needed):

```bash
g++ -std=c++17 -I include -o /tmp/test_grbl_status \
  test/test_grbl_status.cpp src/grbl_status.cpp && /tmp/test_grbl_status
g++ -std=c++17 -I include -o /tmp/test_gcode_path \
  test/test_gcode_path.cpp src/gcode_path.cpp && /tmp/test_gcode_path
```

## Next (not in this slice)

* Optional framed MPG transport (`FramedUart` / `Plugin_mpg_transport`)
* Probe / program sender / SD or USB job load on device
* Case / mounting for the 7″ panel + handwheel

## Layout lab

Desktop preview of the same 480×800 chrome lives in `../simulator` — useful while iterating UI without flashing.
