#include "screen_settings.h"
#include "ui_manager.h"
#include "color_palette.h"
#include "../app_state.h"
#include "../utils/persistence.h"
#include "ST7701S.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *TAG = "Screen_Settings";

#define DISPLAY_R    240
#define DISPLAY_CY   240
#define ROW_SPACING  64
#define MAX_ROW_W    420
#define MIN_VISIBLE_W 120
#define ROW_INNER_PAD 12
#define ROW_COL_GAP   10

#define NUM_SETTINGS 3

typedef enum {
    SETTING_BRIGHTNESS = 0,
    SETTING_VOLUME,
    SETTING_TIMEOUT
} setting_id_t;

static const char *setting_names[NUM_SETTINGS] = {
    "Brightness", "Volume", "Idle Timeout"
};
static const int setting_min[NUM_SETTINGS] = { 0, 0, 0 };
static const int setting_max[NUM_SETTINGS] = { 100, 100, 120 };
static const int setting_step[NUM_SETTINGS] = { 1, 1, 5 };

static lv_obj_t *screen;
static lv_obj_t *settings_container;
static lv_obj_t *arrow_up_label;
static lv_obj_t *arrow_down_label;
static lv_obj_t *hint_label;

static lv_obj_t *setting_rows[NUM_SETTINGS];
static lv_obj_t *setting_labels[NUM_SETTINGS];
static lv_obj_t *setting_values[NUM_SETTINGS];

static lv_obj_t *focused_row;
static int focused_index = 0;
static int editing_index = -1;
static lv_timer_t *radial_timer;

static void update_focus_styles(void);
static void update_arrow_visibility(void);
static void apply_radial_scroll(void);
static void radial_timer_cb(lv_timer_t *timer);
static void initial_scroll_cb(lv_timer_t *timer);

static int *setting_value_ptr(int index)
{
    settings_t *s = &app_state_get()->settings;
    switch (index) {
        case SETTING_BRIGHTNESS: return &s->brightness;
        case SETTING_VOLUME:     return &s->volume;
        case SETTING_TIMEOUT:    return &s->idle_timeout;
        default:                 return NULL;
    }
}

static void update_value_label(int index)
{
    lv_obj_t *lbl = setting_values[index];
    int *vp = setting_value_ptr(index);
    if (!lbl || !vp) return;

    if (index == SETTING_TIMEOUT && *vp == 0) {
        lv_label_set_text(lbl, "Off");
    } else if (index == SETTING_TIMEOUT) {
        lv_label_set_text_fmt(lbl, "%d min", *vp);
    } else {
        lv_label_set_text_fmt(lbl, "%d%%", *vp);
    }
}

static void apply_setting_change(int index)
{
    settings_t *s = &app_state_get()->settings;
    if (index == SETTING_BRIGHTNESS) {
        Set_Backlight((uint8_t)s->brightness);
    }
    persistence_mark_dirty();
    app_state_broadcast_settings_sync();
}

