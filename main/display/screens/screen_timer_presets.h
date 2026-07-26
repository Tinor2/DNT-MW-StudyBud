#pragma once

#include "lvgl.h"

lv_obj_t *screen_timer_presets_create(void);
void screen_timer_presets_encoder_event(lv_indev_data_t *data);
