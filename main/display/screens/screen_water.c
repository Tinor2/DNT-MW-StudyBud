#include "screen_water.h"
#include "ui_manager.h"
#include "studybud_theme.h"
#include "../app_state.h"
#include "../utils/persistence.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "Screen_Water";

#define FOCUS_ANIM_MS 300

typedef enum {
    WATER_BTN_MINUS,
    WATER_BTN_PLUS,
    WATER_BTN_GOAL,
    WATER_BTN_COUNT
} water_btn_t;

static lv_obj_t *screen = NULL;
static int focus_index = 0;
static int prev_focus_index = -1;

static lv_obj_t *lbl_title = NULL;
static lv_obj_t *lbl_count = NULL;
static lv_obj_t *lbl_status = NULL;
static lv_obj_t *bar_progress = NULL;
static lv_obj_t *btn_minus = NULL;
static lv_obj_t *btn_plus = NULL;
static lv_obj_t *btn_goal = NULL;
static lv_obj_t *lbl_celebrate = NULL;
static lv_timer_t *celebrate_timer = NULL;

static void anim_set_opa(void *var, int32_t val)
{
    if (var) lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)val, 0);
}

static void anim_set_border_width(void *var, int32_t val)
{
    if (var) lv_obj_set_style_border_width((lv_obj_t *)var, val, 0);
}

static void animate_style(lv_obj_t *obj, lv_anim_exec_xcb_t exec_cb,
                          int32_t from, int32_t to, uint32_t time, uint32_t delay)
{
    if (!obj) return;
    lv_anim_del(obj, exec_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, exec_cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, time);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void anim_set_bar_value(void *var, int32_t val)
{
    if (var) lv_bar_set_value((lv_obj_t *)var, val, LV_ANIM_OFF);
}

static void animate_bar_to(lv_obj_t *bar, int32_t to)
{
    if (!bar) return;
    int32_t from = lv_bar_get_value(bar);
    if (from == to) return;

    lv_anim_del(bar, (lv_anim_exec_xcb_t)anim_set_bar_value);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, bar);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_set_bar_value);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, 400);
    lv_anim_set_path_cb(&a, lv_anim_path_overshoot);
    lv_anim_start(&a);
}

static void celebrate_hide(void)
{
    if (celebrate_timer) {
        lv_timer_del(celebrate_timer);
        celebrate_timer = NULL;
    }
    if (lbl_celebrate) lv_obj_add_flag(lbl_celebrate, LV_OBJ_FLAG_HIDDEN);
}

static void celebrate_timer_cb(lv_timer_t *t)
{
    (void)t;
    celebrate_hide();
}

