# SmartPendant Large Screen Fork

Fork of [nickshl/SmartPendant](https://github.com/nickshl/SmartPendant) aimed at **CrowPanel Advance** touch HMIs in **480×800 portrait**.

> **Status: work in progress.** Hardware smoke on CrowPanel + controller is not done yet.  
> Do **not** open a PR back to nickshl/SmartPendant until that testing is finished.

## Target hardware (what to flash)

| | Default | Large-screen alternate |
|--|--|--|
| Board | **Elecrow CrowPanel Advance 5.0″** | CrowPanel Advance 7.0″ |
| Driver IC | **ST7262** | GW1076-i2 |
| MCU | ESP32-S3-WROOM-1-N16R8 | same |
| Native panel | 800×480 RGB IPS | same |
| UI | **480×800** portrait | same chrome |
| Touch | GT911 | GT911 |
| Firmware env | `crowpanel-advance-5` | `crowpanel-advance-7-v13` |

```bash
cd firmware-esp32
pio run -e crowpanel-advance-5 -t upload          # 5″ ST7262 (default)
pio run -e crowpanel-advance-7-v13 -t upload      # 7″ GW1076-i2
```

See [`firmware-esp32/README.md`](firmware-esp32/README.md) for backlight variants and wiring.

The stock STM32F411 + ILI9488 SmartPendant tree remains in this repo as the upstream reference (`Application/`, `DevCore/`, …). **It does not run on CrowPanel** — different MCU and display bus.

## Layout lab (browser)

Interactive 480×800 portrait preview (no hardware needed):

```bash
cd simulator
npm install
npm run dev
```

Open [http://127.0.0.1:43123](http://127.0.0.1:43123). Profiles include stock 3.5″/4″ plus **5″ Advance** (default) and **7″ Advance** (same pixel map, larger glass).

## STM32 profiles (reference only)

`Application/ScreenConfig.h` still documents the logical sizes used by the layout lab:

| Profile | Logical size | Notes |
|--------|--------------|--------|
| 0 | 320×480 | Upstream 3.5″ |
| 1 | 320×480 | 4″ same-res |
| 2 | **480×800** | CrowPanel Advance 5″ (default) |
| 3 | **480×800** | CrowPanel Advance 7″ alternate |

## Status

* CrowPanel Advance bring-up + SmartPendant-style MPG chrome: **in `firmware-esp32/`** (5″ default, 7″ env retained)
* Soft macros (teachable), overrides, FEED laser %, PROBE, jobs/sender: **in lab + firmware**
* Hardware smoke on real CrowPanel + controller: **not started** — required before any upstream PR
* Upstream contribution to nickshl/SmartPendant: **blocked** until smoke test passes

## Credits

Original SmartPendant by Nicolai Shlapunov / Devtronic. CrowPanel pin map / timing adapted from Elecrow examples and community Advance bring-up. DevCore is BSD-3-Clause.
