#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

lv_obj_t *screen_settings_create(void);
void screen_settings_refresh(void);
void screen_settings_encoder_event(lv_indev_data_t *data);

#ifdef __cplusplus
}
#endif