static void celebrate_show(void)
{
    if (!lbl_celebrate) return;
    lv_obj_clear_flag(lbl_celebrate, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(lbl_celebrate, LV_OPA_TRANSP, 0);
    animate_style(lbl_celebrate, (lv_anim_exec_xcb_t)anim_set_opa,
                  LV_OPA_TRANSP, LV_OPA_COVER, 350, 0);

    if (celebrate_timer) lv_timer_del(celebrate_timer);
    celebrate_timer = lv_timer_create(celebrate_timer_cb, 2500, NULL);
    lv_timer_set_repeat_count(celebrate_timer, 1);
}

static void refresh_water(void)
{
    app_state_t *state = app_state_get();

    lv_label_set_text_fmt(lbl_count, "%d", state->water.glasses);
    lv_label_set_text_fmt(lbl_status, "of %d glasses", state->water.goal);

    if (state->water.goal > 0) {
        int pct = (state->water.glasses * 100) / state->water.goal;
        if (pct > 100) pct = 100;
        animate_bar_to(bar_progress, pct);
    } else {
        animate_bar_to(bar_progress, 0);
    }

    if (state->water.glasses >= state->water.goal && state->water.goal > 0) {
        lv_obj_set_style_text_color(lbl_count, LV_COLOR_SUCCESS, 0);
    } else {
        lv_obj_set_style_text_color(lbl_count, LV_COLOR_WATER, 0);
    }
}

static void water_add(void)
{
    app_state_t *state = app_state_get();
    int old = state->water.glasses;
    state->water.glasses++;
    refresh_water();
    app_state_broadcast_water_sync();
    persistence_mark_dirty();
    if (state->water.goal > 0 && old < state->water.goal && state->water.glasses >= state->water.goal) {
        celebrate_show();
    }
    ESP_LOGI(TAG, "Water +1 -> %d", state->water.glasses);
}

static void water_remove(void)
{
    app_state_t *state = app_state_get();
    if (state->water.glasses > 0) state->water.glasses--;
    refresh_water();
    app_state_broadcast_water_sync();
    persistence_mark_dirty();
    ESP_LOGI(TAG, "Water -1 -> %d", state->water.glasses);
}

static void water_set_goal(int goal)
{
    app_state_t *state = app_state_get();
    if (goal < 1) goal = 1;
    if (goal > 32) goal = 32;
    state->water.goal = goal;
    refresh_water();
    app_state_broadcast_water_sync();
    persistence_mark_dirty();
    ESP_LOGI(TAG, "Water goal -> %d", goal);
}

static void cycle_goal(void)
{
    app_state_t *state = app_state_get();
    static const int goals[] = { 6, 8, 10, 12 };
    int next = goals[0];
    for (int i = 0; i < 4; i++) {
        if (state->water.goal < goals[i]) {
            next = goals[i];
            break;
        }
    }
    water_set_goal(next);
}

static void update_focus_styles(void)
{
    if (focus_index == prev_focus_index) return;

    lv_obj_t *btns[WATER_BTN_COUNT] = { btn_minus, btn_plus, btn_goal };
    for (int i = 0; i < WATER_BTN_COUNT; i++) {
        if (!btns[i]) continue;
        bool focused = (i == focus_index);
        animate_style(btns[i], (lv_anim_exec_xcb_t)anim_set_border_width,
                      focused ? 0 : 4, focused ? 4 : 0, FOCUS_ANIM_MS, 0);
        animate_style(btns[i], (lv_anim_exec_xcb_t)anim_set_opa,
                      focused ? LV_OPA_80 : LV_OPA_COVER,
                      focused ? LV_OPA_COVER : LV_OPA_80, FOCUS_ANIM_MS, 0);
    }

    prev_focus_index = focus_index;
}

static void make_button(lv_obj_t **btn, const char *text, int x, int y, int w, int h)
{
    *btn = lv_btn_create(screen);
    lv_obj_set_size(*btn, w, h);
    lv_obj_align(*btn, LV_ALIGN_CENTER, x, y);
    lv_obj_set_style_radius(*btn, h / 2, 0);
    lv_obj_set_style_bg_color(*btn, LV_COLOR_WATER, 0);
    lv_obj_set_style_shadow_width(*btn, 0, 0);
    lv_obj_set_style_shadow_opa(*btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(*btn, 0, 0);
    lv_obj_set_style_border_color(*btn, LV_COLOR_WATER, 0);
    lv_obj_set_style_pad_all(*btn, 0, 0);
    lv_obj_t *lbl = lv_label_create(*btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl);
}

lv_obj_t *screen_water_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, LV_COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    focus_index = 0;
    prev_focus_index = -1;

    lbl_title = lv_label_create(screen);
    lv_label_set_text(lbl_title, "Water");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 30);

    lbl_count = lv_label_create(screen);
    lv_label_set_text(lbl_count, "0");
    lv_obj_set_style_text_font(lbl_count, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_count, LV_COLOR_WATER, 0);
    lv_obj_align(lbl_count, LV_ALIGN_CENTER, 0, -60);

    lbl_status = lv_label_create(screen);
    lv_label_set_text(lbl_status, "");
    lv_obj_set_style_text_font(lbl_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_status, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lbl_status, LV_ALIGN_CENTER, 0, -10);

    lbl_celebrate = lv_label_create(screen);
    lv_label_set_text(lbl_celebrate, "Goal reached!");
    lv_obj_set_style_text_font(lbl_celebrate, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_celebrate, LV_COLOR_SUCCESS, 0);
    lv_obj_align(lbl_celebrate, LV_ALIGN_CENTER, 0, 15);
    lv_obj_add_flag(lbl_celebrate, LV_OBJ_FLAG_HIDDEN);

    bar_progress = lv_bar_create(screen);
    lv_obj_set_size(bar_progress, 320, 24);
    lv_obj_align(bar_progress, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_bg_color(bar_progress, LV_COLOR_SURFACE, 0);
    lv_obj_set_style_radius(bar_progress, 12, 0);
    lv_obj_set_style_pad_all(bar_progress, 3, 0);
    lv_bar_set_range(bar_progress, 0, 100);
    lv_bar_set_value(bar_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar_progress, LV_COLOR_WATER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_progress, 10, LV_PART_INDICATOR);

    make_button(&btn_minus, LV_SYMBOL_MINUS, -90, 120, 50, 50);
    make_button(&btn_plus, LV_SYMBOL_PLUS, 90, 120, 50, 50);

    btn_goal = lv_btn_create(screen);
    lv_obj_set_size(btn_goal, 160, 46);
    lv_obj_align(btn_goal, LV_ALIGN_CENTER, 0, 165);
    lv_obj_set_style_radius(btn_goal, 23, 0);
    lv_obj_set_style_bg_color(btn_goal, LV_COLOR_PRIMARY_DARK, 0);
    lv_obj_set_style_shadow_width(btn_goal, 0, 0);
    lv_obj_set_style_shadow_opa(btn_goal, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_goal, 0, 0);
    lv_obj_set_style_border_color(btn_goal, LV_COLOR_PRIMARY_LIGHT, 0);
    lv_obj_set_style_pad_all(btn_goal, 0, 0);
    lv_obj_t *lbl_goal = lv_label_create(btn_goal);
    lv_label_set_text(lbl_goal, "Set Goal");
    lv_obj_set_style_text_font(lbl_goal, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_goal, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_goal);

    refresh_water();
    update_focus_styles();

    ESP_LOGI(TAG, "Water screen created");
    return screen;
}

void screen_water_refresh(void)
{
    if (!screen) return;
    refresh_water();
}

void screen_water_encoder_event(lv_indev_data_t *data)
{
    if (!data) return;

    bool is_pressed = (data->state == LV_INDEV_STATE_PR);

    if (data->enc_diff != 0) {
        focus_index += data->enc_diff;
        if (focus_index < 0) focus_index = WATER_BTN_COUNT - 1;
        if (focus_index >= WATER_BTN_COUNT) focus_index = 0;
        update_focus_styles();
    }

    if (is_pressed && data->enc_diff == 0) {
        switch (focus_index) {
        case WATER_BTN_MINUS:
            water_remove();
            break;
        case WATER_BTN_PLUS:
            water_add();
            break;
        case WATER_BTN_GOAL:
            cycle_goal();
            break;
        default:
            break;
        }
    }
}
