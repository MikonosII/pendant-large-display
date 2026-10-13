# SmartPendant Large Screen Fork

Fork of [nickshl/SmartPendant](https://github.com/nickshl/SmartPendant) — the grblHAL MPG/DRO touch pendant — adapted so the UI can target a **bigger touch screen**.

Upstream is locked to a **3.5″ ILI9488 / FT6236** panel (logical **320×480** portrait). This fork adds compile-time **screen profiles**, scales chrome (header, tabs, soft buttons), and ships a **browser layout lab** so you can try the larger layout before flashing hardware.

## Screen profiles

| Profile | Define | Logical size | Typical panel |
|--------|--------|--------------|---------------|
| Stock 3.5″ | `SCREEN_PROFILE_3INCH5_320x480` (0) | 320×480 | ILI9488 + FT6236 |
| 4″ same-res | `SCREEN_PROFILE_4INCH_320x480` (1) | 320×480 | Larger glass, same pixels (ST7796/ILI9488) |
| **5″ large** (default) | `SCREEN_PROFILE_5INCH_480x800` (2) | **480×800** | 800×480 native module, rotated to portrait |

Select at configure time:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=./cmake/arm_none_eabi_gcc.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DSCREEN_PROFILE=2 \
      ..
```

Or change the default in `Application/ScreenConfig.h`.

> **Hardware note:** The 5″ profile sizes the framebuffer and layout for 480×800. Stock BlackPill + SPI ILI9488 hardware is still 320×480 — use profile `0` or `1` on that board. For a true 5″ panel you need a matching display/touch controller (and may want more RAM/SPI bandwidth than the F411 offers comfortably). The layout lab below is the place to validate UX while you choose hardware.

## Layout lab (browser)

Interactive preview of the pendant chrome and MPG screen at 3.5″ vs 5″ sizes:

```bash
cd simulator
npm install
npm run dev
```

Open the URL printed by Next.js (this environment uses port **43123**).

## Firmware build

Same as upstream SmartPendant:

* **STM32CubeIDE** — import the project, build, flash with STM32CubeProgrammer.
* **CMake + arm-none-eabi-gcc** (CMake ≥ 4.0):

```bash
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=./cmake/arm_none_eabi_gcc.cmake -DCMAKE_BUILD_TYPE=Debug -DSCREEN_PROFILE=2 ..
make
```

`DevCore/` is **vendored** in this fork (no submodule checkout required).

## Upstream hardware (unchanged for profiles 0/1)

* WeAct BlackPill F411 (25 MHz HSE)
* 3.5″ (or 4″ same-res) SPI touch display
* 100 PPR handwheel
* grblHAL controller UART in **MPG & DRO** mode

Case/PCB: [Devtronic-US/SmartPendant](https://github.com/Devtronic-US/SmartPendant/)

## What changed vs upstream

* `Application/ScreenConfig.h` — display profiles and chrome metrics
* `AppMain.cpp` — panel/touch constructed from profile sizes
* Header / tabs / soft-button heights follow the profile instead of hard-coded `40`
* Version bumped to **0.50.0** (fork marker)
* `simulator/` — Next.js layout lab

## Credits

Original SmartPendant firmware by Nicolai Shlapunov / Devtronic. DevCore is BSD-3-Clause. This fork keeps their copyright headers and adds large-screen layout work on top.
