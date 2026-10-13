#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/touch/Touch_GT911.hpp>
#include "pins_config.h"

// CrowPanel Advance RGB + GT911.
//
//   PANEL_ST7262  — Advance 5″ (default)  Elecrow ST7262 timing
//   PANEL_GW1076  — Advance 7″            GW1076-i2 IPS timing
//
// Pin map is shared across Advance 4.3 / 5 / 7; only RGB timing differs.

#ifndef PANEL_ST7262
#define PANEL_ST7262 0
#endif
#ifndef PANEL_GW1076
#define PANEL_GW1076 0
#endif

#if !PANEL_ST7262 && !PANEL_GW1076
#undef PANEL_ST7262
#define PANEL_ST7262 1
#endif

class LGFX : public lgfx::LGFX_Device {
 public:
  lgfx::Bus_RGB _bus_instance;
  lgfx::Panel_RGB _panel_instance;
  lgfx::Touch_GT911 _touch_instance;

  LGFX(void) {
    {
      auto cfg = _panel_instance.config();
      cfg.memory_width = LCD_H_RES;
      cfg.memory_height = LCD_V_RES;
      cfg.panel_width = LCD_H_RES;
      cfg.panel_height = LCD_V_RES;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      _panel_instance.config(cfg);
    }

    {
      auto cfg = _panel_instance.config_detail();
      cfg.use_psram = 1;
      _panel_instance.config_detail(cfg);
    }

    {
      auto cfg = _bus_instance.config();
      cfg.panel = &_panel_instance;
      cfg.pin_d0 = GPIO_NUM_21;   // B0
      cfg.pin_d1 = GPIO_NUM_47;   // B1
      cfg.pin_d2 = GPIO_NUM_48;   // B2
      cfg.pin_d3 = GPIO_NUM_45;   // B3
      cfg.pin_d4 = GPIO_NUM_38;   // B4
      cfg.pin_d5 = GPIO_NUM_9;    // G0
      cfg.pin_d6 = GPIO_NUM_10;   // G1
      cfg.pin_d7 = GPIO_NUM_11;   // G2
      cfg.pin_d8 = GPIO_NUM_12;   // G3
      cfg.pin_d9 = GPIO_NUM_13;   // G4
      cfg.pin_d10 = GPIO_NUM_14;  // G5
      cfg.pin_d11 = GPIO_NUM_7;   // R0
      cfg.pin_d12 = GPIO_NUM_17;  // R1
      cfg.pin_d13 = GPIO_NUM_18;  // R2
      cfg.pin_d14 = GPIO_NUM_3;   // R3
      cfg.pin_d15 = GPIO_NUM_46;  // R4

      cfg.pin_henable = GPIO_NUM_42;
      cfg.pin_vsync = GPIO_NUM_41;
      cfg.pin_hsync = GPIO_NUM_40;
      cfg.pin_pclk = GPIO_NUM_39;

#if PANEL_GW1076
      // CrowPanel Advance 7″ GW1076-i2 (Elecrow V1.3 / FluidTouch Advance)
      cfg.freq_write = 18000000;
      cfg.hsync_polarity = 1;
      cfg.vsync_polarity = 1;
#else
      // CrowPanel Advance 5″ ST7262 (Elecrow lesson-03 BigInch_LVGL)
      cfg.freq_write = 21000000;
      cfg.hsync_polarity = 0;
      cfg.vsync_polarity = 0;
#endif
      cfg.hsync_front_porch = 8;
      cfg.hsync_pulse_width = 4;
      cfg.hsync_back_porch = 8;
      cfg.vsync_front_porch = 8;
      cfg.vsync_pulse_width = 4;
      cfg.vsync_back_porch = 8;
      cfg.pclk_idle_high = 1;

      _bus_instance.config(cfg);
    }
    _panel_instance.setBus(&_bus_instance);

    {
      auto cfg = _touch_instance.config();
      cfg.x_min = 0;
      cfg.x_max = LCD_H_RES - 1;
      cfg.y_min = 0;
      cfg.y_max = LCD_V_RES - 1;
      cfg.pin_int = -1;
      cfg.bus_shared = true;
      cfg.offset_rotation = 0;
      cfg.i2c_port = I2C_NUM_0;
      cfg.pin_sda = TOUCH_SDA;
      cfg.pin_scl = TOUCH_SCL;
      cfg.pin_rst = -1;
      cfg.freq = 400000;
      cfg.i2c_addr = TOUCH_I2C_ADDR;
      _touch_instance.config(cfg);
      _panel_instance.setTouch(&_touch_instance);
    }

    setPanel(&_panel_instance);
  }
};
