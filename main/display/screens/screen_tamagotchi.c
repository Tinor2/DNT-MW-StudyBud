#include "screen_tamagotchi.h"
#include "ui_manager.h"
#include "color_palette.h"
#include "esp_log.h"

static const char *TAG = "Screen_Tamagotchi";

static lv_obj_t *screen = NULL;

lv_obj_t *screen_tamagotchi_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, pastel_color(theme_accent(SCREEN_TAMAGOTCHI)), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl_title = lv_label_create(screen);
    lv_label_set_text(lbl_title, "Tamagotchi");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, darken_text_color(theme_accent(SCREEN_TAMAGOTCHI), 0.5f), 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_t *lbl_coming = lv_label_create(screen);
    lv_label_set_text(lbl_coming, "Coming soon!");
    lv_obj_set_style_text_font(lbl_coming, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_coming, darken_text_color(theme_accent(SCREEN_TAMAGOTCHI), 0.6f), 0);
    lv_obj_align(lbl_coming, LV_ALIGN_CENTER, 0, 0);

    ESP_LOGI(TAG, "Tamagotchi screen created");
    return screen;
}

void screen_tamagotchi_encoder_event(lv_indev_data_t *data)
{
    (void)data;
}
