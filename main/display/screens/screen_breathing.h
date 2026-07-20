#pragma once

#include "lvgl.h"

lv_obj_t *screen_breathing_create(void);
void screen_breathing_encoder_event(lv_indev_data_t *data);
