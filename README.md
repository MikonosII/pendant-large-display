# SmartPendant Large Screen Fork

Fork of [nickshl/SmartPendant](https://github.com/nickshl/SmartPendant) — the grblHAL MPG/DRO touch pendant — adapted for a **7″ touch screen in 480×800 portrait**.

Upstream is locked to a **3.5″ ILI9488 / FT6236** panel (logical **320×480** portrait). This fork adds compile-time **screen profiles**, scales chrome (header, tabs, soft buttons), and ships a **browser layout lab** so you can try the larger layout before flashing hardware.

## Target display

**7″ panel, native 800×480, UI in portrait 480×800** (`SCREEN_PROFILE=2`, default).

| Profile | Define | Logical size | Typical panel |
|--------|--------|--------------|---------------|
| Stock 3.5″ | `SCREEN_PROFILE_3INCH5_320x480` (0) | 320×480 | ILI9488 + FT6236 |
| 4″ same-res | `SCREEN_PROFILE_4INCH_320x480` (1) | 320×480 | Larger glass, same pixels |
| **7″ portrait** (default) | `SCREEN_PROFILE_7INCH_480x800` (2) | **480×800** | 7″ 800×480 module, rotated to portrait |

Select at configure time:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=./cmake/arm_none_eabi_gcc.cmake \
      -DCMAKE_BUILD_TYPE=Release \
      -DSCREEN_PROFILE=2 \
      ..
```

Or change the default in `Application/ScreenConfig.h`.

> **Hardware note:** Profile `2` sizes the framebuffer for **480×800 portrait**. Stock BlackPill + SPI ILI9488 hardware is still 320×480 — use profile `0` or `1` on that board. For the 7″ panel you need a matching display/touch controller (and may want more RAM/SPI bandwidth than the F411 offers comfortably). Use the layout lab to validate UX while wiring hardware.

## Layout lab (browser)

Interactive preview of the pendant chrome at 3.5″ vs 7″ portrait:

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

* `Application/ScreenConfig.h` — display profiles and chrome metrics (default **7″ 480×800 portrait**)
* `AppMain.cpp` — panel/touch constructed from profile sizes
* Header / tabs / soft-button heights follow the profile instead of hard-coded `40`
* Version bumped to **0.50.x** (fork marker)
* `simulator/` — Next.js layout lab

## Credits

Original SmartPendant firmware by Nicolai Shlapunov / Devtronic. DevCore is BSD-3-Clause. This fork keeps their copyright headers and adds large-screen layout work on top.
