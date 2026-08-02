#include "screen_timer.h"
#include "ui_manager.h"
#include "color_palette.h"
#include "../app_state.h"
#include "../utils/timer_store.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "Screen_Timer";

#define ARC_SIZE      300
#define ARC_OFFSET_Y  -25
#define FOCUS_ANIM_MS 200
#define BTN_SIZE      60
#define BTN_GAP       100
#define BTN_ROW_Y     -30

#define ARC_START_ANGLE 135
#define ARC_END_ANGLE   405
#define ARC_RANGE_ANGLE (ARC_END_ANGLE - ARC_START_ANGLE)

typedef enum {
    TIMER_BTN_PAUSE,
    TIMER_BTN_RESTART,
    TIMER_BTN_BACK,
    TIMER_BTN_COUNT
} timer_btn_t;

static lv_obj_t *screen = NULL;

static lv_obj_t *lbl_title = NULL;
static lv_obj_t *arc = NULL;
static lv_obj_t *lbl_countdown = NULL;
static lv_obj_t *lbl_phase = NULL;
static lv_obj_t *btn_objects[TIMER_BTN_COUNT];
static lv_obj_t *btn_labels[TIMER_BTN_COUNT];

static lv_obj_t *btn_nevermind = NULL;
static lv_obj_t *lbl_nevermind = NULL;

static lv_timer_t *tick_timer = NULL;

static int focus_idx = 0;
static bool confirm_mode = false;

static void update_arc(void);
static void update_countdown_label(void);
static void update_controls(void);
static void tick_callback(lv_timer_t *timer);
static void format_time(char *buf, size_t len, uint32_t sec);
static void arm_end_tick(void);
static void sync_remaining(void);

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

static void update_arc(void)
{
    if (!arc) return;
    timer_state_t *ts = app_state_get_timer();
    if (ts->total_seconds == 0) {
        lv_arc_set_angles(arc, ARC_START_ANGLE, ARC_START_ANGLE);
        return;
    }

    uint32_t elapsed = ts->total_seconds - ts->remaining_seconds;
    uint16_t end_angle = ARC_START_ANGLE + (uint16_t)(((uint64_t)ARC_RANGE_ANGLE * 1000 * elapsed) / ts->total_seconds / 1000);
    
    if (end_angle > ARC_END_ANGLE) end_angle = ARC_END_ANGLE;
    lv_arc_set_angles(arc, ARC_START_ANGLE, end_angle);

    lv_color_t arc_color = (ts->phase == TIMER_PHASE_SESSION) ?
                           LV_COLOR_TIMER : LV_COLOR_TIMER_BREAK;
    lv_obj_set_style_arc_color(arc, arc_color, LV_PART_INDICATOR);
}

static void update_countdown_label(void)
{
    if (!lbl_countdown) return;
    timer_state_t *ts = app_state_get_timer();

    if (ts->phase_complete_awaiting_press) {
        const char *next_name = (ts->phase == TIMER_PHASE_SESSION) ? "break" : "session";
        char buf[48];
        snprintf(buf, sizeof(buf), "Press to start\n%s", next_name);
        lv_label_set_text(lbl_countdown, buf);
        lv_obj_set_style_text_font(lbl_countdown, &lv_font_montserrat_16, 0);
    } else {
        char time_buf[16];
        format_time(time_buf, sizeof(time_buf), ts->remaining_seconds);
        lv_label_set_text(lbl_countdown, time_buf);
        lv_obj_set_style_text_font(lbl_countdown, &lv_font_montserrat_48, 0);
    }
    lv_obj_align(lbl_countdown, LV_ALIGN_CENTER, 0, ARC_OFFSET_Y);
}

