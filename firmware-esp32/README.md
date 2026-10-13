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
* **MPG START** — green confirm button sends cycle start (`~`) so a job already on the controller can be released from the pendant

LightBurn does **not** auto-fill the pendant preview: LB streams G-code on its own link to grblHAL. The pendant preview is from NC loaded/parsed on the pendant (`sample-badge.nc` or a file in the lab). Use **START** after the controller has the job (or is in Hold) to cycle-start from the device.

### Wire to grblHAL (SKR / controller)

| CrowPanel UART1-OUT | Controller |
|---------------------|------------|
| TX (IO20) | RX |
| RX (IO19) | TX |
| GND | GND |
| 3.3 V logic only | — |

* Baud **115200** 8N1 (same as stock SmartPendant default)
* Protocol: plain Grbl status by default (`?` / `<Idle\|WPos:…\|FS:…>`) + realtime `0x8B` MPG toggle
* Optional **framed** transport (`Plugin_mpg_transport`): toggle in **SETTINGS → UART TRANSPORT**, or build with `-DGRBL_TRANSPORT_FRAMED=1`. Both ends must match.
* Put the S0/S1 function switches in a mode that leaves UART1 free (not mic/I2S on IO19/IO20)

Host parser smoke tests (no board needed):

```bash
g++ -std=c++17 -I include -o /tmp/test_grbl_status \
  test/test_grbl_status.cpp src/grbl_status.cpp && /tmp/test_grbl_status
g++ -std=c++17 -I include -o /tmp/test_gcode_path \
  test/test_gcode_path.cpp src/gcode_path.cpp && /tmp/test_gcode_path
g++ -std=c++17 -I include -o /tmp/test_mpg_framed \
  test/test_mpg_framed.cpp src/mpg_framed.cpp && /tmp/test_mpg_framed
```

### Handwheel (MPG encoder)

Stock pendant uses a **100 PPR** quadrature wheel on a timer channel. CrowPanel Advance has almost no free GPIO after the RGB bus + UART1, so A/B default to **disabled (`-1`)**.

1. Free two 3.3 V inputs (common: reclaim **IO4 / IO5** when TF card & speaker are off — check rear DIP switches).
2. In `include/pins_config.h` (or `build_flags`):
   ```
   -DHANDWHEEL_A_GPIO=4 -DHANDWHEEL_B_GPIO=5
   ```
3. Wire A/B (and GND). Internal pull-ups are enabled.
4. With MPG on, each detent jogs the active axis by the selected step size.

Without pins, the decode path still compiles; the layout lab exposes **◀ Wheel / Wheel ▶** as a stand-in.

## Next (not in this slice)

* Probe / program sender / SD or USB job load on device
* Case / mounting for the 7″ panel + handwheel
* Optional I2C GPIO expander for handwheel + axis-select buttons

## Layout lab

Desktop preview of the same 480×800 chrome lives in `../simulator` — useful while iterating UI without flashing.
