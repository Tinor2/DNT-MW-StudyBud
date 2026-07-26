#include "screen_timer.h"
#include "ui_manager.h"
#include "studybud_theme.h"
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

typedef enum {
    TIMER_BTN_PAUSE,
    TIMER_BTN_RESTART,
    TIMER_BTN_BACK,
    TIMER_BTN_COUNT
} timer_btn_t;

static lv_obj_t *screen;

static lv_obj_t *lbl_title;
static lv_obj_t *arc;
static lv_obj_t *lbl_countdown;
static lv_obj_t *lbl_phase;
static lv_obj_t *btn_objects[TIMER_BTN_COUNT];
static lv_obj_t *btn_labels[TIMER_BTN_COUNT];

static lv_obj_t *btn_nevermind;
static lv_obj_t *lbl_nevermind;

static lv_timer_t *tick_timer = NULL;

static int focus_idx = 0;
static int prev_focus = -1;
static bool confirm_mode = false;

static void update_arc(void);
static void update_countdown_label(void);
static void update_controls(void);
static void tick_callback(lv_timer_t *timer);
static void format_time(char *buf, size_t len, uint32_t sec);

static void anim_set_opa(void *var, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)val, 0);
}

static void anim_set_border_width(void *var, int32_t val)
{
    lv_obj_set_style_border_width((lv_obj_t *)var, val, 0);
}

static void anim_start_opa(lv_obj_t *obj, int32_t from, int32_t to, uint32_t time)
{
    lv_anim_del(obj, (lv_anim_exec_xcb_t)anim_set_opa);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_set_opa);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_time(&a, time);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_start(&a);
}

static void anim_start_border(lv_obj_t *obj, int32_t from, int32_t to, uint32_t time)
{
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
        lv_arc_set_end_angle(arc, 0);
        return;
    }
    uint32_t elapsed = ts->total_seconds - ts->remaining_seconds;
    uint16_t angle = (uint16_t)((uint32_t)360 * elapsed / ts->total_seconds);
    lv_arc_set_end_angle(arc, angle);

    lv_color_t arc_color = (ts->phase == TIMER_PHASE_SESSION) ?
                           LV_COLOR_TIMER : LV_COLOR_TIMER_BREAK;
    lv_obj_set_style_arc_color(arc, arc_color, LV_PART_INDICATOR);
}

