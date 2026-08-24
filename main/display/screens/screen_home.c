#include "screen_home.h"
#include "ui_manager.h"
#include "color_palette.h"
#include "../app_state.h"
#include "../utils/session_store.h"
#include "../utils/points_store.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "Screen_Home";

static lv_obj_t *screen = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *date_label = NULL;
static lv_obj_t *today_label = NULL;
static lv_obj_t *level_label = NULL;
static lv_obj_t *hint_label = NULL;

static void update_time_cb(lv_timer_t *timer)
{
    (void)timer;
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    char time_buf[8];
    strftime(time_buf, sizeof(time_buf), "%H:%M", &timeinfo);
    lv_label_set_text(time_label, time_buf);

    char date_buf[32];
    strftime(date_buf, sizeof(date_buf), "%A", &timeinfo);
    lv_label_set_text(date_label, date_buf);
}

void screen_home_refresh(void)
{
    points_state_t *ps = points_store_get_state();
    int level = points_store_get_level();
    int into = points_store_get_level_progress();
    int need = points_store_get_level_threshold();

    lv_label_set_text_fmt(today_label, "%d seeds today", ps->today_points);
    lv_label_set_text_fmt(level_label, "Level %d  —  %d / %d to next", level, into, need);

    if (ps->today_points == 0) {
        lv_label_set_text(hint_label, "Press to set goals and grow your plant");
    } else {
        lv_label_set_text(hint_label, "Press to check goals and streaks");
    }
}

lv_obj_t *screen_home_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, pastel_color(theme_accent(SCREEN_HOME)), 0);

    time_label = lv_label_create(screen);
    lv_label_set_text(time_label, "00:00");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label, LV_COLOR_TEXT, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -50);

    date_label = lv_label_create(screen);
    lv_label_set_text(date_label, "Loading...");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(date_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, -10);

    today_label = lv_label_create(screen);
    lv_label_set_text(today_label, "0 seeds today");
    lv_obj_set_style_text_font(today_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(today_label, LV_COLOR_TEXT, 0);
    lv_obj_align(today_label, LV_ALIGN_CENTER, 0, 30);

    level_label = lv_label_create(screen);
    lv_label_set_text(level_label, "Level 1 — 0 / 100 to next");
    lv_obj_set_style_text_font(level_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(level_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(level_label, LV_ALIGN_CENTER, 0, 56);

    hint_label = lv_label_create(screen);
    lv_label_set_text(hint_label, "Press to set goals and grow your plant");
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint_label, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -30);

    /* Update time immediately, then every second */
    update_time_cb(NULL);
    lv_timer_create(update_time_cb, 1000, NULL);

    /* Reflect current state immediately */
    screen_home_refresh();

    ESP_LOGI(TAG, "Home screen created");
    return screen;
}

void screen_home_encoder_event(lv_indev_data_t *data)
{
    (void)data;
}
