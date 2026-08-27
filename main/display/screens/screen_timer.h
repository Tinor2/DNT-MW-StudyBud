#pragma once

#include "lvgl.h"

lv_obj_t *screen_timer_create(void);
void screen_timer_destroy(void);
void screen_timer_encoder_event(lv_indev_data_t *data);
void screen_timer_background_tick(void);