static void update_controls(void)
{
    timer_state_t *ts = app_state_get_timer();

    const char *pause_text = ts->is_running ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
    if (btn_labels[TIMER_BTN_PAUSE]) {
        lv_label_set_text(btn_labels[TIMER_BTN_PAUSE], pause_text);
    }

    if (arc) {
        lv_color_t bg_arc_color = (ts->phase == TIMER_PHASE_SESSION) ? 
                                   LV_COLOR_TIMER : LV_COLOR_TIMER_BREAK;
        lv_obj_set_style_arc_color(arc, bg_arc_color, LV_PART_MAIN);
    }

    if (lbl_phase) {
        timer_preset_t *p = timer_store_get_by_id(ts->preset_id);
        bool is_pomo = (p && p->type == TIMER_TYPE_POMODORO);
        if (is_pomo) {
            lv_obj_clear_flag(lbl_phase, LV_OBJ_FLAG_HIDDEN);
            const char *names[] = { "Session", "Short Break", "Long Break" };
            if (ts->phase <= TIMER_PHASE_LONG_BREAK) {
                lv_label_set_text(lbl_phase, names[ts->phase]);
            }
        } else {
            lv_obj_add_flag(lbl_phase, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (confirm_mode) {
        for (int i = 0; i < TIMER_BTN_COUNT; i++) {
            if (btn_objects[i]) lv_obj_add_flag(btn_objects[i], LV_OBJ_FLAG_HIDDEN);
        }

        if (btn_nevermind) {
            lv_obj_clear_flag(btn_nevermind, LV_OBJ_FLAG_HIDDEN);
            bool nm_focused = (focus_idx == 1);
            anim_start_border(btn_nevermind,
                lv_obj_get_style_border_width(btn_nevermind, 0),
                nm_focused ? 3 : 0, FOCUS_ANIM_MS);
            lv_color_t nm_bg = nm_focused ? theme_accent(SCREEN_TIMER) : theme_accent_dark(SCREEN_TIMER);
            lv_obj_set_style_bg_color(btn_nevermind, nm_bg, 0);
            lv_obj_set_style_text_color(lbl_nevermind, contrast_text_color(nm_bg), 0);
        }

        if (btn_objects[TIMER_BTN_BACK]) {
            lv_obj_clear_flag(btn_objects[TIMER_BTN_BACK], LV_OBJ_FLAG_HIDDEN);
            bool back_focused = (focus_idx == 0);
            lv_label_set_text(btn_labels[TIMER_BTN_BACK], LV_SYMBOL_CLOSE);
            lv_obj_set_style_bg_color(btn_objects[TIMER_BTN_BACK], LV_COLOR_ERROR, 0);
            anim_start_border(btn_objects[TIMER_BTN_BACK],
                lv_obj_get_style_border_width(btn_objects[TIMER_BTN_BACK], 0),
                back_focused ? 3 : 0, FOCUS_ANIM_MS);
        }
    } else {
        for (int i = 0; i < TIMER_BTN_COUNT; i++) {
            if (btn_objects[i]) lv_obj_clear_flag(btn_objects[i], LV_OBJ_FLAG_HIDDEN);
        }

        if (btn_labels[TIMER_BTN_BACK]) {
            lv_label_set_text(btn_labels[TIMER_BTN_BACK], LV_SYMBOL_LEFT);
        }

        for (int i = 0; i < TIMER_BTN_COUNT; i++) {
            if (!btn_objects[i]) continue;
            bool focused = (i == focus_idx);
            anim_start_border(btn_objects[i],
                lv_obj_get_style_border_width(btn_objects[i], 0),
                focused ? 3 : 0, FOCUS_ANIM_MS);
            lv_color_t bg = focused ? theme_accent(SCREEN_TIMER) : theme_accent_dark(SCREEN_TIMER);
            lv_obj_set_style_bg_color(btn_objects[i], bg, 0);
            if (btn_labels[i]) lv_obj_set_style_text_color(btn_labels[i], contrast_text_color(bg), 0);
        }

        if (btn_nevermind) {
            lv_obj_add_flag(btn_nevermind, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void enter_confirm(void)
{
    confirm_mode = true;
    focus_idx = 1; // Default safely to "Nevermind" instead of Exit
    update_controls();
    ESP_LOGI(TAG, "Entered confirm mode");
}

static void exit_confirm(void)
{
    confirm_mode = false;
    focus_idx = TIMER_BTN_BACK;
    update_controls();
    ESP_LOGI(TAG, "Exited confirm mode");
}

static void advance_pomodoro_phase(void)
{
    timer_state_t *ts = app_state_get_timer();
    timer_preset_t *p = timer_store_get_by_id(ts->preset_id);
    if (!p || p->type != TIMER_TYPE_POMODORO) return;

    if (ts->phase == TIMER_PHASE_SESSION) {
        ts->pomodoro_session_count++;
        if (ts->pomodoro_session_count >= 4) {
            ts->phase = TIMER_PHASE_LONG_BREAK;
            ts->total_seconds = p->long_break_sec;
            ts->remaining_seconds = p->long_break_sec;
            ts->pomodoro_session_count = 0;
        } else {
            ts->phase = TIMER_PHASE_SHORT_BREAK;
            ts->total_seconds = p->short_break_sec;
            ts->remaining_seconds = p->short_break_sec;
        }
    } else {
        ts->phase = TIMER_PHASE_SESSION;
        ts->total_seconds = p->session_sec;
        ts->remaining_seconds = p->session_sec;
    }

    ts->is_running = true;
    ts->phase_complete_awaiting_press = false;
    arm_end_tick();
    if (tick_timer) lv_timer_resume(tick_timer);

    app_state_broadcast_timer_sync();
    update_arc();
    update_countdown_label();
    update_controls();
    ESP_LOGI(TAG, "Advanced to phase %d", ts->phase);
}

static void arm_end_tick(void)
{
    timer_state_t *ts = app_state_get_timer();
    ts->end_tick = (int64_t)lv_tick_get() + (int64_t)ts->remaining_seconds * 1000;
}

static void sync_remaining(void)
{
    timer_state_t *ts = app_state_get_timer();
    if (ts->end_tick <= 0) return;
    int64_t now = (int64_t)lv_tick_get();
    int64_t ms_left = ts->end_tick - now;
    if (ms_left <= 0) {
        ts->remaining_seconds = 0;
    } else {
        ts->remaining_seconds = (uint32_t)((ms_left + 999) / 1000);
    }
}

static void tick_callback(lv_timer_t *timer)
{
    (void)timer;
    timer_state_t *ts = app_state_get_timer();
    if (!ts->is_running) return;
    if (ts->phase_complete_awaiting_press) return;

    sync_remaining();

    update_arc();
    update_countdown_label();
    app_state_broadcast_timer_sync();

    if (ts->remaining_seconds == 0) {
        ts->is_running = false;
        ts->end_tick = 0;
        ts->phase_complete_awaiting_press = true;
        if (tick_timer) lv_timer_pause(tick_timer);
        app_state_broadcast_timer_session_complete(ts->phase, ts->preset_id);
        app_state_broadcast_timer_sync();
        update_countdown_label();
        update_controls();
        ESP_LOGI(TAG, "Phase complete, awaiting press");
    }
}

void screen_timer_destroy(void)
{
    if (tick_timer) {
        lv_timer_del(tick_timer);
        tick_timer = NULL;
    }
    if (screen) {
        lv_obj_del(screen);
        screen = NULL;
    }
    focus_idx = 0;
    confirm_mode = false;
}

void screen_timer_background_tick(void)
{
    timer_state_t *ts = app_state_get_timer();
    if (!ts->is_running) return;
    if (ts->phase_complete_awaiting_press) return;

    sync_remaining();
    if (ts->remaining_seconds == 0) {
        ts->is_running = false;
        ts->end_tick = 0;
        ts->phase_complete_awaiting_press = true;
        app_state_broadcast_timer_session_complete(ts->phase, ts->preset_id);
        app_state_broadcast_timer_sync();
        ESP_LOGI(TAG, "Phase complete in background, awaiting press");
    } else {
        app_state_broadcast_timer_sync();
    }
}

void screen_timer_encoder_event(lv_indev_data_t *data)
{
    timer_state_t *ts = app_state_get_timer();

    if (ts->phase_complete_awaiting_press) {
        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            advance_pomodoro_phase();
        }
        return;
    }

    if (confirm_mode) {
        if (data->enc_diff != 0) {
            int new_f = focus_idx + data->enc_diff;
            while (new_f < 0) new_f += 2;
            focus_idx = new_f % 2;
            update_controls();
        }
        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            if (focus_idx == 0) {
                ui_manager_switch_screen(SCREEN_TIMER_PRESETS);
                ESP_LOGI(TAG, "Confirmed exit to presets");
            } else {
                exit_confirm();
            }
        }
        return;
    }

    if (data->enc_diff != 0) {
        int new_f = focus_idx + data->enc_diff;
        if (new_f < 0) new_f = TIMER_BTN_COUNT - 1;
        if (new_f >= TIMER_BTN_COUNT) new_f = 0;
        focus_idx = new_f;
        update_controls();
    }

    if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
        switch (focus_idx) {
        case TIMER_BTN_PAUSE:
            if (ts->is_running) {
                sync_remaining();
                ts->end_tick = 0;
                if (tick_timer) lv_timer_pause(tick_timer);
            } else {
                arm_end_tick();
                if (tick_timer) lv_timer_resume(tick_timer);
            }
            ts->is_running = !ts->is_running;
            update_controls();
            app_state_broadcast_timer_sync();
            ESP_LOGI(TAG, "Timer %s", ts->is_running ? "resumed" : "paused");
            break;

        case TIMER_BTN_RESTART: {
            timer_preset_t *p = timer_store_get_by_id(ts->preset_id);
            if (p) {
                if (p->type == TIMER_TYPE_POMODORO) {
                    if (ts->phase == TIMER_PHASE_SESSION) ts->total_seconds = p->session_sec;
                    else if (ts->phase == TIMER_PHASE_SHORT_BREAK) ts->total_seconds = p->short_break_sec;
                    else ts->total_seconds = p->long_break_sec;
                } else {
                    ts->total_seconds = p->duration_sec;
                }
                ts->remaining_seconds = ts->total_seconds;
                if (ts->phase_complete_awaiting_press) {
                    ts->phase_complete_awaiting_press = false;
                    ts->is_running = true;
                    arm_end_tick();
                    if (tick_timer) lv_timer_resume(tick_timer);
                } else if (ts->is_running) {
                    arm_end_tick();
                }
            }
            update_arc();
            update_countdown_label();
            app_state_broadcast_timer_sync();
            ESP_LOGI(TAG, "Timer restarted");
            break;
        }

        case TIMER_BTN_BACK:
            enter_confirm();
            break;

        default:
            break;
        }
    }
}

lv_obj_t *screen_timer_create(void)
{
    screen_timer_destroy();

    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, pastel_color(theme_accent(SCREEN_TIMER)), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* --- Title --- */
    lbl_title = lv_label_create(screen);
    lv_label_set_text(lbl_title, "Timer");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 12);

    /* --- Phase label --- */
    lbl_phase = lv_label_create(screen);
    lv_label_set_text(lbl_phase, "");
    lv_obj_set_style_text_font(lbl_phase, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_phase, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lbl_phase, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_add_flag(lbl_phase, LV_OBJ_FLAG_HIDDEN);

    /* --- Progress arc --- */
    arc = lv_arc_create(screen);
    lv_obj_set_size(arc, ARC_SIZE, ARC_SIZE);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, ARC_OFFSET_Y);
    lv_arc_set_bg_angles(arc, ARC_START_ANGLE, ARC_END_ANGLE);
    lv_arc_set_angles(arc, ARC_START_ANGLE, ARC_START_ANGLE);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_obj_set_style_border_width(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, theme_accent_dark(SCREEN_TIMER), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, LV_COLOR_TIMER, LV_PART_INDICATOR);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    /* --- Countdown label --- */
    lbl_countdown = lv_label_create(screen);
    lv_label_set_text(lbl_countdown, "00:00");
    lv_obj_set_style_text_font(lbl_countdown, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_countdown, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_countdown, LV_ALIGN_CENTER, 0, ARC_OFFSET_Y);

    /* --- Control buttons --- */
    const char *icons[] = { LV_SYMBOL_PLAY, LV_SYMBOL_REFRESH, LV_SYMBOL_LEFT };
    for (int i = 0; i < TIMER_BTN_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(screen);
        lv_obj_set_size(btn, BTN_SIZE, BTN_SIZE);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, (i - 1) * BTN_GAP, BTN_ROW_Y);
        lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(btn, theme_accent_dark(SCREEN_TIMER), 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_border_color(btn, theme_accent_light(SCREEN_TIMER), 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        btn_objects[i] = btn;

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, icons[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl, contrast_text_color(theme_accent_dark(SCREEN_TIMER)), 0);
        lv_obj_center(lbl);
        btn_labels[i] = lbl;
    }

    /* --- Nevermind button --- */
    btn_nevermind = lv_btn_create(screen);
    lv_obj_set_size(btn_nevermind, 140, 40);
    lv_obj_align(btn_nevermind, LV_ALIGN_BOTTOM_MID, 0, BTN_ROW_Y - 55);
    lv_obj_set_style_radius(btn_nevermind, 20, 0);
    lv_obj_set_style_bg_color(btn_nevermind, theme_accent_dark(SCREEN_TIMER), 0);
    lv_obj_set_style_shadow_width(btn_nevermind, 0, 0);
    lv_obj_set_style_border_width(btn_nevermind, 0, 0);
    lv_obj_set_style_border_color(btn_nevermind, theme_accent_light(SCREEN_TIMER), 0);
    lv_obj_set_style_pad_all(btn_nevermind, 0, 0);
    lv_obj_add_flag(btn_nevermind, LV_OBJ_FLAG_HIDDEN);

    lbl_nevermind = lv_label_create(btn_nevermind);
    lv_label_set_text(lbl_nevermind, "Nevermind");
    lv_obj_set_style_text_font(lbl_nevermind, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_nevermind, contrast_text_color(theme_accent_dark(SCREEN_TIMER)), 0);
    lv_obj_center(lbl_nevermind);

    /* --- 1-second tick timer --- */
    tick_timer = lv_timer_create(tick_callback, 1000, NULL);

    /* --- Sync state --- */
    timer_state_t *ts = app_state_get_timer();
    if (ts->total_seconds > 0) {
        char title_buf[32];
        timer_preset_t *p = timer_store_get_by_id(ts->preset_id);
        if (p) {
            snprintf(title_buf, sizeof(title_buf), "%s", p->name);
            lv_label_set_text(lbl_title, title_buf);
        }
    }

    if (ts->is_running && !ts->phase_complete_awaiting_press) {
        if (ts->end_tick <= 0) arm_end_tick();
        lv_timer_resume(tick_timer);
    } else {
        lv_timer_pause(tick_timer);
    }

    update_arc();
    update_countdown_label();
    focus_idx = TIMER_BTN_PAUSE;
    confirm_mode = false;
    update_controls();
    app_state_broadcast_timer_sync();

    ESP_LOGI(TAG, "Timer screen created");
    return screen;
}