#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *screen_demo_create(void);
void screen_demo_refresh(void);
void screen_demo_encoder_event(lv_indev_data_t *data);

#ifdef __cplusplus
}
#endif
