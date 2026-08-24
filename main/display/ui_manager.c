#include "ui_manager.h"
#include "screens/screen_home.h"
#include "screens/screen_menu.h"
#include "screens/screen_todos.h"
#include "screens/screen_breathing.h"
#include "screens/screen_idle_background.h"
#include "screens/screen_sleep.h"
#include "screens/screen_timer_presets.h"
#include "screens/screen_timer_edit.h"
#include "screens/screen_timer.h"
#include "screens/screen_tamagotchi.h"
#include "screens/screen_water.h"
#include "screens/screen_settings.h"
#include "screens/screen_sedentary.h"
#include "screens/screen_demo.h"
#include "demo_mode.h"
#include "../utils/sedentary_store.h"
#include "app_state.h"
#include "studybud_theme.h"
#include "color_palette.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "UI_Manager";

static screen_id_t current_screen = SCREEN_HOME;
static lv_obj_t *screens[SCREEN_COUNT] = {0};

static void (*screen_event_handlers[SCREEN_COUNT])(lv_indev_data_t *) = {0};

#define LONG_PRESS_MS 800
#define GLOW_MAX_OPA  100
#define DISPLAY_SIZE  480
#define DISPLAY_CX    240
#define GLOW_OUTER_R  240
#define GLOW_THICKNESS 25
#define GLOW_INNER_R  (GLOW_OUTER_R - GLOW_THICKNESS)

#define READING_MAX_OPA 90
#define READING_LIGHT_COLOR lv_color_hex(0xFFA93D)

static uint32_t press_start_tick = 0;
static bool waiting_for_release = false;
static bool long_press_fired = false;

static lv_obj_t *glow_overlay = NULL;
static lv_timer_t *glow_timer = NULL;

static lv_obj_t *nav_bubble = NULL;

static lv_obj_t *reading_overlay = NULL;
static volatile int pending_reading_light = -1;

/* Re-tint the glow ring and nav bubble with the accent of the current app */
static void update_glow_color(void)
{
    if (!glow_overlay) return;

    lv_color_t c = theme_accent(current_screen);
    lv_img_dsc_t *img = lv_canvas_get_img(glow_overlay);
    if (!img || !img->data) return;

    uint8_t px_size = lv_img_cf_get_px_size(LV_IMG_CF_TRUE_COLOR_ALPHA) >> 3;

    for (lv_coord_t y = 0; y < DISPLAY_SIZE; y++) {
        for (lv_coord_t x = 0; x < DISPLAY_SIZE; x++) {
            uint32_t px = (uint32_t)(y * DISPLAY_SIZE + x) * px_size;
            if (img->data[px + px_size - 1] == 0) continue;
            lv_canvas_set_px_color(glow_overlay, x, y, c);
        }
    }

    if (nav_bubble) lv_obj_set_style_bg_color(nav_bubble, c, 0);
}

static void glow_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!glow_overlay) return;

    if (!waiting_for_release) {
        lv_obj_set_style_opa(glow_overlay, LV_OPA_TRANSP, 0);
        if (nav_bubble) lv_obj_set_style_opa(nav_bubble, LV_OPA_TRANSP, 0);
        if (glow_timer) {
            lv_timer_pause(glow_timer);
        }
        return;
    }

    if (long_press_fired) {
        lv_obj_set_style_opa(glow_overlay, LV_OPA_TRANSP, 0);
        if (nav_bubble) lv_obj_set_style_opa(nav_bubble, LV_OPA_TRANSP, 0);
        if (glow_timer) lv_timer_pause(glow_timer);
        return;
    }

    uint32_t elapsed = lv_tick_elaps(press_start_tick);
    if (elapsed >= LONG_PRESS_MS) elapsed = LONG_PRESS_MS;

    lv_opa_t opa = (lv_opa_t)((uint32_t)GLOW_MAX_OPA * elapsed / LONG_PRESS_MS);
    lv_obj_set_style_opa(glow_overlay, opa, 0);
    if (nav_bubble) lv_obj_set_style_opa(nav_bubble, opa, 0);
}

