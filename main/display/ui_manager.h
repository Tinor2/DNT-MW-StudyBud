#pragma once

#include "lvgl.h"

typedef enum {
    SCREEN_HOME,
    SCREEN_MENU,
    SCREEN_TIMER,
    SCREEN_TIMER_PRESETS,
    SCREEN_TODOS,
    SCREEN_WATER,
    SCREEN_BREATHING,
    SCREEN_SEDENTARY,
    SCREEN_SETTINGS,
    SCREEN_NOTIFICATIONS,
    SCREEN_COUNT
} screen_id_t;

void ui_manager_init(void);
void ui_manager_switch_screen(screen_id_t screen);
void ui_manager_encoder_event(lv_indev_data_t *data);
screen_id_t ui_manager_get_current_screen(void);
