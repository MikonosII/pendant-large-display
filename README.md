# SmartPendant Large Screen Fork

Fork of [nickshl/SmartPendant](https://github.com/nickshl/SmartPendant) aimed at a **7″ CrowPanel Advance** touch HMI in **480×800 portrait**.

## Target hardware (what to flash)

| | |
|--|--|
| Board | **Elecrow CrowPanel Advance 7.0″** |
| MCU | ESP32-S3-WROOM-1-N16R8 |
| Display | **GW1076-i2** IPS RGB 800×480 → UI rotated to **480×800** |
| Touch | GT911 |
| Firmware | [`firmware-esp32/`](firmware-esp32/) |

```bash
cd firmware-esp32
pio run -e crowpanel-advance-7-v13 -t upload
```

See [`firmware-esp32/README.md`](firmware-esp32/README.md) for v1.2 vs v1.3 backlight envs.

The stock STM32F411 + ILI9488 SmartPendant tree remains in this repo as the upstream reference (`Application/`, `DevCore/`, …). **It does not run on the CrowPanel** — different MCU and display bus.

## Layout lab (browser)

Interactive 480×800 portrait preview (no hardware needed):

```bash
cd simulator
npm install
npm run dev
```

Open [http://127.0.0.1:43123](http://127.0.0.1:43123).

## STM32 profiles (reference only)

`Application/ScreenConfig.h` still documents the logical sizes used by the layout lab:

| Profile | Logical size | Notes |
|--------|--------------|--------|
| 0 | 320×480 | Upstream 3.5″ |
| 1 | 320×480 | 4″ same-res |
| 2 | **480×800** | Matches CrowPanel portrait UI |

## Status

* CrowPanel Advance 7″ bring-up + SmartPendant-style MPG chrome: **in `firmware-esp32/`**
* grblHAL UART / framed MPG link: **not wired yet**
* Full screen set (probe, sender, …): **stubbed as tabs**

## Credits

Original SmartPendant by Nicolai Shlapunov / Devtronic. CrowPanel pin map / timing adapted from Elecrow examples and community Advance bring-up. DevCore is BSD-3-Clause.
