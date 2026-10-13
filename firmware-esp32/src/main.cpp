// SmartPendant Large — CrowPanel Advance bring-up
// Default: 5″ ST7262; alternate: 7″ GW1076-i2 (PANEL_GW1076).
// Portrait 480×800 UI over native 800×480 RGB panel + GT911 touch.

#include <Arduino.h>
#include <Wire.h>
#include <lvgl.h>
#include <esp_heap_caps.h>

#include "pins_config.h"
#include "LovyanGFX_Driver.h"
#include "backlight.h"
#include "grbl_link.h"
#include "handwheel.h"
#include "i2c_mpg_io.h"
#include "job_store.h"
#include "pendant_cfg.h"
#include "program_sender.h"
#include "ui_pendant.h"

static LGFX gfx;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;

static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  if (gfx.getStartCount() > 0) {
    gfx.endWrite();
  }
  gfx.pushImageDMA(area->x1, area->y1, area->x2 - area->x1 + 1, area->y2 - area->y1 + 1,
                   (lgfx::rgb565_t *)&color_p->full);
  lv_disp_flush_ready(disp);
}

static void touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  (void)indev_driver;
  uint16_t x = 0, y = 0;
  data->state = LV_INDEV_STATE_REL;
  if (gfx.getTouch(&x, &y)) {
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
  }
}

static void wake_touch_mcu() {
  // Elecrow Advance: STC8H1K28 must be awake before GT911 answers.
  Wire.begin(TOUCH_SDA, TOUCH_SCL);
  delay(40);
  for (int attempt = 0; attempt < 8; attempt++) {
    Wire.beginTransmission(BL_I2C_ADDR);
    bool bl = (Wire.endTransmission() == 0);
    Wire.beginTransmission(TOUCH_I2C_ADDR);
    bool tp = (Wire.endTransmission() == 0);
    if (bl && tp) {
      Serial.printf("[i2c] ready BL=0x%02X TP=0x%02X\n", BL_I2C_ADDR, TOUCH_I2C_ADDR);
      return;
    }
    // Pulse-reset path from Elecrow examples
    Wire.beginTransmission(BL_I2C_ADDR);
    Wire.write(0x19);
    Wire.endTransmission();
    pinMode(1, OUTPUT);
    digitalWrite(1, LOW);
    delay(120);
    pinMode(1, INPUT);
    delay(100);
  }
  Serial.println("[i2c] WARNING: BL/GT911 not both detected");
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
#if PANEL_GW1076
  Serial.println("SmartPendant Large — CrowPanel Advance 7\" (GW1076-i2)");
#else
  Serial.println("SmartPendant Large — CrowPanel Advance 5\" (ST7262)");
#endif
  Serial.printf("Build %s  portrait=%d\n", VERSION, PORTRAIT_MODE);

  wake_touch_mcu();

  gfx.init();
  gfx.initDMA();
#if PORTRAIT_MODE
  // Rotate native 800×480 → logical 480×800 (pendant portrait)
  gfx.setRotation(1);
#endif
  gfx.startWrite();
  gfx.fillScreen(TFT_BLACK);

  backlight_init();

  // Runtime board / pin / laser settings (LittleFS + optional SD /settings.cfg)
  pendant_cfg_begin();

  lv_init();

  const size_t px = (size_t)UI_H_RES * (size_t)UI_V_RES;
  buf1 = (lv_color_t *)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  buf2 = (lv_color_t *)heap_caps_malloc(px * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  if (!buf1 || !buf2) {
    Serial.println("[lvgl] PSRAM buffer alloc failed");
    while (true) delay(1000);
  }
  lv_disp_draw_buf_init(&draw_buf, buf1, buf2, px);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = UI_H_RES;
  disp_drv.ver_res = UI_V_RES;
  disp_drv.flush_cb = disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = touchpad_read;
  lv_indev_drv_register(&indev_drv);

  grbl_link_begin();
  handwheel_begin();
  i2c_mpg_begin();  // optional PCF8574 on same Wire bus as touch
  job_store_begin();
  ui_pendant_create();
  Serial.printf("[ui] %dx%d ready\n", UI_H_RES, UI_V_RES);
}

void loop() {
  grbl_link_poll();
  program_sender_poll();
  i2c_mpg_poll();
  handwheel_poll();
  lv_timer_handler();
  ui_pendant_tick();
  delay(2);
}
