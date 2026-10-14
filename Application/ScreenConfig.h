//******************************************************************************
//  @file ScreenConfig.h
//  @details Display size profiles for the SmartPendant large-screen fork.
//
//  Upstream SmartPendant targets a 3.5" ILI9488 panel (480×320 native,
//  shown in portrait as 320×480). Stock 3.5"/4.0" keep DRO + steps chrome
//  plus a compact 2×4 soft-macro grid; live map is Advance-only.
//  Non-display work (settings.cfg, macros.cfg, UART, laser dout, …) is shared.
//
//  Flash target: CrowPanel Advance via firmware-esp32/ (480×800 portrait).
//  The STM32 ILI9488 path below is layout-reference only.
//
//  Select a profile with -DSCREEN_PROFILE=... at compile time, or edit the
//  default below. The interactive layout lab under simulator/ mirrors these
//  sizes for desktop preview.
//******************************************************************************

#ifndef ScreenConfig_h
#define ScreenConfig_h

#include <stdint.h>

// *****************************************************************************
// ***   Profile IDs   *********************************************************
// *****************************************************************************
#define SCREEN_PROFILE_3INCH5_320x480  0  // Stock Devtronic / upstream
#define SCREEN_PROFILE_4INCH_320x480   1  // Same pixels, ~4" glass (ST7796/ILI9488)
#define SCREEN_PROFILE_7INCH_480x800   2  // 7" 800×480 panel → 480×800 portrait

// Back-compat alias (older docs used a 5" name for the same logical canvas)
#define SCREEN_PROFILE_5INCH_480x800   SCREEN_PROFILE_7INCH_480x800

#ifndef SCREEN_PROFILE
// Default: 7" touch panel in portrait (480×800).
#define SCREEN_PROFILE SCREEN_PROFILE_7INCH_480x800
#endif

// *****************************************************************************
// ***   Per-profile geometry   ************************************************
// *****************************************************************************
// PANEL_* = native controller width/height before Application rotation.
// After IDisplay::ROTATION_RIGHT the logical framebuffer is LOGICAL_W × LOGICAL_H
// (portrait, handwheel-friendly). HEADER_H / SOFT_BTN_H scale with width so
// chrome stays finger-friendly on bigger glass.

#if (SCREEN_PROFILE == SCREEN_PROFILE_3INCH5_320x480) || \
    (SCREEN_PROFILE == SCREEN_PROFILE_4INCH_320x480)

#define PANEL_NATIVE_W          480
#define PANEL_NATIVE_H          320
#define LOGICAL_SCREEN_W        320
#define LOGICAL_SCREEN_H        480
#define SCREEN_HEADER_H         40
#define SCREEN_TABS_H           40
#define SCREEN_SOFT_BTN_H       36   // Font_8x12 * 3
#define SCREEN_TOUCH_W          320u
#define SCREEN_TOUCH_H          480u
#define DISPLAY_LINE_BUF_PIXELS 480

#if (SCREEN_PROFILE == SCREEN_PROFILE_4INCH_320x480)
#define SCREEN_PROFILE_NAME     "4.0\" 320x480"
#define SCREEN_PROFILE_DIAG_IN  4.0f
#else
#define SCREEN_PROFILE_NAME     "3.5\" 320x480"
#define SCREEN_PROFILE_DIAG_IN  3.5f
#endif

#elif (SCREEN_PROFILE == SCREEN_PROFILE_7INCH_480x800)

// 7" modules are commonly 800×480 native; Application uses ROTATION_RIGHT so
// the UI framebuffer is portrait 480 wide × 800 tall.
#define PANEL_NATIVE_W          800
#define PANEL_NATIVE_H          480
#define LOGICAL_SCREEN_W        480
#define LOGICAL_SCREEN_H        800
#define SCREEN_HEADER_H         56
#define SCREEN_TABS_H           48
#define SCREEN_SOFT_BTN_H       48
#define SCREEN_TOUCH_W          480u
#define SCREEN_TOUCH_H          800u
#define DISPLAY_LINE_BUF_PIXELS 800
#define SCREEN_PROFILE_NAME     "7.0\" 480x800 portrait"
#define SCREEN_PROFILE_DIAG_IN  7.0f

#else
#error "Unsupported SCREEN_PROFILE value"
#endif

// Line buffer for DisplayDrv (ILI9488 18-bit path needs 1.5× when using 16-bit color)
#ifndef DISPLAY_MAX_BUF_LEN
#define DISPLAY_MAX_BUF_LEN ((uint32_t)(DISPLAY_LINE_BUF_PIXELS * 3 / 2))
#endif

#endif
