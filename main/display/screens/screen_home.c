#include "screen_home.h"
#include "ui_manager.h"
#include "studybud_theme.h"
#include "esp_log.h"
#include <time.h>

static const char *TAG = "Screen_Home";

static lv_obj_t *screen = NULL;
static lv_obj_t *time_label = NULL;
static lv_obj_t *date_label = NULL;
static lv_obj_t *session_label = NULL;
static lv_obj_t *water_label = NULL;

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

lv_obj_t *screen_home_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, LV_COLOR_BG, 0);

    time_label = lv_label_create(screen);
    lv_label_set_text(time_label, "00:00");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label, LV_COLOR_TEXT, 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, -30);

    date_label = lv_label_create(screen);
    lv_label_set_text(date_label, "Loading...");
    lv_obj_set_style_text_font(date_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(date_label, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(date_label, LV_ALIGN_CENTER, 0, 20);

    session_label = lv_label_create(screen);
    lv_label_set_text_fmt(session_label, LV_SYMBOL_HOME " %d sessions", 0);
    lv_obj_set_style_text_font(session_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(session_label, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(session_label, LV_ALIGN_BOTTOM_MID, 0, -40);

    water_label = lv_label_create(screen);
    lv_label_set_text_fmt(water_label, LV_SYMBOL_BELL " %d/8", 0);
    lv_obj_set_style_text_font(water_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(water_label, LV_COLOR_INFO, 0);
    lv_obj_align(water_label, LV_ALIGN_BOTTOM_MID, 0, -15);

    /* Update time immediately, then every second */
    update_time_cb(NULL);
    lv_timer_create(update_time_cb, 1000, NULL);

    ESP_LOGI(TAG, "Home screen created");
    return screen;
}

void screen_home_encoder_event(lv_indev_data_t *data)
{
    (void)data;
}
