#include "screen_sedentary.h"
#include "ui_manager.h"
#include "color_palette.h"
#include "studybud_theme.h"
#include "../app_state.h"
#include "../utils/sedentary_store.h"
#include "../utils/points_store.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static const char *TAG = "Screen_Sedentary";

#define ARC_SIZE      300
#define ARC_OFFSET_Y  -25
#define FOCUS_ANIM_MS 200

#define ARC_START_ANGLE 135
#define ARC_END_ANGLE   405
#define ARC_RANGE_ANGLE (ARC_END_ANGLE - ARC_START_ANGLE)

/* Radial list constants (mirror screen_menu) */
#define DISPLAY_R     240
#define DISPLAY_CY    240
#define ROW_SPACING   64
#define MAX_ROW_W     420
#define MIN_VISIBLE_W 120
#define FADE_ZONE     60

#define BTN_START_W   240
#define BTN_START_H   120
#define BTN_PILL_W    200
#define BTN_PILL_H    46
#define BTN_PAUSE_W   190
#define BTN_PAUSE_H   50

static lv_obj_t *screen = NULL;

/* --- IDLE group --- */
static lv_obj_t *grp_idle = NULL;
static lv_obj_t *lbl_chip_today = NULL;
static lv_obj_t *lbl_streak = NULL;
static lv_obj_t *btn_start = NULL;
static lv_obj_t *lbl_start_sub = NULL;
static lv_obj_t *btn_enabled = NULL;
static lv_obj_t *lbl_enabled_text = NULL;

/* --- RUNNING group --- */
static lv_obj_t *grp_running = NULL;
static lv_obj_t *lbl_chip_next = NULL;
static lv_obj_t *arc = NULL;
static lv_obj_t *lbl_countdown = NULL;
static lv_obj_t *btn_pause = NULL;
static lv_obj_t *lbl_pause_text = NULL;

/* --- BREAK group --- */
static lv_obj_t *grp_break = NULL;
static lv_obj_t *break_container = NULL;
static lv_obj_t *arrow_up = NULL;
static lv_obj_t *arrow_down = NULL;
static lv_obj_t *lbl_break_hint = NULL;
static lv_obj_t *row_objects[SEDENTARY_MAX_EXERCISES];
static lv_obj_t *row_icons[SEDENTARY_MAX_EXERCISES];
static lv_obj_t *row_labels[SEDENTARY_MAX_EXERCISES];
static lv_obj_t *focused_row = NULL;
static lv_timer_t *radial_timer = NULL;

/* --- SUMMARY group --- */
static lv_obj_t *grp_summary = NULL;
static lv_obj_t *lbl_plus = NULL;
static lv_obj_t *lbl_plus_sub = NULL;
static lv_obj_t *lbl_sum_breaks = NULL;
static lv_obj_t *lbl_sum_streak = NULL;

static int focus_idx = 0;
static int prev_state = -1;
static int prev_exercise_count = -1;
static int last_awarded_pts = 0;
static int built_rows = 0;

static void refresh(void);
static void update_arc(void);
static void update_focus_styles(void);
static void scroll_to_focused(void);
static void format_time(char *buf, size_t len, uint32_t sec);

static void anim_set_border_width(void *var, int32_t val)
{
    if (var) lv_obj_set_style_border_width((lv_obj_t *)var, val, 0);
}

static void anim_start_border(lv_obj_t *obj, int32_t from, int32_t to, uint32_t time)
{
    if (!obj || !lv_obj_is_valid(obj)) return;
    lv_anim_del(obj, (lv_anim_exec_xcb_t)anim_set_border_width);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_set_border_width);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, time);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void format_time(char *buf, size_t len, uint32_t sec)
{
    uint32_t h = sec / 3600;
    uint32_t m = (sec % 3600) / 60;
    uint32_t s = sec % 60;
    if (h > 0) {
        snprintf(buf, len, "%02lu:%02lu:%02lu",
                 (unsigned long)h, (unsigned long)m, (unsigned long)s);
    } else {
        snprintf(buf, len, "%02lu:%02lu",
                 (unsigned long)m, (unsigned long)s);
    }
}