static lv_obj_t *create_setting_row(lv_obj_t *parent, int index)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, MAX_ROW_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_row(row, 0, 0);
    lv_obj_set_style_pad_column(row, ROW_COL_GAP, 0);
    lv_obj_set_style_pad_all(row, ROW_INNER_PAD, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_min_width(row, 0, 0);
    setting_rows[index] = row;

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, setting_names[index]);
    lv_obj_set_flex_grow(label, 1);
    lv_obj_set_style_text_color(label, LV_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_opa(label, LV_OPA_80, 0);
    lv_obj_set_style_pad_ver(row, 6, 0);
    setting_labels[index] = label;

    lv_obj_t *val = lv_label_create(row);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(val, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_set_style_pad_left(val, 8, 0);
    lv_obj_set_style_pad_right(val, 8, 0);
    lv_obj_set_style_pad_ver(val, 2, 0);
    lv_obj_set_style_bg_opa(val, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(val, 0, 0);
    lv_obj_set_style_border_color(val, theme_accent(SCREEN_SETTINGS), 0);
    setting_values[index] = val;
    update_value_label(index);

    return row;
}

static void update_focus_styles(void)
{
    for (int i = 0; i < NUM_SETTINGS; i++) {
        lv_obj_t *row = setting_rows[i];
        lv_obj_t *label = setting_labels[i];
        lv_obj_t *val = setting_values[i];
        if (!row || !label || !val) continue;

        bool focused = (row == focused_row);
        bool editing = (i == editing_index);

        if (focused) {
            lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
            lv_obj_set_style_text_color(label, LV_COLOR_TEXT, 0);
            lv_obj_set_style_pad_ver(row, 12, 0);
            lv_obj_set_style_text_font(val, &lv_font_montserrat_20, 0);
        } else {
            lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
            lv_obj_set_style_text_opa(label, LV_OPA_80, 0);
            lv_obj_set_style_pad_ver(row, 6, 0);
            lv_obj_set_style_text_font(val, &lv_font_montserrat_16, 0);
        }

        if (editing) {
            lv_obj_set_style_bg_color(val, theme_accent(SCREEN_SETTINGS), 0);
            lv_obj_set_style_bg_opa(val, LV_OPA_30, 0);
            lv_obj_set_style_border_width(val, 3, 0);
            lv_obj_set_style_border_color(val, theme_accent(SCREEN_SETTINGS), 0);
            lv_obj_set_style_radius(val, 8, 0);
            lv_obj_set_style_text_color(val, theme_accent(SCREEN_SETTINGS), 0);
        } else {
            lv_obj_set_style_bg_opa(val, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(val, 0, 0);
            lv_obj_set_style_radius(val, 0, 0);
            lv_obj_set_style_text_color(val, focused ? theme_accent(SCREEN_SETTINGS) : LV_COLOR_TEXT_SECONDARY, 0);
        }
    }
}

static void update_arrow_visibility(void)
{
    lv_coord_t st = lv_obj_get_scroll_top(settings_container);
    lv_coord_t sb = lv_obj_get_scroll_bottom(settings_container);
    if (st <= 0) lv_obj_add_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_clear_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);
    if (sb <= 0) lv_obj_add_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_clear_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);
}

static void update_hint(void)
{
    if (!hint_label) return;
    if (editing_index >= 0) {
        lv_label_set_text(hint_label, "Rotate: adjust    Press: done");
    } else {
        lv_label_set_text(hint_label, "Rotate: move    Press: edit");
    }
}

static void apply_radial_scroll(void)
{
    if (!settings_container) return;

    lv_obj_update_layout(settings_container);

    bool any_hidden_top = false;
    bool any_hidden_bottom = false;

    for (int i = 0; i < NUM_SETTINGS; i++) {
        lv_obj_t *row = setting_rows[i];
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

lv_obj_t *screen_settings_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, pastel_color(theme_accent(SCREEN_SETTINGS)), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    settings_container = lv_obj_create(screen);
    lv_obj_set_size(settings_container, 480, 480);
    lv_obj_align(settings_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(settings_container, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(settings_container, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_bg_opa(settings_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(settings_container, 0, 0);
    lv_obj_set_style_pad_all(settings_container, 0, 0);
    lv_obj_set_scroll_dir(settings_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(settings_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_y(settings_container, LV_SCROLL_SNAP_CENTER);
    lv_obj_add_event_cb(settings_container, scroll_cb, LV_EVENT_SCROLL, NULL);

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

    hint_label = lv_label_create(screen);
    lv_label_set_text(hint_label, "Rotate: move    Press: edit");
    lv_obj_set_style_text_font(hint_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint_label, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(hint_label, LV_ALIGN_BOTTOM_MID, 0, -60);

    for (int i = 0; i < NUM_SETTINGS; i++) {
        create_setting_row(settings_container, i);
        lv_obj_set_pos(setting_rows[i], (480 - MAX_ROW_W) / 2, i * ROW_SPACING);
    }

    focused_index = 0;
    focused_row = setting_rows[0];
    editing_index = -1;

    update_focus_styles();
    update_hint();
    update_arrow_visibility();

    apply_radial_scroll();
    radial_timer = lv_timer_create(radial_timer_cb, 50, NULL);

    lv_timer_t *init_timer = lv_timer_create(initial_scroll_cb, 50, NULL);
    init_timer->repeat_count = 1;

    ESP_LOGI(TAG, "Settings screen created");
    return screen;
}

void screen_settings_refresh(void)
{
    if (!screen || !settings_container) return;

    for (int i = 0; i < NUM_SETTINGS; i++) {
        update_value_label(i);
    }

    focused_index = 0;
    focused_row = setting_rows[0];
    editing_index = -1;

    update_focus_styles();
    update_hint();
    update_arrow_visibility();

    lv_obj_scroll_to_y(settings_container, 0, LV_ANIM_OFF);
    if (focused_row) {
        lv_obj_update_layout(settings_container);
        lv_obj_scroll_to_view(focused_row, LV_ANIM_OFF);
    }
    apply_radial_scroll();

    ESP_LOGI(TAG, "Settings screen refreshed");
}

void screen_settings_encoder_event(lv_indev_data_t *data)
{
    if (!settings_container) return;

    if (editing_index >= 0) {
        /* Edit mode: rotate adjusts the value, press commits */
        if (data->enc_diff != 0) {
            int *vp = setting_value_ptr(editing_index);
            if (vp) {
                int step = setting_step[editing_index];
                *vp += data->enc_diff * step;
                if (*vp < setting_min[editing_index]) *vp = setting_min[editing_index];
                if (*vp > setting_max[editing_index]) *vp = setting_max[editing_index];
                update_value_label(editing_index);
                apply_setting_change(editing_index);
            }
        }

        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            editing_index = -1;
            update_focus_styles();
            update_hint();
        }
        return;
    }

    /* Navigation mode: rotate moves focus between rows */
    if (data->enc_diff != 0) {
        int new_idx = focused_index + data->enc_diff;
        if (new_idx < 0) new_idx = NUM_SETTINGS - 1;
        if (new_idx >= NUM_SETTINGS) new_idx = 0;
        focused_index = new_idx;
        focused_row = setting_rows[new_idx];
        update_focus_styles();
        lv_obj_scroll_to_view(focused_row, LV_ANIM_ON);
    }

    /* Press starts editing the focused row */
    if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
        editing_index = focused_index;
        update_focus_styles();
        update_hint();
    }
}
