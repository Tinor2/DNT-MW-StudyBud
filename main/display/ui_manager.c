#include "ui_manager.h"
#include "screens/screen_home.h"
#include "screens/screen_menu.h"
#include "screens/screen_todos.h"
#include "studybud_theme.h"
#include "esp_log.h"

static const char *TAG = "UI_Manager";

static screen_id_t current_screen = SCREEN_HOME;
static lv_obj_t *screens[SCREEN_COUNT] = {0};

static void (*screen_event_handlers[SCREEN_COUNT])(lv_indev_data_t *) = {0};

#define LONG_PRESS_MS 800
static uint32_t press_start_tick = 0;
static bool waiting_for_release = false;
static bool long_press_fired = false;

void ui_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing UI Manager");

    /* Create default theme */
    lv_disp_t *disp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(
        disp,
        LV_COLOR_PRIMARY,
        LV_COLOR_SECONDARY,
        false,
        LV_FONT_DEFAULT
    );
    lv_disp_set_theme(disp, theme);

    /* Create all screens */
    screens[SCREEN_HOME] = screen_home_create();
    screens[SCREEN_MENU] = screen_menu_create();
    screens[SCREEN_TODOS] = screen_todos_create();

    /* Register event handlers */
    screen_event_handlers[SCREEN_HOME] = screen_home_encoder_event;
    screen_event_handlers[SCREEN_MENU] = screen_menu_encoder_event;
    screen_event_handlers[SCREEN_TODOS] = screen_todos_encoder_event;

    /* Load home screen as default */
    lv_scr_load(screens[SCREEN_HOME]);
    current_screen = SCREEN_HOME;

    ESP_LOGI(TAG, "UI Manager initialized, showing Home screen");
}

void ui_manager_switch_screen(screen_id_t screen)
{
    if (screen >= SCREEN_COUNT || !screens[screen]) {
        ESP_LOGW(TAG, "Invalid screen ID: %d", screen);
        return;
    }

    ESP_LOGI(TAG, "Switching to screen %d", screen);
    lv_scr_load_anim(screens[screen], LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
    current_screen = screen;
}

void ui_manager_encoder_event(lv_indev_data_t *data)
{
    /* Forward rotation immediately, even while button is held */
    if (data->enc_diff != 0 && screen_event_handlers[current_screen]) {
        lv_indev_data_t fwd;
        fwd.enc_diff = data->enc_diff;
        fwd.state = LV_INDEV_STATE_REL;
        screen_event_handlers[current_screen](&fwd);
    }

    /* Button just pressed: start long-press timer, don't forward to screen yet */
    if (data->state == LV_INDEV_STATE_PR && !waiting_for_release) {
        press_start_tick = lv_tick_get();
        waiting_for_release = true;
        long_press_fired = false;
        return;
    }

    /* Button still held: check if long press threshold crossed */
    if (waiting_for_release && data->state == LV_INDEV_STATE_PR) {
        if (!long_press_fired && lv_tick_elaps(press_start_tick) >= LONG_PRESS_MS) {
            if (current_screen != SCREEN_HOME && current_screen != SCREEN_MENU) {
                long_press_fired = true;
                ESP_LOGI(TAG, "Long press -> menu");
                ui_manager_switch_screen(SCREEN_MENU);
            }
        }
        return;
    }

    /* Button released: fire short press action if it wasn't a long press */
    if (data->state == LV_INDEV_STATE_REL && waiting_for_release) {
        bool was_long = long_press_fired;
        waiting_for_release = false;
        press_start_tick = 0;
        long_press_fired = false;

        if (was_long) return;

        /* Short press: forward to screen handler */
        if (current_screen == SCREEN_MENU) {
            screen_id_t selected = screen_menu_get_selection();
            if (selected != SCREEN_COUNT) {
                ui_manager_switch_screen(selected);
                return;
            }
        }
        if (screen_event_handlers[current_screen]) {
            lv_indev_data_t pr = {0};
            pr.state = LV_INDEV_STATE_PR;
            pr.enc_diff = 0;
            screen_event_handlers[current_screen](&pr);
        }
        return;
    }
}

screen_id_t ui_manager_get_current_screen(void)
{
    return current_screen;
}
