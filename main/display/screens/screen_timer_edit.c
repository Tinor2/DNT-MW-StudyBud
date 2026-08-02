#include "screen_timer_edit.h"
#include "ui_manager.h"
#include "color_palette.h"
#include "../app_state.h"
#include "../utils/timer_store.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "Screen_TimerEdit";

#define SCREEN_W      480
#define SCREEN_H      480
#define CARD_W        420
#define FOCUS_ANIM_MS 200

#define MODE_BTN_W    260
#define MODE_BTN_H    44

#define TIME_FONT     lv_font_montserrat_48
#define TIME_LABEL_W  90

#define PHASE_BTN_W   100
#define PHASE_BTN_H   36
#define PHASE_BTN_GAP 110

#define CANCEL_SIZE   60
#define SAVE_W        180
#define SAVE_H        56
#define BTN_BOTTOM_Y  -30

typedef enum {
    FOCUS_MODE,
    FOCUS_PHASE_SESSION,
    FOCUS_PHASE_SHORT,
    FOCUS_PHASE_LONG,
    FOCUS_TIME,
    FOCUS_CANCEL,
    FOCUS_SAVE
} edit_focus_t;

static lv_obj_t *screen;
static lv_obj_t *container = NULL;

static lv_obj_t *mode_capsule;
static lv_obj_t *lbl_mode_text;

static lv_obj_t *lbl_hours;
static lv_obj_t *lbl_mins;
static lv_obj_t *lbl_secs;
static lv_obj_t *lbl_sep1;
static lv_obj_t *lbl_sep2;

static lv_obj_t *phase_session_btn;
static lv_obj_t *phase_short_btn;
static lv_obj_t *phase_long_btn;
static lv_obj_t *lbl_phase_session;
static lv_obj_t *lbl_phase_short;
static lv_obj_t *lbl_phase_long;
static bool pomo_buttons_exist = false;

static lv_obj_t *btn_cancel;
static lv_obj_t *btn_save;

static edit_focus_t focus_idx = FOCUS_MODE;

static bool time_armed = false;
static int  time_segment = 0;

static timer_type_t edit_type = TIMER_TYPE_STANDARD;
static int active_phase = 0; // 0 = Session, 1 = Short Break, 2 = Long Break

// Temporary in-memory state for edits so phase switching doesn't wipe unsaved values
static uint32_t pomo_phase_sec[3] = { 25 * 60, 5 * 60, 15 * 60 };
static uint32_t std_duration_sec = 25 * 60;

static uint32_t edit_hours = 0;
static uint32_t edit_mins = 25;
static uint32_t edit_secs = 0;

static bool editing_existing = false;
static int  edit_preset_id = -1;

static void update_focus_ring(void);
static void set_time_display(uint32_t h, uint32_t m, uint32_t s);
static void save_current_display_to_phase(void);
static void load_current_phase_to_display(void);

static void anim_set_border_width(void *var, int32_t val)
{
    lv_obj_set_style_border_width((lv_obj_t *)var, val, 0);
}

