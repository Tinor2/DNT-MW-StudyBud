#include "screen_menu.h"
#include "studybud_theme.h"
#include "ui_manager.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "Screen_Menu";

#define DISPLAY_R    240
#define DISPLAY_CY   240
#define ROW_SPACING  64
#define MAX_ROW_W    420
#define MIN_VISIBLE_W 120
#define ROW_INNER_PAD 12

static lv_obj_t *screen = NULL;
static lv_obj_t *menu_container = NULL;
static lv_obj_t *arrow_up_label = NULL;
static lv_obj_t *arrow_down_label = NULL;
static lv_obj_t *count_label = NULL;
static lv_timer_t *radial_timer = NULL;

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
    { LV_SYMBOL_IMAGE,     "Backgrounds",   SCREEN_BACKGROUNDS },
    { LV_SYMBOL_SETTINGS,  "Settings",      SCREEN_SETTINGS },
};
static const int menu_count = sizeof(menu_items) / sizeof(menu_items[0]);

static lv_obj_t *row_icons[7];
static lv_obj_t *row_labels[7];
static lv_obj_t *row_objects[7];
static lv_obj_t *focused_row = NULL;

static void update_focus_styles(void);
static void update_arrow_visibility(void);
static void update_menu_count(void);
static void apply_radial_scroll(void);
static void radial_timer_cb(lv_timer_t *timer);
static void initial_scroll_cb(lv_timer_t *timer);

static void anim_set_opa(void *var, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)val, 0);
}

static void create_menu_row(lv_obj_t *parent, int index)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, MAX_ROW_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_row(row, 0, 0);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_style_pad_all(row, ROW_INNER_PAD, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_min_width(row, 0, 0);
    row_objects[index] = row;

    lv_obj_t *icon = lv_label_create(row);
    lv_label_set_text(icon, menu_items[index].icon);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(icon, LV_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_opa(icon, LV_OPA_80, 0);
    row_icons[index] = icon;

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, menu_items[index].name);
    lv_obj_set_flex_grow(label, 1);
    lv_obj_set_style_text_color(label, LV_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_80, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_pad_ver(row, 6, 0);
    row_labels[index] = label;
}

static void update_focus_styles(void)
{
    for (int i = 0; i < menu_count; i++) {
        lv_obj_t *row = row_objects[i];
        lv_obj_t *label = row_labels[i];
        lv_obj_t *icon = row_icons[i];
        if (!row || !label || !icon) continue;

        if (row == focused_row) {
            lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_opa(icon, LV_OPA_COVER, 0);
            lv_obj_set_style_pad_ver(row, 12, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_20, 0);
            lv_obj_set_style_bg_color(row, LV_COLOR_PRIMARY, 0);
            lv_obj_set_style_radius(row, 12, 0);

            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, label);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_set_opa);
            lv_anim_set_values(&a, LV_OPA_80, LV_OPA_COVER);
            lv_anim_set_time(&a, 200);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_start(&a);
        } else {
            lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
            lv_obj_set_style_text_opa(icon, LV_OPA_80, 0);
            lv_obj_set_style_pad_ver(row, 6, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);

            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, label);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_set_opa);
            lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_80);
            lv_anim_set_time(&a, 200);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_start(&a);
        }
    }
}

static void update_arrow_visibility(void)
{
    lv_coord_t st = lv_obj_get_scroll_top(menu_container);
    lv_coord_t sb = lv_obj_get_scroll_bottom(menu_container);
    if (st <= 0) lv_obj_add_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_clear_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);
    if (sb <= 0) lv_obj_add_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_clear_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);
}

static void update_menu_count(void)
{
    if (!count_label) return;
    lv_label_set_text_fmt(count_label, "%d apps", menu_count);
}

