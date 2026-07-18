#include "screen_menu.h"
#include "studybud_theme.h"
#include "esp_log.h"

static const char *TAG = "Screen_Menu";

static lv_obj_t *screen = NULL;
static lv_obj_t *menu_list = NULL;
static int selected_index = 0;

typedef struct {
    const char *icon;
    const char *name;
    screen_id_t target;
} menu_item_t;

static const menu_item_t menu_items[] = {
    { LV_SYMBOL_HOME,      "Home",          SCREEN_HOME },
    { LV_SYMBOL_PLAY,      "Timer",         SCREEN_TIMER },
    { LV_SYMBOL_LIST,      "Todos",         SCREEN_TODOS },
    { LV_SYMBOL_BELL,      "Water",         SCREEN_WATER },
    { LV_SYMBOL_REFRESH,   "Breathing",     SCREEN_BREATHING },
    { LV_SYMBOL_SETTINGS,  "Settings",      SCREEN_SETTINGS },
};
static const int menu_count = sizeof(menu_items) / sizeof(menu_items[0]);

lv_obj_t *screen_menu_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, LV_COLOR_BG, 0);

    /* Menu title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Menu");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* Menu list using lv_list */
    menu_list = lv_list_create(screen);
    lv_obj_set_size(menu_list, 360, 340);
    lv_obj_align(menu_list, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_bg_color(menu_list, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_border_color(menu_list, LV_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(menu_list, 1, 0);
    lv_obj_set_style_radius(menu_list, 16, 0);
    lv_obj_set_style_pad_all(menu_list, 8, 0);

    /* Add menu items */
    for (int i = 0; i < menu_count; i++) {
        lv_obj_t *btn = lv_list_add_btn(menu_list, menu_items[i].icon, menu_items[i].name);
        lv_obj_set_style_text_font(btn, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(btn, LV_COLOR_TEXT, 0);
        lv_obj_set_style_bg_color(btn, LV_COLOR_BG_CARD, 0);
        lv_obj_set_style_bg_color(btn, LV_COLOR_PRIMARY_LIGHT, LV_STATE_PRESSED);

        /* Highlight selected item */
        if (i == selected_index) {
            lv_obj_set_style_bg_color(btn, LV_COLOR_PRIMARY_LIGHT, LV_STATE_FOCUSED);
            lv_obj_add_state(btn, LV_STATE_FOCUSED);
        }
    }

    /* Update list to show selected item */
    lv_obj_scroll_to_y(menu_list, selected_index * 56, LV_ANIM_ON);

    ESP_LOGI(TAG, "Menu screen created with %d items", menu_count);
    return screen;
}

void screen_menu_encoder_event(lv_indev_data_t *data)
{
    if (data->enc_diff != 0) {
        /* Scroll through menu items */
        selected_index += data->enc_diff;
        if (selected_index < 0) selected_index = menu_count - 1;
        if (selected_index >= menu_count) selected_index = 0;

        /* Update visual selection */
        uint32_t child_count = lv_obj_get_child_cnt(menu_list);
        for (uint32_t i = 0; i < child_count; i++) {
            lv_obj_t *child = lv_obj_get_child(menu_list, i);
            if ((int)i == selected_index) {
                lv_obj_add_state(child, LV_STATE_FOCUSED);
                lv_obj_set_style_bg_color(child, LV_COLOR_PRIMARY_LIGHT, LV_STATE_FOCUSED);
            } else {
                lv_obj_clear_state(child, LV_STATE_FOCUSED);
            }
        }

        /* Scroll to keep selected item visible */
        lv_obj_scroll_to_y(menu_list, selected_index * 56, LV_ANIM_ON);
    }
}

screen_id_t screen_menu_get_selection(void)
{
    if (selected_index >= 0 && selected_index < menu_count) {
        return menu_items[selected_index].target;
    }
    return SCREEN_COUNT;
}
