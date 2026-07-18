#pragma once

#include "lvgl.h"
#include "../ui_manager.h"

lv_obj_t *screen_menu_create(void);
void screen_menu_encoder_event(lv_indev_data_t *data);
screen_id_t screen_menu_get_selection(void);