static void draw_glow_gradient(lv_obj_t *canvas)
{
    lv_color_t c = LV_COLOR_PRIMARY;

    for (lv_coord_t y = 0; y < DISPLAY_SIZE; y++) {
        lv_coord_t dy = y - DISPLAY_CX;
        for (lv_coord_t x = 0; x < DISPLAY_SIZE; x++) {
            lv_coord_t dx = x - DISPLAY_CX;
            float dist = sqrtf((float)(dx * dx + dy * dy));

            if (dist < GLOW_INNER_R || dist > GLOW_OUTER_R) {
                lv_canvas_set_px_opa(canvas, x, y, LV_OPA_TRANSP);
            } else {
                float t = (dist - GLOW_INNER_R) / (float)GLOW_THICKNESS;
                float curved = powf(t, 0.6f);
                lv_opa_t opa = (lv_opa_t)(75 + curved * 204);
                lv_canvas_set_px_color(canvas, x, y, c);
                lv_canvas_set_px_opa(canvas, x, y, opa);
            }
        }
    }
}

static void create_glow_overlay(void)
{
    lv_obj_t *top_layer = lv_layer_top();

    glow_overlay = lv_canvas_create(top_layer);

    lv_color_t *buf = lv_mem_alloc(DISPLAY_SIZE * DISPLAY_SIZE * sizeof(lv_color32_t));
    lv_canvas_set_buffer(glow_overlay, buf, DISPLAY_SIZE, DISPLAY_SIZE, LV_IMG_CF_TRUE_COLOR_ALPHA);

    lv_canvas_fill_bg(glow_overlay, LV_COLOR_PRIMARY, LV_OPA_TRANSP);
    draw_glow_gradient(glow_overlay);

    lv_obj_align(glow_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_opa(glow_overlay, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(glow_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(glow_overlay, LV_OBJ_FLAG_SCROLLABLE);

    glow_timer = lv_timer_create(glow_timer_cb, 16, NULL);
    lv_timer_pause(glow_timer);
}

static void create_nav_bubble(void)
{
    lv_obj_t *top_layer = lv_layer_top();

    nav_bubble = lv_obj_create(top_layer);
    lv_obj_set_size(nav_bubble, 160, 22);
    lv_obj_align(nav_bubble, LV_ALIGN_TOP_MID, 0, 12);

    lv_obj_set_style_bg_color(nav_bubble, LV_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(nav_bubble, LV_OPA_40, 0);
    lv_obj_set_style_radius(nav_bubble, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_shadow_width(nav_bubble, 0, 0);

    lv_obj_set_style_border_width(nav_bubble, 0, 0);
    lv_obj_set_style_pad_all(nav_bubble, 0, 0);

    lv_obj_clear_flag(nav_bubble, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(nav_bubble, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *label = lv_label_create(nav_bubble);
    lv_label_set_text(label, LV_SYMBOL_LEFT "  Menu");
    lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
    lv_obj_center(label);

    lv_obj_set_style_opa(nav_bubble, LV_OPA_TRANSP, 0);
}

void ui_manager_set_reading_light(int strength)
{
    if (strength < 0) strength = 0;
    if (strength > 100) strength = 100;
    pending_reading_light = strength;
}

static void apply_reading_light(int strength)
{
    if (strength == 0) {
        if (reading_overlay) {
            lv_obj_add_flag(reading_overlay, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_opa(reading_overlay, LV_OPA_TRANSP, 0);
        }
        return;
    }

    if (!reading_overlay) {
        reading_overlay = lv_obj_create(lv_layer_top());
        lv_obj_set_size(reading_overlay, DISPLAY_SIZE, DISPLAY_SIZE);
        lv_obj_align(reading_overlay, LV_ALIGN_CENTER, 0, 0);
        lv_obj_set_style_radius(reading_overlay, 0, 0);
        lv_obj_set_style_bg_color(reading_overlay, READING_LIGHT_COLOR, 0);
        lv_obj_set_style_border_width(reading_overlay, 0, 0);
        lv_obj_set_style_shadow_width(reading_overlay, 0, 0);
        lv_obj_set_style_pad_all(reading_overlay, 0, 0);
        lv_obj_clear_flag(reading_overlay, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(reading_overlay, LV_OBJ_FLAG_SCROLLABLE);
    }

    lv_obj_clear_flag(reading_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(reading_overlay);
    lv_opa_t opa = (lv_opa_t)((uint32_t)strength * READING_MAX_OPA / 100);
    lv_obj_set_style_bg_opa(reading_overlay, opa, 0);
}

/* Applied on the LVGL thread so the web-server task never touches LVGL directly */
static void reading_light_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    int v = pending_reading_light;
    if (v < 0) return;
    pending_reading_light = -1;
    apply_reading_light(v);
}

/* ============================================================ */
/* Stretch Break alert popup                                     */
/* ============================================================ */

static lv_obj_t *alert_overlay = NULL;
static lv_obj_t *alert_card = NULL;
static lv_obj_t *alert_btn_snooze = NULL;
static lv_obj_t *alert_btn_break = NULL;
static int alert_focus = 0;

static void alert_update_focus(void);

static void alert_anim_set_border(void *var, int32_t val)
{
    if (var) lv_obj_set_style_border_width((lv_obj_t *)var, val, 0);
}

static void alert_anim_border(lv_obj_t *obj, int32_t from, int32_t to)
{
    if (!obj) return;
    lv_anim_del(obj, (lv_anim_exec_xcb_t)alert_anim_set_border);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)alert_anim_set_border);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, 200);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void create_alert_popup(void)
{
    lv_obj_t *top = lv_layer_top();

    alert_overlay = lv_obj_create(top);
    lv_obj_set_size(alert_overlay, DISPLAY_SIZE, DISPLAY_SIZE);
    lv_obj_align(alert_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(alert_overlay, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(alert_overlay, LV_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(alert_overlay, LV_OPA_60, 0);
    lv_obj_set_style_border_width(alert_overlay, 0, 0);
    lv_obj_set_style_pad_all(alert_overlay, 0, 0);
    lv_obj_clear_flag(alert_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(alert_overlay, LV_OBJ_FLAG_HIDDEN);

    alert_card = lv_obj_create(alert_overlay);
    lv_obj_set_size(alert_card, 360, 260);
    lv_obj_align(alert_card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(alert_card, 24, 0);
    lv_obj_set_style_bg_color(alert_card, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_border_width(alert_card, 0, 0);
    lv_obj_set_style_shadow_width(alert_card, 0, 0);
    lv_obj_set_style_pad_all(alert_card, 20, 0);
    lv_obj_clear_flag(alert_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(alert_card);
    lv_label_set_text(title, "Time to move!");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, LV_COLOR_TEXT, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *sub = lv_label_create(alert_card);
    lv_label_set_text(sub, "You've been sitting for 60 min");
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sub, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 44);

    alert_btn_snooze = lv_btn_create(alert_card);
    lv_obj_set_size(alert_btn_snooze, 150, 54);
    lv_obj_align(alert_btn_snooze, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_style_radius(alert_btn_snooze, 27, 0);
    lv_obj_set_style_bg_color(alert_btn_snooze, LV_COLOR_SURFACE, 0);
    lv_obj_set_style_shadow_width(alert_btn_snooze, 0, 0);
    lv_obj_set_style_shadow_opa(alert_btn_snooze, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(alert_btn_snooze, 0, 0);
    lv_obj_set_style_border_color(alert_btn_snooze, theme_accent(SCREEN_SEDENTARY), 0);
    lv_obj_set_style_pad_all(alert_btn_snooze, 0, 0);

    lv_obj_t *snooze_lbl = lv_label_create(alert_btn_snooze);
    lv_label_set_text(snooze_lbl, "Snooze (10m)");
    lv_obj_set_style_text_font(snooze_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(snooze_lbl, LV_COLOR_TEXT, 0);
    lv_obj_center(snooze_lbl);

    alert_btn_break = lv_btn_create(alert_card);
    lv_obj_set_size(alert_btn_break, 150, 54);
    lv_obj_align(alert_btn_break, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_set_style_radius(alert_btn_break, 27, 0);
    lv_obj_set_style_bg_color(alert_btn_break, theme_accent(SCREEN_SEDENTARY), 0);
    lv_obj_set_style_shadow_width(alert_btn_break, 0, 0);
    lv_obj_set_style_shadow_opa(alert_btn_break, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(alert_btn_break, 0, 0);
    lv_obj_set_style_border_color(alert_btn_break, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_pad_all(alert_btn_break, 0, 0);

    lv_obj_t *break_lbl = lv_label_create(alert_btn_break);
    lv_label_set_text(break_lbl, "Start Break");
    lv_obj_set_style_text_font(break_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(break_lbl, LV_COLOR_BG_CARD, 0);
    lv_obj_center(break_lbl);

    alert_focus = 0;
    alert_update_focus();
}

static void alert_update_focus(void)
{
    if (!alert_overlay) return;
    for (int i = 0; i < 2; i++) {
        lv_obj_t *btn = (i == 0) ? alert_btn_snooze : alert_btn_break;
        if (!btn) continue;
        bool focused = (i == alert_focus);
        lv_color_t accent = theme_accent(SCREEN_SEDENTARY);
        lv_color_t bg = focused ? accent : LV_COLOR_SURFACE;
        lv_obj_set_style_bg_color(btn, bg, 0);
        alert_anim_border(btn,
            lv_obj_get_style_border_width(btn, 0),
            focused ? 3 : 0);
    }
}

static void alert_show(void)
{
    if (!alert_overlay) create_alert_popup();
    sedentary_state_t *sed = sedentary_store_get_state();
    alert_focus = 0;
    alert_update_focus();
    lv_obj_clear_flag(alert_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(alert_overlay);
    ESP_LOGI(TAG, "Sedentary alert shown (interval %d min)", sed->interval_min);
}

static void alert_hide(void)
{
    if (alert_overlay) lv_obj_add_flag(alert_overlay, LV_OBJ_FLAG_HIDDEN);
}

static bool alert_is_showing(void)
{
    return alert_overlay && !lv_obj_has_flag(alert_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void sedentary_tick_cb(lv_timer_t *timer)
{
    (void)timer;
    sedentary_store_tick();

    if (sedentary_store_consume_alert()) {
        alert_show();
    }

    if (current_screen == SCREEN_SEDENTARY) {
        screen_sedentary_refresh();
    }
}

static void timer_background_cb(lv_timer_t *timer)
{
    (void)timer;
    if (current_screen == SCREEN_TIMER) return;
    screen_timer_background_tick();
}

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

    /* Create glow overlay (on top layer, above all screens) */
    create_glow_overlay();

    /* Create navigation bubble (on top layer, above all screens) */
    create_nav_bubble();

    /* Reading light: applied on the LVGL thread via a poll timer */
    lv_timer_create(reading_light_timer_cb, 100, NULL);
    apply_reading_light(app_state_get()->settings.reading_light);

    /* Create all screens (menu first to populate accent colors) */
    screens[SCREEN_MENU] = screen_menu_create();
    screens[SCREEN_HOME] = screen_home_create();
    screens[SCREEN_TODOS] = screen_todos_create();
    screens[SCREEN_BREATHING] = screen_breathing_create();
    screens[SCREEN_BACKGROUNDS] = screen_idle_background_create();
    screens[SCREEN_SLEEP] = screen_sleep_create();
    screens[SCREEN_TIMER_PRESETS] = screen_timer_presets_create();
    screens[SCREEN_TIMER_EDIT] = screen_timer_edit_create();
    screens[SCREEN_TIMER] = screen_timer_create();
    screens[SCREEN_TAMAGOTCHI] = screen_tamagotchi_create();
    screens[SCREEN_WATER] = screen_water_create();
    screens[SCREEN_SEDENTARY] = screen_sedentary_create();
    screens[SCREEN_SETTINGS] = screen_settings_create();
    screens[SCREEN_DEMO] = screen_demo_create();

    /* Register event handlers */
    screen_event_handlers[SCREEN_HOME] = screen_home_encoder_event;
    screen_event_handlers[SCREEN_MENU] = screen_menu_encoder_event;
    screen_event_handlers[SCREEN_TODOS] = screen_todos_encoder_event;
    screen_event_handlers[SCREEN_BREATHING] = screen_breathing_encoder_event;
    screen_event_handlers[SCREEN_BACKGROUNDS] = screen_idle_background_encoder_event;
    screen_event_handlers[SCREEN_SLEEP] = screen_sleep_encoder_event;
    screen_event_handlers[SCREEN_TIMER_PRESETS] = screen_timer_presets_encoder_event;
    screen_event_handlers[SCREEN_TIMER_EDIT] = screen_timer_edit_encoder_event;
    screen_event_handlers[SCREEN_TIMER] = screen_timer_encoder_event;
    screen_event_handlers[SCREEN_TAMAGOTCHI] = screen_tamagotchi_encoder_event;
    screen_event_handlers[SCREEN_WATER] = screen_water_encoder_event;
    screen_event_handlers[SCREEN_SEDENTARY] = screen_sedentary_encoder_event;
    screen_event_handlers[SCREEN_SETTINGS] = screen_settings_encoder_event;
    screen_event_handlers[SCREEN_DEMO] = screen_demo_encoder_event;

    /* Load home screen as default */
    lv_scr_load(screens[SCREEN_HOME]);
    current_screen = SCREEN_HOME;
    app_state_broadcast_screen_change(SCREEN_HOME);

    /* Persistent background timer so a running timer keeps counting (and
       broadcasting) even when the user navigates away from the timer screen */
    lv_timer_create(timer_background_cb, 1000, NULL);

    /* Persistent background timer for the stretch-break countdown + alerts */
    lv_timer_create(sedentary_tick_cb, 1000, NULL);

    ESP_LOGI(TAG, "UI Manager initialized, showing Home screen");
}

void ui_manager_switch_screen(screen_id_t screen)
{
    if (screen >= SCREEN_COUNT) {
        ESP_LOGW(TAG, "Invalid screen ID: %d", screen);
        return;
    }

    switch (screen) {
    case SCREEN_TIMER:
        screens[SCREEN_TIMER] = screen_timer_create();
        break;
    case SCREEN_TIMER_PRESETS:
        screens[SCREEN_TIMER_PRESETS] = screen_timer_presets_create();
        break;
    case SCREEN_TIMER_EDIT:
        screens[SCREEN_TIMER_EDIT] = screen_timer_edit_create();
        break;
    case SCREEN_TODOS:
        screen_todos_refresh();
        break;
    case SCREEN_HOME:
        screen_home_refresh();
        break;
    case SCREEN_WATER:
        screen_water_refresh();
        break;
    case SCREEN_SETTINGS:
        screen_settings_refresh();
        break;
    case SCREEN_TAMAGOTCHI:
        screen_tamagotchi_refresh();
        break;
    case SCREEN_SEDENTARY:
        screen_sedentary_refresh();
        break;
    case SCREEN_DEMO:
        screen_demo_refresh();
        break;
    default:
        break;
    }

    if (!screens[screen]) {
        ESP_LOGW(TAG, "Screen %d not available", screen);
        return;
    }

    ESP_LOGI(TAG, "Switching to screen %d", screen);
    lv_scr_load_anim(screens[screen], LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
    current_screen = screen;
    app_state_broadcast_screen_change(screen);
}

void ui_manager_encoder_event(lv_indev_data_t *data)
{
    /* Alert popup (Stretch Break) fully intercepts the encoder while shown */
    if (alert_is_showing()) {
        if (data->enc_diff != 0) {
            alert_focus += data->enc_diff;
            if (alert_focus < 0) alert_focus = 1;
            if (alert_focus > 1) alert_focus = 0;
            alert_update_focus();
            app_state_broadcast_encoder_event(data->enc_diff > 0 ? "cw" : "ccw", "none");
        }
        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            if (alert_focus == 0) {
                app_state_broadcast_encoder_event("none", "press");
                sedentary_store_snooze();
                alert_hide();
            } else {
                app_state_broadcast_encoder_event("none", "press");
                alert_hide();
                sedentary_store_enter_break();
                ui_manager_switch_screen(SCREEN_SEDENTARY);
            }
        }
        return;
    }

    /* Demo mode: intercept input during INSTRUCTION checkpoints,
       pass through during WAIT_ACTION checkpoints so user can interact */
    if (demo_mode_is_active()) {
        /* Task mode — let input pass through to underlying screen.
           After the user completes the task, demo_mode_check_task_complete()
           will detect it and show the overlay.  Long press always exits. */
        if (demo_mode_is_task_active()) {
            if (data->enc_diff != 0 && screen_event_handlers[current_screen]) {
                app_state_broadcast_encoder_event(data->enc_diff > 0 ? "cw" : "ccw", "none");
                lv_indev_data_t fwd = {0};
                fwd.enc_diff = data->enc_diff;
                fwd.state = LV_INDEV_STATE_REL;
                screen_event_handlers[current_screen](&fwd);
            }
            if (data->state == LV_INDEV_STATE_PR && !waiting_for_release) {
                press_start_tick = lv_tick_get();
                waiting_for_release = true;
                long_press_fired = false;
                return;
            }
            if (waiting_for_release && data->state == LV_INDEV_STATE_PR) {
                if (!long_press_fired && lv_tick_elaps(press_start_tick) >= LONG_PRESS_MS) {
                    long_press_fired = true;
                    demo_mode_stop(true);
                }
                return;
            }
            if (data->state == LV_INDEV_STATE_REL && waiting_for_release) {
                bool was_long = long_press_fired;
                waiting_for_release = false;
                press_start_tick = 0;
                long_press_fired = false;
                if (was_long) return;
                /* Short press — forward to screen handler */
                if (current_screen == SCREEN_MENU) {
                    screen_id_t sel = screen_menu_get_selection();
                    if (sel != SCREEN_COUNT) { ui_manager_switch_screen(sel); return; }
                }
                if (screen_event_handlers[current_screen]) {
                    lv_indev_data_t pr = {0};
                    pr.state = LV_INDEV_STATE_PR;
                    pr.enc_diff = 0;
                    screen_event_handlers[current_screen](&pr);
                }
                demo_mode_check_task_complete();
            }
            return;
        }

        /* Instruction mode — demo owns the encoder */
        if (data->enc_diff != 0) return;

        if (data->state == LV_INDEV_STATE_PR && !waiting_for_release) {
            press_start_tick = lv_tick_get();
            waiting_for_release = true;
            long_press_fired = false;
            return;
        }
        if (waiting_for_release && data->state == LV_INDEV_STATE_PR) {
            if (!long_press_fired && lv_tick_elaps(press_start_tick) >= LONG_PRESS_MS) {
                long_press_fired = true;
                demo_mode_stop(true);
            }
            return;
        }
        if (data->state == LV_INDEV_STATE_REL && waiting_for_release) {
            bool was_long = long_press_fired;
            waiting_for_release = false;
            press_start_tick = 0;
            long_press_fired = false;
            if (!was_long) {
                demo_mode_advance();
            }
            return;
        }
        return;
    }

    /* Forward rotation immediately, even while button is held */
    if (data->enc_diff != 0 && screen_event_handlers[current_screen]) {
        app_state_broadcast_encoder_event(data->enc_diff > 0 ? "cw" : "ccw", "none");
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
        app_state_broadcast_encoder_event("none", "press");

        if (current_screen != SCREEN_MENU) {
            update_glow_color();
            if (glow_timer) lv_timer_resume(glow_timer);
        }
        return;
    }

    /* Button still held: check if long press threshold crossed */
    if (waiting_for_release && data->state == LV_INDEV_STATE_PR) {
        if (!long_press_fired && lv_tick_elaps(press_start_tick) >= LONG_PRESS_MS) {
            long_press_fired = true;
            app_state_broadcast_encoder_event("none", "long_press");
            if (current_screen != SCREEN_MENU) {
                ESP_LOGI(TAG, "Long press -> menu");
                ui_manager_switch_screen(SCREEN_MENU);
            }
        }
        return;
    }

    /* Button released: fire short press action if it wasn't a long press */
    if (data->state == LV_INDEV_STATE_REL && waiting_for_release) {
        bool was_long = long_press_fired;
        uint32_t held_ms = lv_tick_elaps(press_start_tick);
        waiting_for_release = false;
        press_start_tick = 0;
        long_press_fired = false;

        if (glow_timer) lv_timer_pause(glow_timer);
        lv_obj_set_style_opa(glow_overlay, LV_OPA_TRANSP, 0);
        if (nav_bubble) lv_obj_set_style_opa(nav_bubble, LV_OPA_TRANSP, 0);

        if (was_long) return;

        app_state_broadcast_encoder_event("none", "short_press");

        /* If held for the full long-press duration, treat as cancelled hold */
        if (held_ms >= LONG_PRESS_MS) return;

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
