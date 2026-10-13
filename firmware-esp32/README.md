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
* Mock jog (no grblHAL link yet)

## Next (not in this slice)

* UART / framed MPG transport to grblHAL
* Probe / overrides / program sender screens wired to a real controller
* Case / mounting for the 7″ panel + handwheel

## Layout lab

Desktop preview of the same 480×800 chrome lives in `../simulator` — useful while iterating UI without flashing.