static void apply_radial_scroll(void)
{
    if (!menu_container) return;

    lv_obj_update_layout(menu_container);

    bool any_hidden_top = false;
    bool any_hidden_bottom = false;

    for (int i = 0; i < menu_count; i++) {
        lv_obj_t *row = row_objects[i];
        if (!row) continue;

        lv_coord_t row_y = row->coords.y1;
        lv_coord_t row_h = lv_obj_get_height(row);
        lv_coord_t mid_y = row_y + row_h / 2;
        lv_coord_t dy = mid_y - DISPLAY_CY;
        lv_coord_t ady = dy < 0 ? -dy : dy;

        if (ady >= DISPLAY_R) {
            lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
            if (dy < 0) any_hidden_top = true;
            else        any_hidden_bottom = true;
            continue;
        }

        float fhalf = sqrtf((float)DISPLAY_R * DISPLAY_R - (float)ady * ady);
        lv_coord_t half_w = (lv_coord_t)fhalf;
        lv_coord_t avail_w = half_w * 2;
        if (avail_w > MAX_ROW_W) avail_w = MAX_ROW_W;

        if (avail_w < MIN_VISIBLE_W) {
            lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
            if (dy < 0) any_hidden_top = true;
            else        any_hidden_bottom = true;
            continue;
        }

        lv_obj_clear_flag(row, LV_OBJ_FLAG_HIDDEN);

        lv_coord_t new_x = (480 - avail_w) / 2;
        lv_obj_set_width(row, avail_w);
        lv_obj_set_x(row, new_x);

        lv_opa_t opa;
        if (ady < 60) {
            opa = LV_OPA_COVER;
        } else {
            float fade = 1.0f - (float)(ady - 60) / (float)(DISPLAY_R - 60);
            if (fade < 0.15f) fade = 0.15f;
            opa = (lv_opa_t)(fade * 255);
        }
        lv_obj_set_style_opa(row, opa, 0);
    }

    if (any_hidden_top) lv_obj_clear_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);
    if (any_hidden_bottom) lv_obj_clear_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);
}

static void radial_timer_cb(lv_timer_t *timer) { (void)timer; apply_radial_scroll(); }

static void initial_scroll_cb(lv_timer_t *timer)
{
    (void)timer;
    if (focused_row) {
        lv_obj_scroll_to_view(focused_row, LV_ANIM_OFF);
        apply_radial_scroll();
        update_focus_styles();
    }
}

static void scroll_cb(lv_event_t *e) { (void)e; update_arrow_visibility(); }

lv_obj_t *screen_menu_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, LV_COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Menu");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    /* Scrollable container for menu rows */
    menu_container = lv_obj_create(screen);
    lv_obj_set_size(menu_container, 480, 480);
    lv_obj_align(menu_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(menu_container, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(menu_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(menu_container, 0, 0);
    lv_obj_set_style_pad_all(menu_container, 0, 0);
    lv_obj_set_scroll_dir(menu_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(menu_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_y(menu_container, LV_SCROLL_SNAP_CENTER);
    lv_obj_add_event_cb(menu_container, scroll_cb, LV_EVENT_SCROLL, NULL);

    /* Arrow indicators */
    arrow_up_label = lv_label_create(screen);
    lv_label_set_text(arrow_up_label, LV_SYMBOL_UP);
    lv_obj_set_style_text_color(arrow_up_label, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(arrow_up_label, &lv_font_montserrat_16, 0);
    lv_obj_align(arrow_up_label, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_add_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);

    arrow_down_label = lv_label_create(screen);
    lv_label_set_text(arrow_down_label, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(arrow_down_label, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(arrow_down_label, &lv_font_montserrat_16, 0);
    lv_obj_align(arrow_down_label, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_add_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);

    /* Bottom count label */
    count_label = lv_label_create(screen);
    lv_label_set_text(count_label, "");
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(count_label, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, -60);

    /* Create menu rows */
    for (int i = 0; i < menu_count; i++) {
        create_menu_row(menu_container, i);
        lv_obj_set_pos(row_objects[i], (480 - MAX_ROW_W) / 2, i * ROW_SPACING);
    }

    focused_row = row_objects[selected_index];
    update_focus_styles();
    update_menu_count();
    update_arrow_visibility();

    apply_radial_scroll();
    radial_timer = lv_timer_create(radial_timer_cb, 50, NULL);

    lv_timer_t *init_timer = lv_timer_create(initial_scroll_cb, 50, NULL);
    init_timer->repeat_count = 1;

    ESP_LOGI(TAG, "Menu screen created with %d items", menu_count);
    return screen;
}

void screen_menu_encoder_event(lv_indev_data_t *data)
{
    if (!menu_container) return;

    if (data->enc_diff != 0) {
        int new_idx = selected_index + data->enc_diff;
        if (new_idx < 0) new_idx = menu_count - 1;
        if (new_idx >= menu_count) new_idx = 0;

        selected_index = new_idx;
        focused_row = row_objects[selected_index];
        update_focus_styles();
        lv_obj_scroll_to_view(focused_row, LV_ANIM_ON);
    }

    if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
        screen_id_t target = screen_menu_get_selection();
        if (target != SCREEN_COUNT) {
            ui_manager_switch_screen(target);
        }
    }
}

screen_id_t screen_menu_get_selection(void)
{
    if (selected_index >= 0 && selected_index < menu_count) {
        return menu_items[selected_index].target;
    }
    return SCREEN_COUNT;
}