static void anim_start_border(lv_obj_t *obj, int32_t from, int32_t to, uint32_t time)
{
    if (!obj) return;
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

static void create_pomo_buttons(void)
{
    if (pomo_buttons_exist) return;

    lv_obj_t *parent = container ? container : screen;
    int y = 0;

    phase_session_btn = lv_btn_create(parent);
    lv_obj_set_size(phase_session_btn, PHASE_BTN_W, PHASE_BTN_H);
    lv_obj_align(phase_session_btn, LV_ALIGN_CENTER, -PHASE_BTN_GAP, y);
    lv_obj_set_style_radius(phase_session_btn, 12, 0);
    lv_obj_set_style_bg_color(phase_session_btn, theme_accent_dark(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_shadow_width(phase_session_btn, 0, 0);
    lv_obj_set_style_border_width(phase_session_btn, 0, 0);
    lv_obj_set_style_border_color(phase_session_btn, theme_accent_light(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_pad_all(phase_session_btn, 0, 0);
    lbl_phase_session = lv_label_create(phase_session_btn);
    lv_label_set_text(lbl_phase_session, "Session");
    lv_obj_set_style_text_font(lbl_phase_session, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_phase_session, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_phase_session);

    phase_short_btn = lv_btn_create(parent);
    lv_obj_set_size(phase_short_btn, PHASE_BTN_W, PHASE_BTN_H);
    lv_obj_align(phase_short_btn, LV_ALIGN_CENTER, 0, y);
    lv_obj_set_style_radius(phase_short_btn, 12, 0);
    lv_obj_set_style_bg_color(phase_short_btn, theme_accent_dark(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_shadow_width(phase_short_btn, 0, 0);
    lv_obj_set_style_border_width(phase_short_btn, 0, 0);
    lv_obj_set_style_border_color(phase_short_btn, theme_accent_light(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_pad_all(phase_short_btn, 0, 0);
    lbl_phase_short = lv_label_create(phase_short_btn);
    lv_label_set_text(lbl_phase_short, "Short");
    lv_obj_set_style_text_font(lbl_phase_short, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_phase_short, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_phase_short);

    phase_long_btn = lv_btn_create(parent);
    lv_obj_set_size(phase_long_btn, PHASE_BTN_W, PHASE_BTN_H);
    lv_obj_align(phase_long_btn, LV_ALIGN_CENTER, PHASE_BTN_GAP, y);
    lv_obj_set_style_radius(phase_long_btn, 12, 0);
    lv_obj_set_style_bg_color(phase_long_btn, theme_accent_dark(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_shadow_width(phase_long_btn, 0, 0);
    lv_obj_set_style_border_width(phase_long_btn, 0, 0);
    lv_obj_set_style_border_color(phase_long_btn, theme_accent_light(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_pad_all(phase_long_btn, 0, 0);
    lbl_phase_long = lv_label_create(phase_long_btn);
    lv_label_set_text(lbl_phase_long, "Long");
    lv_obj_set_style_text_font(lbl_phase_long, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_phase_long, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_phase_long);

    pomo_buttons_exist = true;
}

static void delete_pomo_buttons(void)
{
    if (!pomo_buttons_exist) return;
    lv_obj_del(phase_session_btn);
    lv_obj_del(phase_short_btn);
    lv_obj_del(phase_long_btn);
    phase_session_btn = NULL;
    phase_short_btn = NULL;
    phase_long_btn = NULL;
    lbl_phase_session = NULL;
    lbl_phase_short = NULL;
    lbl_phase_long = NULL;
    pomo_buttons_exist = false;
}

static void set_time_display(uint32_t h, uint32_t m, uint32_t s)
{
    char buf[4];
    snprintf(buf, sizeof(buf), "%02lu", (unsigned long)h);
    lv_label_set_text(lbl_hours, buf);
    snprintf(buf, sizeof(buf), "%02lu", (unsigned long)m);
    lv_label_set_text(lbl_mins, buf);
    snprintf(buf, sizeof(buf), "%02lu", (unsigned long)s);
    lv_label_set_text(lbl_secs, buf);
}

static void save_current_display_to_phase(void)
{
    uint32_t total_sec = edit_hours * 3600 + edit_mins * 60 + edit_secs;
    if (edit_type == TIMER_TYPE_POMODORO) {
        if (active_phase >= 0 && active_phase < 3) {
            pomo_phase_sec[active_phase] = total_sec;
        }
    } else {
        std_duration_sec = total_sec;
    }
}

static void load_current_phase_to_display(void)
{
    uint32_t sec = (edit_type == TIMER_TYPE_POMODORO) ? pomo_phase_sec[active_phase] : std_duration_sec;
    edit_hours = sec / 3600;
    edit_mins = (sec % 3600) / 60;
    edit_secs = sec % 60;
    set_time_display(edit_hours, edit_mins, edit_secs);
}

static void update_focus_ring(void)
{
    bool pomo = (edit_type == TIMER_TYPE_POMODORO);

    /* --- Mode capsule --- */
    bool mode_focused = (focus_idx == FOCUS_MODE);
    anim_start_border(mode_capsule, lv_obj_get_style_border_width(mode_capsule, 0), mode_focused ? 3 : 0, FOCUS_ANIM_MS);
    lv_obj_set_style_bg_color(mode_capsule, mode_focused ? theme_accent(SCREEN_TIMER_EDIT) : theme_accent_dark(SCREEN_TIMER_EDIT), 0);

    /* --- Phase buttons --- */
    if (pomo && pomo_buttons_exist) {
        lv_obj_t *btns[] = { phase_session_btn, phase_short_btn, phase_long_btn };
        lv_obj_t *lbls[] = { lbl_phase_session, lbl_phase_short, lbl_phase_long };
        for (int i = 0; i < 3; i++) {
            bool focused = ((int)focus_idx == FOCUS_PHASE_SESSION + i);
            bool is_active = (active_phase == i);
            anim_start_border(btns[i], lv_obj_get_style_border_width(btns[i], 0), focused ? 3 : 0, FOCUS_ANIM_MS);
            
            // Highlight selected phase with primary color, non-selected with dark background
            lv_obj_set_style_bg_color(btns[i], (focused || is_active) ? theme_accent(SCREEN_TIMER_EDIT) : theme_accent_dark(SCREEN_TIMER_EDIT), 0);
            if (lbls[i]) {
                lv_obj_set_style_text_color(lbls[i], (focused || is_active) ? LV_COLOR_BG_CARD : LV_COLOR_SECONDARY_LIGHT, 0);
            }
        }
    }

    /* --- Time display --- */
    bool time_focused = (focus_idx == FOCUS_TIME);
    lv_obj_t *time_segs[] = { lbl_hours, lbl_mins, lbl_secs };
    for (int i = 0; i < 3; i++) {
        if (!time_segs[i]) continue;
        if (time_armed && i == time_segment) {
            lv_obj_set_style_bg_color(time_segs[i], theme_accent(SCREEN_TIMER_EDIT), 0);
            lv_obj_set_style_bg_opa(time_segs[i], LV_OPA_30, 0);
            lv_obj_set_style_border_width(time_segs[i], 3, 0);
            lv_obj_set_style_border_color(time_segs[i], theme_accent(SCREEN_TIMER_EDIT), 0);
            lv_obj_set_style_text_color(time_segs[i], theme_accent(SCREEN_TIMER_EDIT), 0);
        } else if (time_focused && !time_armed) {
            lv_obj_set_style_bg_color(time_segs[i], theme_accent_light(SCREEN_TIMER_EDIT), 0);
            lv_obj_set_style_bg_opa(time_segs[i], LV_OPA_20, 0);
            lv_obj_set_style_border_width(time_segs[i], 2, 0);
            lv_obj_set_style_border_color(time_segs[i], theme_accent_light(SCREEN_TIMER_EDIT), 0);
            lv_obj_set_style_text_color(time_segs[i], LV_COLOR_TEXT, 0);
        } else {
            lv_obj_set_style_bg_opa(time_segs[i], LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(time_segs[i], 0, 0);
            lv_obj_set_style_text_color(time_segs[i], LV_COLOR_TEXT, 0);
        }
    }

    /* --- Cancel button --- */
    bool cancel_focused = (focus_idx == FOCUS_CANCEL);
    anim_start_border(btn_cancel, lv_obj_get_style_border_width(btn_cancel, 0), cancel_focused ? 3 : 0, FOCUS_ANIM_MS);
    lv_obj_set_style_bg_color(btn_cancel, cancel_focused ? LV_COLOR_ERROR : theme_accent_dark(SCREEN_TIMER_EDIT), 0);

    /* --- Save button --- */
    bool save_focused = (focus_idx == FOCUS_SAVE);
    anim_start_border(btn_save, lv_obj_get_style_border_width(btn_save, 0), save_focused ? 3 : 0, FOCUS_ANIM_MS);
    lv_obj_set_style_bg_color(btn_save, save_focused ? theme_accent(SCREEN_TIMER_EDIT) : theme_accent_dark(SCREEN_TIMER_EDIT), 0);
}

void screen_timer_edit_encoder_event(lv_indev_data_t *data)
{
    bool pomo = (edit_type == TIMER_TYPE_POMODORO);

    if (time_armed) {
        if (data->enc_diff != 0) {
            if (time_segment == 0) {
                edit_hours = (edit_hours + data->enc_diff + 24) % 24;
            } else if (time_segment == 1) {
                edit_mins = (edit_mins + data->enc_diff + 60) % 60;
            } else {
                edit_secs = (edit_secs + data->enc_diff + 60) % 60;
            }
            set_time_display(edit_hours, edit_mins, edit_secs);
            save_current_display_to_phase();
        }

        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            time_segment++;
            if (time_segment >= 3) {
                time_armed = false;
                time_segment = 0;
            }
            update_focus_ring();
        }
        return;
    }

    /* Fixed Encoder Focus Order Array */
    static const edit_focus_t pomo_order[] = {
        FOCUS_MODE, FOCUS_PHASE_SESSION, FOCUS_PHASE_SHORT, FOCUS_PHASE_LONG, FOCUS_TIME, FOCUS_CANCEL, FOCUS_SAVE
    };
    static const edit_focus_t std_order[] = {
        FOCUS_MODE, FOCUS_TIME, FOCUS_CANCEL, FOCUS_SAVE
    };

    const edit_focus_t *order = pomo ? pomo_order : std_order;
    int count = pomo ? 7 : 4;

    if (data->enc_diff != 0) {
        int cur_idx = 0;
        for (int i = 0; i < count; i++) {
            if (order[i] == focus_idx) {
                cur_idx = i;
                break;
            }
        }
        cur_idx = (cur_idx + data->enc_diff) % count;
        if (cur_idx < 0) cur_idx += count;
        focus_idx = order[cur_idx];
        update_focus_ring();
    }

    if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
        switch (focus_idx) {
        case FOCUS_MODE:
            save_current_display_to_phase();
            edit_type = (edit_type == TIMER_TYPE_STANDARD) ? TIMER_TYPE_POMODORO : TIMER_TYPE_STANDARD;
            lv_label_set_text(lbl_mode_text, edit_type == TIMER_TYPE_POMODORO ? "Pomodoro" : "Normal");

            if (edit_type == TIMER_TYPE_POMODORO) {
                create_pomo_buttons();
                active_phase = 0;
            } else {
                delete_pomo_buttons();
            }
            load_current_phase_to_display();
            update_focus_ring();
            break;

        case FOCUS_PHASE_SESSION:
            save_current_display_to_phase();
            active_phase = 0;
            load_current_phase_to_display();
            update_focus_ring();
            break;

        case FOCUS_PHASE_SHORT:
            save_current_display_to_phase();
            active_phase = 1;
            load_current_phase_to_display();
            update_focus_ring();
            break;

        case FOCUS_PHASE_LONG:
            save_current_display_to_phase();
            active_phase = 2;
            load_current_phase_to_display();
            update_focus_ring();
            break;

        case FOCUS_TIME:
            time_armed = true;
            time_segment = 0;
            update_focus_ring();
            break;

        case FOCUS_CANCEL:
            ui_manager_switch_screen(SCREEN_TIMER_PRESETS);
            break;

        case FOCUS_SAVE: {
            save_current_display_to_phase();

            timer_preset_t preset;
            memset(&preset, 0, sizeof(preset));
            preset.type = edit_type;
            if (editing_existing) {
                strncpy(preset.name, "Edited Timer", TIMER_STORE_NAME_LEN - 1);
            } else {
                int n = timer_store_next_id();
                snprintf(preset.name, TIMER_STORE_NAME_LEN - 1,
                    edit_type == TIMER_TYPE_POMODORO ? "Pomodoro %d" : "Normal %d", n);
            }

            if (edit_type == TIMER_TYPE_POMODORO) {
                preset.session_sec = pomo_phase_sec[0];
                preset.short_break_sec = pomo_phase_sec[1];
                preset.long_break_sec = pomo_phase_sec[2];
            } else {
                preset.duration_sec = std_duration_sec;
            }

            if (editing_existing) {
                timer_store_update(edit_preset_id, &preset);
            } else {
                int new_id = timer_store_add(&preset);
                if (new_id >= 0) edit_preset_id = new_id;
            }

            ui_manager_switch_screen(SCREEN_TIMER_PRESETS);
            ESP_LOGI(TAG, "Saved preset (id=%d, type=%d)", edit_preset_id, edit_type);
            break;
        }

        default:
            break;
        }
    }
}

lv_obj_t *screen_timer_edit_create(void)
{
    if (screen) {
        lv_obj_del(screen);
    }
    screen = NULL;
    container = NULL;
    pomo_buttons_exist = false;
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, pastel_color(theme_accent(SCREEN_TIMER_EDIT)), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* --- Container card --- */
    container = lv_obj_create(screen);
    lv_obj_set_size(container, 420, 380);
    lv_obj_center(container);
    lv_obj_set_style_radius(container, 24, 0);
    lv_obj_set_style_bg_color(container, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_shadow_width(container, 8, 0);
    lv_obj_set_style_shadow_opa(container, LV_OPA_20, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_clear_flag(container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(container, 0, 0);

    /* --- Title --- */
    lv_obj_t *lbl_title = lv_label_create(container);
    lv_label_set_text(lbl_title, "Edit Timer");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 16);

    /* --- Mode capsule --- */
    mode_capsule = lv_btn_create(container);
    lv_obj_set_size(mode_capsule, MODE_BTN_W, MODE_BTN_H);
    lv_obj_align(mode_capsule, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_radius(mode_capsule, MODE_BTN_H / 2, 0);
    lv_obj_set_style_bg_color(mode_capsule, theme_accent_dark(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_shadow_width(mode_capsule, 0, 0);
    lv_obj_set_style_border_width(mode_capsule, 0, 0);
    lv_obj_set_style_border_color(mode_capsule, theme_accent_light(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_pad_all(mode_capsule, 0, 0);

    lbl_mode_text = lv_label_create(mode_capsule);
    lv_label_set_text(lbl_mode_text, "Timer");
    lv_obj_set_style_text_font(lbl_mode_text, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_mode_text, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_mode_text);

    /* --- Time display: HH : MM : SS --- */
    int time_y = 120;
    int seg_w = TIME_LABEL_W;
    int sep_gap = 12;
    int total_w = seg_w * 3 + sep_gap * 2;
    int start_x = (CARD_W - total_w) / 2;

    lbl_hours = lv_label_create(container);
    lv_label_set_text(lbl_hours, "00");
    lv_obj_set_style_text_font(lbl_hours, &TIME_FONT, 0);
    lv_obj_set_style_text_color(lbl_hours, LV_COLOR_TEXT, 0);
    lv_obj_set_style_radius(lbl_hours, 8, 0);
    lv_obj_set_style_pad_left(lbl_hours, 10, 0);
    lv_obj_set_style_pad_right(lbl_hours, 10, 0);
    lv_obj_set_style_border_width(lbl_hours, 0, 0);
    lv_obj_set_style_border_color(lbl_hours, theme_accent(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_bg_opa(lbl_hours, LV_OPA_TRANSP, 0);
    lv_obj_align(lbl_hours, LV_ALIGN_TOP_LEFT, start_x, time_y);

    lbl_sep1 = lv_label_create(container);
    lv_label_set_text(lbl_sep1, ":");
    lv_obj_set_style_text_font(lbl_sep1, &TIME_FONT, 0);
    lv_obj_set_style_text_color(lbl_sep1, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_sep1, LV_ALIGN_TOP_LEFT, start_x + seg_w, time_y);

    lbl_mins = lv_label_create(container);
    lv_label_set_text(lbl_mins, "25");
    lv_obj_set_style_text_font(lbl_mins, &TIME_FONT, 0);
    lv_obj_set_style_text_color(lbl_mins, LV_COLOR_TEXT, 0);
    lv_obj_set_style_radius(lbl_mins, 8, 0);
    lv_obj_set_style_pad_left(lbl_mins, 10, 0);
    lv_obj_set_style_pad_right(lbl_mins, 10, 0);
    lv_obj_set_style_border_width(lbl_mins, 0, 0);
    lv_obj_set_style_border_color(lbl_mins, theme_accent(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_bg_opa(lbl_mins, LV_OPA_TRANSP, 0);
    lv_obj_align(lbl_mins, LV_ALIGN_TOP_LEFT, start_x + seg_w + sep_gap, time_y);

    lbl_sep2 = lv_label_create(container);
    lv_label_set_text(lbl_sep2, ":");
    lv_obj_set_style_text_font(lbl_sep2, &TIME_FONT, 0);
    lv_obj_set_style_text_color(lbl_sep2, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_sep2, LV_ALIGN_TOP_LEFT, start_x + seg_w * 2 + sep_gap, time_y);

    lbl_secs = lv_label_create(container);
    lv_label_set_text(lbl_secs, "00");
    lv_obj_set_style_text_font(lbl_secs, &TIME_FONT, 0);
    lv_obj_set_style_text_color(lbl_secs, LV_COLOR_TEXT, 0);
    lv_obj_set_style_radius(lbl_secs, 8, 0);
    lv_obj_set_style_pad_left(lbl_secs, 10, 0);
    lv_obj_set_style_pad_right(lbl_secs, 10, 0);
    lv_obj_set_style_border_width(lbl_secs, 0, 0);
    lv_obj_set_style_border_color(lbl_secs, theme_accent(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_bg_opa(lbl_secs, LV_OPA_TRANSP, 0);
    lv_obj_align(lbl_secs, LV_ALIGN_TOP_LEFT, start_x + seg_w * 2 + sep_gap * 2, time_y);

    /* --- Cancel button (bottom-left of card) --- */
    btn_cancel = lv_btn_create(container);
    lv_obj_set_size(btn_cancel, CANCEL_SIZE, CANCEL_SIZE);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 30, -20);
    lv_obj_set_style_radius(btn_cancel, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn_cancel, theme_accent_dark(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_shadow_width(btn_cancel, 0, 0);
    lv_obj_set_style_border_width(btn_cancel, 0, 0);
    lv_obj_set_style_border_color(btn_cancel, LV_COLOR_ERROR, 0);
    lv_obj_set_style_pad_all(btn_cancel, 0, 0);
    lv_obj_t *lbl_x = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_x, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(lbl_x, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_x, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_x);

    /* --- Save button (bottom-right of card) --- */
    btn_save = lv_btn_create(container);
    lv_obj_set_size(btn_save, SAVE_W, SAVE_H);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -30, -20);
    lv_obj_set_style_radius(btn_save, SAVE_H / 2, 0);
    lv_obj_set_style_bg_color(btn_save, theme_accent_dark(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_shadow_width(btn_save, 0, 0);
    lv_obj_set_style_border_width(btn_save, 0, 0);
    lv_obj_set_style_border_color(btn_save, theme_accent_light(SCREEN_TIMER_EDIT), 0);
    lv_obj_set_style_pad_all(btn_save, 0, 0);
    lv_obj_t *lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "SAVE");
    lv_obj_set_style_text_font(lbl_save, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_save, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_save);

    /* --- Load preset state --- */
    editing_existing = false;
    edit_preset_id = -1;
    edit_type = TIMER_TYPE_STANDARD;
    active_phase = 0;

    int target_id = app_state_get()->active_preset_id;
    if (target_id >= 0) {
        timer_preset_t *p = timer_store_get_by_id(target_id);
        if (p) {
            editing_existing = true;
            edit_preset_id = p->id;
            edit_type = p->type;
            lv_label_set_text(lbl_mode_text, p->type == TIMER_TYPE_POMODORO ? "Pomodoro" : "Normal");

            if (p->type == TIMER_TYPE_POMODORO) {
                create_pomo_buttons();
                pomo_phase_sec[0] = p->session_sec;
                pomo_phase_sec[1] = p->short_break_sec;
                pomo_phase_sec[2] = p->long_break_sec;
            } else {
                std_duration_sec = p->duration_sec;
            }
        }
    }

    load_current_phase_to_display();
    focus_idx = FOCUS_MODE;
    time_armed = false;
    time_segment = 0;
    update_focus_ring();

    ESP_LOGI(TAG, "Timer edit screen created (preset_id=%d)", edit_preset_id);
    return screen;
}