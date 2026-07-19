#pragma once

#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LVGL_TICK_PERIOD_MS    2
#define LCD_H_RES              480
#define LCD_V_RES              480

void LVGL_Driver_init(void);
void LVGL_Driver_loop(void);

#ifdef __cplusplus
}
#endif