/* ============================================================ */
/* Radial list (mirrors screen_menu)                            */
/* ============================================================ */

static void create_break_row(int index)
{
    sedentary_state_t *sed = sedentary_store_get_state();

    lv_obj_t *row = lv_obj_create(break_container);
    lv_obj_set_size(row, MAX_ROW_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_row(row, 0, 0);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_style_pad_all(row, 12, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_min_width(row, 0, 0);
    row_objects[index] = row;

    lv_obj_t *icon = lv_label_create(row);
    lv_label_set_text(icon, LV_SYMBOL_OK);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(icon, theme_accent(SCREEN_SEDENTARY), 0);
    lv_obj_set_style_text_opa(icon, LV_OPA_80, 0);
    row_icons[index] = icon;

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, sed->exercises[index]);
    lv_obj_set_flex_grow(label, 1);
    lv_obj_set_style_text_color(label, LV_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_80, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_pad_ver(row, 6, 0);
    row_labels[index] = label;
}

static void rebuild_break_rows(void)
{
    sedentary_state_t *sed = sedentary_store_get_state();
    int count = sed->exercise_count;
    if (count > SEDENTARY_MAX_EXERCISES) count = SEDENTARY_MAX_EXERCISES;

    for (int i = built_rows; i < count; i++) {
        create_break_row(i);
        lv_obj_set_pos(row_objects[i], (480 - MAX_ROW_W) / 2, i * ROW_SPACING);
    }
    for (int i = count; i < built_rows; i++) {
        if (row_objects[i]) lv_obj_add_flag(row_objects[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < count; i++) {
        if (row_objects[i]) {
            lv_obj_clear_flag(row_objects[i], LV_OBJ_FLAG_HIDDEN);
            if (row_labels[i]) lv_label_set_text(row_labels[i], sed->exercises[i]);
        }
    }
    built_rows = count;

    if (count > 0) {
        focused_row = row_objects[0];
    } else {
        focused_row = NULL;
    }
}

static void apply_radial_scroll(void)
{
    if (!break_container) return;
    sedentary_state_t *sed = sedentary_store_get_state();
    int count = sed->exercise_count;
    if (count > SEDENTARY_MAX_EXERCISES) count = SEDENTARY_MAX_EXERCISES;

    lv_obj_update_layout(break_container);

    bool any_hidden_top = false;
    bool any_hidden_bottom = false;

    for (int i = 0; i < count; i++) {
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
        if (ady < FADE_ZONE) {
            opa = LV_OPA_COVER;
        } else {
            float fade = 1.0f - (float)(ady - FADE_ZONE) / (float)(DISPLAY_R - FADE_ZONE);
            if (fade < 0.15f) fade = 0.15f;
            opa = (lv_opa_t)(fade * 255);
        }
        lv_obj_set_style_opa(row, opa, 0);
    }

    if (any_hidden_top) lv_obj_clear_flag(arrow_up, LV_OBJ_FLAG_HIDDEN);
    else                lv_obj_add_flag(arrow_up, LV_OBJ_FLAG_HIDDEN);
    if (any_hidden_bottom) lv_obj_clear_flag(arrow_down, LV_OBJ_FLAG_HIDDEN);
    else                   lv_obj_add_flag(arrow_down, LV_OBJ_FLAG_HIDDEN);
}

static void radial_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    apply_radial_scroll();
}

static void initial_scroll_cb(lv_timer_t *timer)
{
    (void)timer;
    scroll_to_focused();
    apply_radial_scroll();
    update_focus_styles();
}

static void scroll_cb(lv_event_t *e)
{
    (void)e;
    apply_radial_scroll();
}

/* ============================================================ */
/* Focus / state                                                */
/* ============================================================ */

static int focus_count_for_state(int state)
{
    switch (state) {
    case SEDENTARY_UI_IDLE:
        return 2;
    case SEDENTARY_UI_RUNNING:
        return 1;
    case SEDENTARY_UI_BREAK:
        return sedentary_store_get_state()->exercise_count;
    default:
        return 0;
    }
}

static void update_focus_styles(void)
{
    sedentary_state_t *sed = sedentary_store_get_state();
    int state = sed->ui_state;
    lv_color_t accent = theme_accent(SCREEN_SEDENTARY);
    lv_color_t accent_dark = theme_accent_dark(SCREEN_SEDENTARY);

    if (state == SEDENTARY_UI_IDLE) {
        lv_obj_t *btns[2] = { btn_start, btn_enabled };
        for (int i = 0; i < 2; i++) {
            if (!btns[i]) continue;
            bool focused = (i == focus_idx);
            lv_color_t bg = focused ? accent : accent_dark;
            lv_obj_set_style_bg_color(btns[i], bg, 0);
            if (btn_start && btn_start == btns[i] && lbl_start_sub) {
                lv_obj_set_style_text_color(lbl_start_sub, contrast_text_color(bg), 0);
            }
            if (lbl_enabled_text && btn_enabled == btns[i]) {
                lv_obj_set_style_text_color(lbl_enabled_text, contrast_text_color(bg), 0);
            }
            anim_start_border(btns[i],
                lv_obj_get_style_border_width(btns[i], 0),
                focused ? 3 : 0, FOCUS_ANIM_MS);
        }
    } else if (state == SEDENTARY_UI_RUNNING) {
        if (btn_pause) {
            bool focused = (focus_idx == 0);
            lv_color_t bg = focused ? accent : accent_dark;
            lv_obj_set_style_bg_color(btn_pause, bg, 0);
            if (lbl_pause_text) {
                lv_obj_set_style_text_color(lbl_pause_text, contrast_text_color(bg), 0);
            }
            anim_start_border(btn_pause,
                lv_obj_get_style_border_width(btn_pause, 0),
                focused ? 3 : 0, FOCUS_ANIM_MS);
        }
    } else if (state == SEDENTARY_UI_BREAK) {
        int count = sed->exercise_count;
        if (count > SEDENTARY_MAX_EXERCISES) count = SEDENTARY_MAX_EXERCISES;
        for (int i = 0; i < count; i++) {
            lv_obj_t *label = row_labels[i];
            lv_obj_t *icon = row_icons[i];
            lv_obj_t *row = row_objects[i];
            if (!label || !icon || !row) continue;

            if (row == focused_row) {
                lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
                lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
                lv_obj_set_style_text_color(label, darken_text_color(accent, 0.5f), 0);
                lv_obj_set_style_text_opa(icon, LV_OPA_COVER, 0);
                lv_obj_set_style_pad_ver(row, 12, 0);
                lv_obj_set_style_bg_opa(row, LV_OPA_20, 0);
                lv_obj_set_style_bg_color(row, accent, 0);
                lv_obj_set_style_radius(row, 12, 0);
            } else {
                lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
                lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
                lv_obj_set_style_text_color(label, LV_COLOR_TEXT, 0);
                lv_obj_set_style_text_opa(icon, LV_OPA_80, 0);
                lv_obj_set_style_pad_ver(row, 6, 0);
                lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
            }
        }
    }
}

static void scroll_to_focused(void)
{
    if (!focused_row || !break_container) return;
    lv_obj_scroll_to_view(focused_row, LV_ANIM_OFF);
    apply_radial_scroll();
}

/* ============================================================ */
/* Widget builders                                               */
/* ============================================================ */

static void style_pill(lv_obj_t *btn)
{
    lv_obj_set_style_radius(btn, lv_obj_get_height(btn) / 2, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_set_style_border_color(btn, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_pad_all(btn, 0, 0);
}

static void build_idle_group(void)
{
    grp_idle = lv_obj_create(screen);
    lv_obj_set_size(grp_idle, 480, 480);
    lv_obj_align(grp_idle, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(grp_idle, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grp_idle, 0, 0);
    lv_obj_clear_flag(grp_idle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(grp_idle, 0, 0);

    lbl_chip_today = lv_label_create(grp_idle);
    lv_label_set_text(lbl_chip_today, "Today 0 / 5 breaks");
    lv_obj_set_style_text_font(lbl_chip_today, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_chip_today, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_chip_today, LV_ALIGN_TOP_MID, 0, 90);

    lbl_streak = lv_label_create(grp_idle);
    lv_label_set_text(lbl_streak, "Move streak: 0 days");
    lv_obj_set_style_text_font(lbl_streak, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_streak, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lbl_streak, LV_ALIGN_TOP_MID, 0, 120);

    btn_start = lv_btn_create(grp_idle);
    lv_obj_set_size(btn_start, BTN_START_W, BTN_START_H);
    lv_obj_align(btn_start, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_radius(btn_start, 32, 0);
    lv_obj_set_style_bg_color(btn_start, theme_accent_dark(SCREEN_SEDENTARY), 0);
    lv_obj_set_style_shadow_width(btn_start, 0, 0);
    lv_obj_set_style_shadow_opa(btn_start, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_start, 0, 0);
    lv_obj_set_style_border_color(btn_start, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_pad_all(btn_start, 0, 0);

    lv_obj_t *lbl_start_main = lv_label_create(btn_start);
    lv_label_set_text(lbl_start_main, "Start");
    lv_obj_set_style_text_font(lbl_start_main, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_start_main, LV_COLOR_BG_CARD, 0);
    lv_obj_align(lbl_start_main, LV_ALIGN_CENTER, 0, -14);

    lbl_start_sub = lv_label_create(btn_start);
    lv_label_set_text(lbl_start_sub, "60 min countdown");
    lv_obj_set_style_text_font(lbl_start_sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_start_sub, LV_COLOR_BG_CARD, 0);
    lv_obj_align(lbl_start_sub, LV_ALIGN_CENTER, 0, 18);

    btn_enabled = lv_btn_create(grp_idle);
    lv_obj_set_size(btn_enabled, BTN_PILL_W, BTN_PILL_H);
    lv_obj_align(btn_enabled, LV_ALIGN_BOTTOM_MID, 0, -42);
    style_pill(btn_enabled);
    lv_obj_set_style_bg_color(btn_enabled, LV_COLOR_SURFACE, 0);

    lbl_enabled_text = lv_label_create(btn_enabled);
    lv_label_set_text(lbl_enabled_text, "Enabled: ON");
    lv_obj_set_style_text_font(lbl_enabled_text, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_enabled_text, theme_accent_dark(SCREEN_SEDENTARY), 0);
    lv_obj_center(lbl_enabled_text);

    lv_obj_t *hint = lv_label_create(grp_idle);
    lv_label_set_text(hint, "Long-press encoder for menu");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -120);
}

static void build_running_group(void)
{
    grp_running = lv_obj_create(screen);
    lv_obj_set_size(grp_running, 480, 480);
    lv_obj_align(grp_running, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(grp_running, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grp_running, 0, 0);
    lv_obj_clear_flag(grp_running, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(grp_running, 0, 0);

    lbl_chip_next = lv_label_create(grp_running);
    lv_label_set_text(lbl_chip_next, "Next break 60 min");
    lv_obj_set_style_text_font(lbl_chip_next, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_chip_next, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_chip_next, LV_ALIGN_TOP_MID, 0, 90);

    arc = lv_arc_create(grp_running);
    lv_obj_set_size(arc, ARC_SIZE, ARC_SIZE);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, ARC_OFFSET_Y);
    lv_arc_set_bg_angles(arc, ARC_START_ANGLE, ARC_END_ANGLE);
    lv_arc_set_angles(arc, ARC_START_ANGLE, ARC_START_ANGLE);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_obj_set_style_border_width(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, theme_accent_dark(SCREEN_SEDENTARY), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, theme_accent(SCREEN_SEDENTARY), LV_PART_INDICATOR);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    lbl_countdown = lv_label_create(grp_running);
    lv_label_set_text(lbl_countdown, "00:00");
    lv_obj_set_style_text_font(lbl_countdown, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_countdown, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_countdown, LV_ALIGN_CENTER, 0, ARC_OFFSET_Y - 12);

    lv_obj_t *sub = lv_label_create(grp_running);
    lv_label_set_text(sub, "until break");
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sub, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, ARC_OFFSET_Y + 34);

    btn_pause = lv_btn_create(grp_running);
    lv_obj_set_size(btn_pause, BTN_PAUSE_W, BTN_PAUSE_H);
    lv_obj_align(btn_pause, LV_ALIGN_BOTTOM_MID, 0, -42);
    style_pill(btn_pause);
    lv_obj_set_style_bg_color(btn_pause, theme_accent_dark(SCREEN_SEDENTARY), 0);

    lbl_pause_text = lv_label_create(btn_pause);
    lv_label_set_text(lbl_pause_text, "Pause");
    lv_obj_set_style_text_font(lbl_pause_text, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_pause_text, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_pause_text);
}

static void build_break_group(void)
{
    grp_break = lv_obj_create(screen);
    lv_obj_set_size(grp_break, 480, 480);
    lv_obj_align(grp_break, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(grp_break, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grp_break, 0, 0);
    lv_obj_clear_flag(grp_break, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(grp_break, 0, 0);

    lv_obj_t *title = lv_label_create(grp_break);
    lv_label_set_text(title, "Select what you did");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    break_container = lv_obj_create(grp_break);
    lv_obj_set_size(break_container, 480, 480);
    lv_obj_align(break_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(break_container, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(break_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(break_container, 0, 0);
    lv_obj_set_style_pad_all(break_container, 0, 0);
    lv_obj_set_scroll_dir(break_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(break_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_y(break_container, LV_SCROLL_SNAP_CENTER);
    lv_obj_add_event_cb(break_container, scroll_cb, LV_EVENT_SCROLL, NULL);

    arrow_up = lv_label_create(grp_break);
    lv_label_set_text(arrow_up, LV_SYMBOL_UP);
    lv_obj_set_style_text_color(arrow_up, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(arrow_up, &lv_font_montserrat_16, 0);
    lv_obj_align(arrow_up, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_add_flag(arrow_up, LV_OBJ_FLAG_HIDDEN);

    arrow_down = lv_label_create(grp_break);
    lv_label_set_text(arrow_down, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(arrow_down, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(arrow_down, &lv_font_montserrat_16, 0);
    lv_obj_align(arrow_down, LV_ALIGN_BOTTOM_MID, 0, -60);
    lv_obj_add_flag(arrow_down, LV_OBJ_FLAG_HIDDEN);

    lbl_break_hint = lv_label_create(grp_break);
    lv_label_set_text(lbl_break_hint, "Press to confirm");
    lv_obj_set_style_text_font(lbl_break_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_break_hint, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_break_hint, LV_ALIGN_BOTTOM_MID, 0, -108);

    if (!radial_timer) {
        radial_timer = lv_timer_create(radial_timer_cb, 50, NULL);
        lv_timer_pause(radial_timer);
    }
}

static void build_summary_group(void)
{
    grp_summary = lv_obj_create(screen);
    lv_obj_set_size(grp_summary, 480, 480);
    lv_obj_align(grp_summary, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_opa(grp_summary, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grp_summary, 0, 0);
    lv_obj_clear_flag(grp_summary, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(grp_summary, 0, 0);

    lv_obj_t *title = lv_label_create(grp_summary);
    lv_label_set_text(title, "Stretch complete");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 70);

    lbl_plus = lv_label_create(grp_summary);
    lv_label_set_text(lbl_plus, "+10 seeds");
    lv_obj_set_style_text_font(lbl_plus, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_plus, theme_accent(SCREEN_SEDENTARY), 0);
    lv_obj_align(lbl_plus, LV_ALIGN_CENTER, 0, -20);

    lbl_plus_sub = lv_label_create(grp_summary);
    lv_label_set_text(lbl_plus_sub, "");
    lv_obj_set_style_text_font(lbl_plus_sub, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_plus_sub, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lbl_plus_sub, LV_ALIGN_CENTER, 0, 20);

    lv_obj_t *card = lv_obj_create(grp_summary);
    lv_obj_set_size(card, 320, 96);
    lv_obj_align(card, LV_ALIGN_CENTER, 0, 78);
    lv_obj_set_style_radius(card, 16, 0);
    lv_obj_set_style_bg_color(card, LV_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_pad_all(card, 12, 0);

    lbl_sum_breaks = lv_label_create(card);
    lv_label_set_text(lbl_sum_breaks, "Breaks today   0 / 5");
    lv_obj_set_style_text_font(lbl_sum_breaks, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_sum_breaks, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_sum_breaks, LV_ALIGN_TOP_LEFT, 12, 12);

    lbl_sum_streak = lv_label_create(card);
    lv_label_set_text(lbl_sum_streak, "Move streak   0 days");
    lv_obj_set_style_text_font(lbl_sum_streak, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_sum_streak, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_sum_streak, LV_ALIGN_TOP_LEFT, 12, 56);

    lv_obj_t *hint = lv_label_create(grp_summary);
    lv_label_set_text(hint, "Press to start next countdown");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -42);
}

/* ============================================================ */
/* Refresh / state switching                                     */
/* ============================================================ */

static void update_arc(void)
{
    if (!arc) return;
    sedentary_state_t *sed = sedentary_store_get_state();
    if (sed->total_sec == 0) {
        lv_arc_set_angles(arc, ARC_START_ANGLE, ARC_START_ANGLE);
        return;
    }

    uint32_t elapsed = sed->total_sec - sed->remaining_sec;
    uint16_t end_angle = ARC_START_ANGLE +
        (uint16_t)(((uint64_t)ARC_RANGE_ANGLE * 1000 * elapsed) / sed->total_sec / 1000);
    if (end_angle > ARC_END_ANGLE) end_angle = ARC_END_ANGLE;
    lv_arc_set_angles(arc, ARC_START_ANGLE, end_angle);
}

static void show_state_group(int state)
{
    if (state < 0 || state > SEDENTARY_UI_SUMMARY) state = SEDENTARY_UI_IDLE;
    lv_obj_t *groups[4] = { grp_idle, grp_running, grp_break, grp_summary };
    for (int i = 0; i < 4; i++) {
        if (!groups[i]) continue;
        if (i == state) lv_obj_clear_flag(groups[i], LV_OBJ_FLAG_HIDDEN);
        else            lv_obj_add_flag(groups[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void refresh(void)
{
    sedentary_state_t *sed = sedentary_store_get_state();
    int state = sed->ui_state;

    if (state != prev_state) {
        prev_state = state;
        focus_idx = 0;
        if (state == SEDENTARY_UI_BREAK) {
            rebuild_break_rows();
            prev_exercise_count = sed->exercise_count;
            if (radial_timer) lv_timer_resume(radial_timer);
        } else if (radial_timer) {
            lv_timer_pause(radial_timer);
        }
    } else if (state == SEDENTARY_UI_BREAK &&
               sed->exercise_count != prev_exercise_count) {
        rebuild_break_rows();
        if (focus_idx >= sed->exercise_count) {
            focus_idx = sed->exercise_count > 0 ? sed->exercise_count - 1 : 0;
        }
        if (focus_idx >= 0 && focus_idx < sed->exercise_count) {
            focused_row = row_objects[focus_idx];
        }
        prev_exercise_count = sed->exercise_count;
    }

    points_state_t *ps = points_store_get_state();
    int move_streak = ps->streaks[STREAK_MOVE].streak;

    if (lbl_chip_today) {
        lv_label_set_text_fmt(lbl_chip_today, "Today %d / %d breaks",
                              sed->breaks_today, SEDENTARY_DAILY_CAP);
    }
    if (lbl_streak) {
        lv_label_set_text_fmt(lbl_streak, "Move streak: %d days", move_streak);
    }
    if (lbl_start_sub) {
        lv_label_set_text_fmt(lbl_start_sub, "%d min countdown", sed->interval_min);
    }
    if (lbl_enabled_text) {
        lv_label_set_text(lbl_enabled_text, sed->enabled ? "Enabled: ON" : "Enabled: OFF");
        lv_color_t ec = sed->enabled ? theme_accent_dark(SCREEN_SEDENTARY) : LV_COLOR_TEXT_MUTED;
        lv_obj_set_style_text_color(lbl_enabled_text, ec, 0);
    }
    if (lbl_chip_next) {
        lv_label_set_text_fmt(lbl_chip_next, "Next break %d min", sed->interval_min);
    }
    if (lbl_pause_text) {
        lv_label_set_text(lbl_pause_text, sed->manually_paused ? "Resume" : "Pause");
    }
    if (lbl_countdown) {
        char time_buf[16];
        format_time(time_buf, sizeof(time_buf), (uint32_t)sed->remaining_sec);
        lv_label_set_text(lbl_countdown, time_buf);
    }
    if (lbl_plus) {
        if (last_awarded_pts > 0) {
            lv_label_set_text_fmt(lbl_plus, "+%d seeds", last_awarded_pts);
        } else {
            lv_label_set_text(lbl_plus, "Good job!");
        }
    }
    if (lbl_plus_sub && sed->last_exercise_idx >= 0 &&
        sed->last_exercise_idx < sed->exercise_count) {
        lv_label_set_text(lbl_plus_sub, sed->exercises[sed->last_exercise_idx]);
    }
    if (lbl_sum_breaks) {
        lv_label_set_text_fmt(lbl_sum_breaks, "Breaks today   %d / %d",
                              sed->breaks_today, SEDENTARY_DAILY_CAP);
    }
    if (lbl_sum_streak) {
        lv_label_set_text_fmt(lbl_sum_streak, "Move streak   %d days", move_streak);
    }

    show_state_group(state);
    update_arc();
    update_focus_styles();
}

lv_obj_t *screen_sedentary_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, pastel_color(theme_accent(SCREEN_SEDENTARY)), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(screen);
    lv_label_set_text(title, "Stretch Break");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    focus_idx = 0;
    prev_state = -1;
    last_awarded_pts = 0;
    built_rows = 0;
    focused_row = NULL;
    memset(row_objects, 0, sizeof(row_objects));
    memset(row_icons, 0, sizeof(row_icons));
    memset(row_labels, 0, sizeof(row_labels));

    build_idle_group();
    build_running_group();
    build_break_group();
    build_summary_group();

    refresh();

    lv_timer_t *init_timer = lv_timer_create(initial_scroll_cb, 50, NULL);
    init_timer->repeat_count = 1;

    ESP_LOGI(TAG, "Stretch Break screen created");
    return screen;
}

void screen_sedentary_refresh(void)
{
    if (!screen) return;
    refresh();
}

void screen_sedentary_encoder_event(lv_indev_data_t *data)
{
    if (!data) return;
    sedentary_state_t *sed = sedentary_store_get_state();
    int state = sed->ui_state;
    int count = focus_count_for_state(state);
    bool is_pressed = (data->state == LV_INDEV_STATE_PR);

    if (data->enc_diff != 0) {
        if (count > 1) {
            focus_idx = ((focus_idx + data->enc_diff) % count + count) % count;
            if (state == SEDENTARY_UI_BREAK) {
                sedentary_state_t *s2 = sedentary_store_get_state();
                if (s2->exercise_count > 0 && focus_idx >= 0 &&
                    focus_idx < s2->exercise_count) {
                    focused_row = row_objects[focus_idx];
                    scroll_to_focused();
                }
            }
            update_focus_styles();
        }
    }

    if (is_pressed && data->enc_diff == 0) {
        switch (state) {
        case SEDENTARY_UI_IDLE:
            if (focus_idx == 0) {
                sedentary_store_start();
            } else {
                sedentary_store_set_enabled(!sed->enabled);
            }
            break;
        case SEDENTARY_UI_RUNNING:
            sedentary_store_toggle_pause();
            break;
        case SEDENTARY_UI_BREAK:
            if (sed->exercise_count > 0) {
                last_awarded_pts = sedentary_store_confirm_break(focus_idx);
            }
            break;
        case SEDENTARY_UI_SUMMARY:
            if (!sed->enabled) {
                sed->ui_state = SEDENTARY_UI_IDLE;
            } else {
                last_awarded_pts = 0;
                sedentary_store_acknowledge_summary();
            }
            break;
        default:
            break;
        }
        refresh();
    }
}