static void update_countdown_label(void)
{
    if (!lbl_countdown) return;
    timer_state_t *ts = app_state_get_timer();

    if (ts->phase_complete_awaiting_press) {
        const char *next_name = "next phase";
        if (ts->phase == TIMER_PHASE_SESSION) next_name = "break";
        else next_name = "session";
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
}

static void update_controls(void)
{
    timer_state_t *ts = app_state_get_timer();

    const char *pause_text = ts->is_running ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY;
    lv_label_set_text(btn_labels[TIMER_BTN_PAUSE], pause_text);

    if (ts->phase == TIMER_PHASE_SESSION) {
        lv_obj_set_style_bg_color(arc, LV_COLOR_TIMER, LV_PART_MAIN);
    } else {
        lv_obj_set_style_bg_color(arc, LV_COLOR_TIMER_BREAK, LV_PART_MAIN);
    }

    if (lbl_phase) {
        bool is_pomo = (ts->phase != TIMER_PHASE_SESSION);
        if (is_pomo) {
            lv_obj_clear_flag(lbl_phase, LV_OBJ_FLAG_HIDDEN);
            const char *names[] = { "Session", "Short Break", "Long Break" };
            lv_label_set_text(lbl_phase, names[ts->phase]);
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
            lv_obj_set_style_bg_color(btn_nevermind,
                nm_focused ? LV_COLOR_PRIMARY : LV_COLOR_PRIMARY_DARK, 0);
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

        lv_label_set_text(btn_labels[TIMER_BTN_BACK], LV_SYMBOL_LEFT);
        lv_obj_set_style_bg_color(btn_objects[TIMER_BTN_BACK], LV_COLOR_PRIMARY_DARK, 0);

        for (int i = 0; i < TIMER_BTN_COUNT; i++) {
            if (!btn_objects[i]) continue;
            bool focused = (i == focus_idx);
            anim_start_border(btn_objects[i],
                lv_obj_get_style_border_width(btn_objects[i], 0),
                focused ? 3 : 0, FOCUS_ANIM_MS);
            lv_obj_set_style_bg_color(btn_objects[i],
                focused ? LV_COLOR_PRIMARY : LV_COLOR_PRIMARY_DARK, 0);
        }

        if (btn_nevermind) {
            lv_obj_add_flag(btn_nevermind, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void enter_confirm(void)
{
    confirm_mode = true;
    focus_idx = 0;
    prev_focus = -1;
    update_controls();
    ESP_LOGI(TAG, "Entered confirm mode");
}

static void exit_confirm(void)
{
    confirm_mode = false;
    focus_idx = TIMER_BTN_BACK;
    prev_focus = -1;
    update_controls();
    ESP_LOGI(TAG, "Exited confirm mode");
}

static void advance_pomodoro_phase(void)
{
    timer_state_t *ts = app_state_get_timer();
    timer_preset_t *p = timer_store_get_by_id(ts->preset_id);
    if (!p || p->type != TIMER_TYPE_POMODORO) return;

    if (ts->phase == TIMER_PHASE_SESSION) {
        ts->phase = TIMER_PHASE_SHORT_BREAK;
        ts->total_seconds = p->short_break_sec;
        ts->remaining_seconds = p->short_break_sec;
    } else if (ts->phase == TIMER_PHASE_SHORT_BREAK) {
        ts->phase = TIMER_PHASE_LONG_BREAK;
        ts->total_seconds = p->long_break_sec;
        ts->remaining_seconds = p->long_break_sec;
    } else {
        ts->phase = TIMER_PHASE_SESSION;
        ts->total_seconds = p->session_sec;
        ts->remaining_seconds = p->session_sec;
    }

    ts->is_running = true;
    ts->phase_complete_awaiting_press = false;
    if (tick_timer) lv_timer_resume(tick_timer);

    update_arc();
    update_countdown_label();
    update_controls();
    ESP_LOGI(TAG, "Advanced to phase %d", ts->phase);
}

static void tick_callback(lv_timer_t *timer)
{
    (void)timer;
    timer_state_t *ts = app_state_get_timer();
    if (!ts->is_running) return;
    if (ts->phase_complete_awaiting_press) return;

    if (ts->remaining_seconds > 0) {
        ts->remaining_seconds--;
        update_arc();
        update_countdown_label();
    }

    if (ts->remaining_seconds == 0) {
        ts->is_running = false;
        ts->phase_complete_awaiting_press = true;
        if (tick_timer) lv_timer_pause(tick_timer);
        update_countdown_label();
        update_controls();
        ESP_LOGI(TAG, "Phase complete, awaiting press");
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
            focus_idx = (focus_idx == 0) ? 1 : 0;
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
            ts->is_running = !ts->is_running;
            if (ts->is_running) {
                if (tick_timer) lv_timer_resume(tick_timer);
            } else {
                if (tick_timer) lv_timer_pause(tick_timer);
            }
            update_controls();
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
            }
            update_arc();
            update_countdown_label();
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
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, LV_COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* --- Title --- */
    lbl_title = lv_label_create(screen);
    lv_label_set_text(lbl_title, "Timer");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 12);

    /* --- Phase label (Pomodoro only, below title) --- */
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
    lv_arc_set_bg_angles(arc, 135, 405);
    lv_arc_set_angles(arc, 135, 135);
    lv_arc_set_mode(arc, LV_ARC_MODE_NORMAL);
    lv_obj_set_style_border_width(arc, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, LV_COLOR_PRIMARY_DARK, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, LV_COLOR_TIMER, LV_PART_INDICATOR);
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE);

    /* --- Countdown label (centered on arc) --- */
    lbl_countdown = lv_label_create(screen);
    lv_label_set_text(lbl_countdown, "00:00");
    lv_obj_set_style_text_font(lbl_countdown, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_countdown, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_countdown, LV_ALIGN_CENTER, 0, ARC_OFFSET_Y);

    /* --- Bottom control buttons --- */
    const char *icons[] = { LV_SYMBOL_PLAY, LV_SYMBOL_REFRESH, LV_SYMBOL_LEFT };
    for (int i = 0; i < TIMER_BTN_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(screen);
        lv_obj_set_size(btn, BTN_SIZE, BTN_SIZE);
        lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, (i - 1) * BTN_GAP, BTN_ROW_Y);
        lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(btn, LV_COLOR_PRIMARY_DARK, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(btn, 0, 0);
        lv_obj_set_style_border_color(btn, LV_COLOR_PRIMARY_LIGHT, 0);
        lv_obj_set_style_pad_all(btn, 0, 0);
        btn_objects[i] = btn;

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, icons[i]);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lbl, LV_COLOR_BG_CARD, 0);
        lv_obj_center(lbl);
        btn_labels[i] = lbl;
    }

    /* --- Nevermind button (hidden by default, used in confirm mode) --- */
    btn_nevermind = lv_btn_create(screen);
    lv_obj_set_size(btn_nevermind, 140, 40);
    lv_obj_align(btn_nevermind, LV_ALIGN_BOTTOM_MID, 0, BTN_ROW_Y - 55);
    lv_obj_set_style_radius(btn_nevermind, 20, 0);
    lv_obj_set_style_bg_color(btn_nevermind, LV_COLOR_PRIMARY_DARK, 0);
    lv_obj_set_style_shadow_width(btn_nevermind, 0, 0);
    lv_obj_set_style_shadow_opa(btn_nevermind, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_nevermind, 0, 0);
    lv_obj_set_style_border_color(btn_nevermind, LV_COLOR_PRIMARY_LIGHT, 0);
    lv_obj_set_style_pad_all(btn_nevermind, 0, 0);
    lv_obj_add_flag(btn_nevermind, LV_OBJ_FLAG_HIDDEN);

    lbl_nevermind = lv_label_create(btn_nevermind);
    lv_label_set_text(lbl_nevermind, "Nevermind");
    lv_obj_set_style_text_font(lbl_nevermind, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_nevermind, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_nevermind);

    /* --- 1-second tick timer --- */
    tick_timer = lv_timer_create(tick_callback, 1000, NULL);
    lv_timer_pause(tick_timer);

    /* --- Initialize visuals --- */
    timer_state_t *ts = app_state_get_timer();
    if (ts->total_seconds > 0) {
        char title_buf[32];
        timer_preset_t *p = timer_store_get_by_id(ts->preset_id);
        if (p) {
            snprintf(title_buf, sizeof(title_buf), "%s", p->name);
            lv_label_set_text(lbl_title, title_buf);
        }
    }

    update_arc();
    update_countdown_label();
    focus_idx = TIMER_BTN_PAUSE;
    prev_focus = -1;
    confirm_mode = false;
    update_controls();

    ESP_LOGI(TAG, "Timer screen created");
    return screen;
}
