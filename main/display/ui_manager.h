#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SCREEN_HOME,
    SCREEN_MENU,
    SCREEN_TIMER,
    SCREEN_TIMER_PRESETS,
    SCREEN_TIMER_EDIT,
    SCREEN_TODOS,
    SCREEN_WATER,
    SCREEN_BREATHING,
    SCREEN_SEDENTARY,
    SCREEN_SETTINGS,
    SCREEN_BACKGROUNDS,
    SCREEN_NOTIFICATIONS,
    SCREEN_SLEEP,
    SCREEN_TAMAGOTCHI,
    SCREEN_DEMO,
    SCREEN_COUNT
} screen_id_t;

void ui_manager_init(void);
void ui_manager_switch_screen(screen_id_t screen);
void ui_manager_encoder_event(lv_indev_data_t *data);
screen_id_t ui_manager_get_current_screen(void);
void ui_manager_set_reading_light(int strength);

#ifdef __cplusplus
}
#endif
