#pragma once

#include "lvgl.h"

void encoder_init(void);
void encoder_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
