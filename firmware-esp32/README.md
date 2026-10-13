# SmartPendant Large — CrowPanel Advance firmware

ESP32-S3 port of the SmartPendant large-screen fork for **Elecrow CrowPanel Advance** HMIs.

| Item | Default (5″) | Alternate (7″) |
|------|--------------|----------------|
| Board | CrowPanel Advance **5.0″** | CrowPanel Advance **7.0″** |
| Driver IC | **ST7262** | **GW1076-i2** |
| MCU | ESP32-S3-WROOM-1-N16R8 | same |
| Native panel | IPS RGB 800×480 | same |
| UI orientation | **Portrait 480×800** (`PORTRAIT_MODE=1`) | same |
| Touch | GT911 @ I2C `0x5D` | same |
| Backlight MCU | STC8H1K28 @ I2C `0x30` | same |
| PlatformIO env | `crowpanel-advance-5` | `crowpanel-advance-7-v13` |

RGB **pin map is shared** across Advance sizes; only LovyanGFX timing differs (`PANEL_ST7262` vs `PANEL_GW1076`). The STM32 tree at the repo root remains as the upstream-layout reference; **this folder is what you flash**.

## Build & flash

```bash
cd firmware-esp32
# 5″ ST7262 (default)
pio run -e crowpanel-advance-5
pio run -e crowpanel-advance-5 -t upload
# 7″ GW1076-i2 (large-screen alternate — keep using if that is your panel)
pio run -e crowpanel-advance-7-v13 -t upload
pio device monitor -b 115200
```

Use `crowpanel-advance-7-v12` if your 7″ board is an older Advance revision (different backlight command range).

### Requirements

* [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
* USB-C data cable to the CrowPanel
* First build downloads the ESP32 Arduino/IDF platform — expect a long first compile

## What works in this slice

* RGB panel bring-up via LovyanGFX (Advance pin map; ST7262 or GW1076 timing)
* GT911 capacitive touch
* STC8H1K28 backlight on
* SmartPendant-style chrome: header tabs, DRO, step sizes, MPG toggle, soft buttons
* **Plain ASCII UART ↔ grblHAL** on CrowPanel **UART1-OUT** (status, MPG claim, jog, hold, zero)
* Offline demo jog/telemetry when no controller answers (status bar shows `Offline · demo`)
* **G-code toolpath preview** — parses embedded `jobs/sample-badge.nc` (G0/G1/G2/G3, M3/M4/M5, S) into the MPG canvas
* **OVERRIDE / FEED tabs** — feed/spindle/rapid realtime overrides (0x90–0x9D), coolant toggles, power-feed jog rate + ±5 mm jogs
* **MPG START** — green confirm button sends cycle start (`~`) so a job already on the controller can be released from the pendant
* **LASER TEST** (MPG) — toggle K+ enable only (`M64`/`M65`). Lights when armed.
* **LASER ON / OFF** (FEED tab) — power in **percent (0.1% steps)** → `M3 S` via `$30`; ON = `M64` + `M3`; OFF = `M65` + `M5`. Dout = `LASER_ENABLE_DOUT`; default = `LASER_FEED_PCT` (5.0%).
* **PROBE** — axis select, G38.2/G38.3 toward WPos ± travel, live `Pn:P` contact, last `[PRB:]` machine coords, Zero WPos
* **GCODE / jobs** — list + load embedded sample, LittleFS `/jobs`, and SD/TF `/jobs` into the toolpath preview
* **I2C MPG expander** (optional) — PCF8574 on SDA15/SCL16 for handwheel A/B + axis/step buttons when GPIO is scarce

LightBurn does **not** auto-fill the pendant preview: LB streams G-code on its own link to grblHAL. The pendant preview is from NC loaded/parsed on the pendant (embedded sample, LittleFS/SD `/jobs`, or a file in the lab). Use **START** after the controller has the job (or is in Hold) to cycle-start from the device.

### Runtime settings (edit the file — no rebuild)

After you flash the binary (+ filesystem), pin and laser setup come from a text file:

1. **Preferred:** put **`/settings.cfg`** on the TF/SD card (edit on a PC, reboot).
2. Or edit LittleFS **`/settings.cfg`** (seeded from [`data/settings.cfg`](data/settings.cfg) via `pio run -t uploadfs`).

```text
board crowpanel-advance-5
laser.enable_dout 0
laser.feed_pct 5.0
laser.s_max 1000
uart.baud 115200
```

| Key | Meaning |
|-----|---------|
| `board` | Built-in profile: `crowpanel-advance-5` or `crowpanel-advance-7` |
| `laser.enable_dout` | grblHAL dout `n` for K+ (`M64 Pn` / `M65 Pn`) |
| `laser.feed_pct` / `laser.s_max` | FEED default % and `$30` scale |
| `uart.tx` / `rx` / `baud` | CrowPanel UART1-OUT → controller |
| `handwheel.*` / `i2c_mpg.*` / `sd.*` | Optional wheel / expander / TF pins |

Board pin maps also live under [`data/boards/`](data/boards/). SETTINGS tab shows the active board and which file won (`sd:` vs `lfs:`).

**Note:** LCD panel timing (ST7262 vs GW1076) still comes from which firmware env you flashed; only UART/laser/handwheel/SD pins are runtime. Full wiring: [`WIRING.md`](WIRING.md).

### Wire to grblHAL (SKR / controller)

| CrowPanel UART1-OUT | Controller |
|---------------------|------------|
| TX (IO20) | RX |
| RX (IO19) | TX |
| GND | GND |
| 3.3 V logic only | — |

* Baud **115200** 8N1 (override with `uart.baud` in `/settings.cfg`)
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
g++ -std=c++17 -I include -DPENDANT_CFG_HOST_TEST -o /tmp/test_pendant_cfg \
  test/test_pendant_cfg.cpp src/pendant_cfg_parse.cpp && /tmp/test_pendant_cfg
g++ -std=c++17 -I include -o /tmp/test_i2c_mpg_io \
  test/test_i2c_mpg_io.cpp && /tmp/test_i2c_mpg_io
```

### Handwheel (MPG encoder)

Stock pendant uses a **100 PPR** quadrature wheel. CrowPanel Advance has almost no free GPIO after the RGB bus + UART1, so A/B default to **disabled (`-1`)** in `/settings.cfg`.

**Option A — direct GPIO** (when TF/speaker are unused): set `handwheel.a 4` / `handwheel.b 5` in `/settings.cfg`, wire A/B + GND, reboot.

**Option B — I2C expander** (preferred when TF uses IO4–6): set `i2c_mpg.enable 1` (and optional `i2c_mpg.addr 0x20`). PCF8574 on SDA15/SCL16 — **P0/P1** wheel, **P2–P4** axis, **P5** step (active-low).

### SD / LittleFS jobs

* LittleFS is mounted at boot. Seed with `pio run -t uploadfs` from `data/` (includes `settings.cfg`, `boards/`, `jobs/`).
* SD/TF SPI pins come from `/settings.cfg` (`sd.*`). Set rear DIP to a **TF-card** mode. Set `sd.enable 0` to reclaim pins for a GPIO handwheel.
* GCODE tab lists embedded sample + `/jobs` on each mounted store.

## Next (not in this slice)

* Program sender (stream NC to grblHAL from the pendant)
* Case / mounting for the panel + handwheel
* Hardware smoke on CrowPanel + controller

## Layout lab

Desktop preview of the same 480×800 chrome lives in `../simulator` — useful while iterating UI without flashing. Pick **5″ Advance** or **7″ Advance** in the profile list (identical logical size; different glass).
