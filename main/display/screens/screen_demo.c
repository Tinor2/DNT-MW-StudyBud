#include "screen_demo.h"
#include "../demo_mode.h"
#include "../app_state.h"
#include "../studybud_theme.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "Screen_Demo";

static lv_obj_t *scr = NULL;
static lv_obj_t *title_label = NULL;
static lv_obj_t *subtitle_label = NULL;
static lv_obj_t *start_label = NULL;

void screen_demo_refresh(void)
{
    if (!scr) return;

    char reason[128] = {0};
    if (demo_mode_is_active()) {
        return;
    }

    bool can_start = demo_mode_can_start(reason, sizeof(reason));
    if (can_start) {
        lv_label_set_text(title_label, LV_SYMBOL_PLAY "  Demo Mode");
        lv_label_set_text(subtitle_label, "Showcase all features with\nseeded presentation data");
        lv_label_set_text(start_label, "[ press to start ]");
        lv_obj_set_style_text_color(start_label, lv_theme_get_color_primary(scr), 0);
    } else {
        lv_label_set_text(title_label, LV_SYMBOL_WARNING "  Cannot Start");
        lv_label_set_text(subtitle_label, reason);
        lv_label_set_text(start_label, "[ press to return ]");
        lv_obj_set_style_text_color(start_label, lv_color_hex(0xFF6B6B), 0);
    }
}

lv_obj_t *screen_demo_create(void)
{
    scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1A1A2E), 0);

    title_label = lv_label_create(scr);
    lv_label_set_text(title_label, LV_SYMBOL_PLAY "  Demo Mode");
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_align(title_label, LV_ALIGN_CENTER, 0, -50);

    subtitle_label = lv_label_create(scr);
    lv_label_set_text(subtitle_label, "Showcase all features with\nseeded presentation data");
    lv_obj_set_style_text_color(subtitle_label, lv_color_hex(0xAAAAAA), 0);
    lv_obj_set_style_text_font(subtitle_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(subtitle_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(subtitle_label, 280);
    lv_obj_align(subtitle_label, LV_ALIGN_CENTER, 0, 10);

    start_label = lv_label_create(scr);
    lv_label_set_text(start_label, "[ press to start ]");
    lv_obj_set_style_text_color(start_label, lv_color_hex(0x00D4AA), 0);
    lv_obj_set_style_text_font(start_label, &lv_font_montserrat_14, 0);
    lv_obj_align(start_label, LV_ALIGN_CENTER, 0, 70);

    return scr;
}

void screen_demo_encoder_event(lv_indev_data_t *data)
{
    if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
        char reason[128] = {0};
        bool can_start = demo_mode_can_start(reason, sizeof(reason));
        if (can_start) {
            demo_mode_start();
        } else {
            ui_manager_switch_screen(SCREEN_MENU);
        }
    }
}
