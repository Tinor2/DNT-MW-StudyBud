#pragma once

#include "lvgl.h"

lv_obj_t *screen_water_create(void);
void screen_water_refresh(void);
void screen_water_encoder_event(lv_indev_data_t *data);
