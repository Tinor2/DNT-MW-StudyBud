#include "screen_sleep.h"
#include "ui_manager.h"
#include "studybud_theme.h"
#include "../utils/sleep_store.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#ifdef LV_USE_CHART
#include "extra/widgets/chart/lv_chart.h"
#endif

static const char *TAG = "Screen_Sleep";

#define TRANSITION_TIME  350
#define FOCUS_ANIM_MS    300

typedef enum {
    SLEEP_STATE_INTRO,
    SLEEP_STATE_START,
    SLEEP_STATE_ACTIVE,
    SLEEP_STATE_SUMMARY,
    SLEEP_STATE_WEEKLY
} sleep_state_t;

static sleep_state_t current_state = SLEEP_STATE_INTRO;
static lv_obj_t *screen;

static int focus_index = 0;
static int prev_focus_index = -1;
static bool confirm_mode = false;

static lv_timer_t *clock_timer = NULL;

/* --- State: INTRO --- */
static lv_obj_t *lbl_title;
static lv_obj_t *btn_start_session;
static lv_obj_t *btn_weekly_review;

/* --- State: START --- */
static lv_obj_t *lbl_greeting;
static lv_obj_t *clock_frame;
static lv_obj_t *lbl_clock;
static lv_obj_t *btn_start_now;
static lv_obj_t *btn_back;

/* --- State: ACTIVE --- */
static lv_obj_t *lbl_tracking;
static lv_obj_t *btn_awake;
static lv_obj_t *lbl_awake_text;
static lv_obj_t *lbl_awake_hint;
static lv_obj_t *badge_pulse;
static lv_obj_t *lbl_goodnight;
static lv_obj_t *lbl_active_clock;
static lv_obj_t *btn_nevermind;

/* --- State: SUMMARY --- */
static lv_obj_t *lbl_result;
static lv_obj_t *lbl_sub;
static lv_obj_t *lbl_hint;

/* --- State: WEEKLY --- */
static lv_obj_t *lbl_weekly_header;
static lv_obj_t *lbl_weekly_avg;
static lv_obj_t *lbl_weekly_msg;
static lv_obj_t *chart_weekly;
static lv_chart_series_t *chart_series;
static lv_obj_t *btn_weekly_back;

static void update_focus_styles(void);
static void animate_style(lv_obj_t *obj, lv_anim_exec_xcb_t exec_cb,
                          int32_t from, int32_t to, uint32_t time, uint32_t delay);
static void transition_to_intro(void);
static void transition_to_start(void);
static void transition_to_active(void);
static void enter_confirm_mode(void);
static void exit_confirm_mode(void);
static void transition_to_summary(void);
static void transition_to_weekly(void);

static void anim_set_opa(void *var, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)val, 0);
}

static void anim_set_border_width(void *var, int32_t val)
{
    lv_obj_set_style_border_width((lv_obj_t *)var, val, 0);
}

static void animate_style(lv_obj_t *obj, lv_anim_exec_xcb_t exec_cb,
                          int32_t from, int32_t to, uint32_t time, uint32_t delay)
{
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

static void start_pulse_animation(void)
{
    if (!badge_pulse) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, badge_pulse);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_set_opa);
    lv_anim_set_values(&a, LV_OPA_30, LV_OPA_80);
    lv_anim_set_time(&a, 1500);
    lv_anim_set_playback_time(&a, 1500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void update_time_cb(lv_timer_t *timer)
{
    (void)timer;
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    char time_buf[8];
    strftime(time_buf, sizeof(time_buf), "%H:%M", &timeinfo);

    if (lbl_clock) lv_label_set_text(lbl_clock, time_buf);
    if (lbl_active_clock) lv_label_set_text(lbl_active_clock, time_buf);
}

static const char *get_greeting(void)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);

    int hour = timeinfo.tm_hour;
    if (hour >= 6 && hour < 12) return "Good morning";
    if (hour >= 12 && hour < 18) return "Good afternoon";
    return "Good evening";
}

static void format_duration(char *buf, size_t len, uint16_t minutes)
{
    uint16_t h = minutes / 60;
    uint16_t m = minutes % 60;
    if (h > 0 && m > 0) {
        snprintf(buf, len, "You slept for %dh %dm", h, m);
    } else if (h > 0) {
        snprintf(buf, len, "You slept for %d hrs", h);
    } else {
        snprintf(buf, len, "You slept for %d min", m);
    }
}

/* ============================================================
 *  FOCUS STYLES
 * ============================================================ */
static void update_focus_styles(void)
{
    if (current_state == SLEEP_STATE_INTRO) {
        if (focus_index != prev_focus_index) {
            lv_obj_t *focused = (focus_index == 0) ? btn_start_session : btn_weekly_review;
            lv_obj_t *defocused = (focus_index == 0) ? btn_weekly_review : btn_start_session;

            animate_style(focused, (lv_anim_exec_xcb_t)anim_set_border_width,
                          0, 4, FOCUS_ANIM_MS, 0);
            animate_style(focused, (lv_anim_exec_xcb_t)anim_set_opa,
                          LV_OPA_80, LV_OPA_COVER, FOCUS_ANIM_MS, 0);

            animate_style(defocused, (lv_anim_exec_xcb_t)anim_set_border_width,
                          4, 0, FOCUS_ANIM_MS, 0);
            animate_style(defocused, (lv_anim_exec_xcb_t)anim_set_opa,
                          LV_OPA_COVER, LV_OPA_80, FOCUS_ANIM_MS, 0);

            prev_focus_index = focus_index;
        }
    } else if (current_state == SLEEP_STATE_START) {
        if (focus_index != prev_focus_index) {
            lv_obj_t *focused = (focus_index == 0) ? btn_start_now : btn_back;
            lv_obj_t *defocused = (focus_index == 0) ? btn_back : btn_start_now;

            animate_style(focused, (lv_anim_exec_xcb_t)anim_set_border_width,
                          0, 4, FOCUS_ANIM_MS, 0);
            animate_style(focused, (lv_anim_exec_xcb_t)anim_set_opa,
                          LV_OPA_80, LV_OPA_COVER, FOCUS_ANIM_MS, 0);

            animate_style(defocused, (lv_anim_exec_xcb_t)anim_set_border_width,
                          4, 0, FOCUS_ANIM_MS, 0);
            animate_style(defocused, (lv_anim_exec_xcb_t)anim_set_opa,
                          LV_OPA_COVER, LV_OPA_80, FOCUS_ANIM_MS, 0);

            prev_focus_index = focus_index;
        }
    } else if (current_state == SLEEP_STATE_ACTIVE && confirm_mode) {
        if (focus_index != prev_focus_index) {
            lv_obj_t *focused = (focus_index == 0) ? btn_awake : btn_nevermind;
            lv_obj_t *defocused = (focus_index == 0) ? btn_nevermind : btn_awake;

            animate_style(focused, (lv_anim_exec_xcb_t)anim_set_border_width,
                          0, 4, FOCUS_ANIM_MS, 0);
            animate_style(focused, (lv_anim_exec_xcb_t)anim_set_opa,
                          LV_OPA_80, LV_OPA_COVER, FOCUS_ANIM_MS, 0);

            animate_style(defocused, (lv_anim_exec_xcb_t)anim_set_border_width,
                          4, 0, FOCUS_ANIM_MS, 0);
            animate_style(defocused, (lv_anim_exec_xcb_t)anim_set_opa,
                          LV_OPA_COVER, LV_OPA_80, FOCUS_ANIM_MS, 0);

            prev_focus_index = focus_index;
        }
    }
}

/* ============================================================
 *  STATE TRANSITIONS
 * ============================================================ */
static void hide_all_widgets(void)
{
    lv_obj_add_flag(lbl_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_start_session, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_weekly_review, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_greeting, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(clock_frame, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_clock, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_start_now, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_back, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_tracking, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_awake, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_awake_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(badge_pulse, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_goodnight, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_active_clock, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_nevermind, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_result, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_sub, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_weekly_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_weekly_avg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_weekly_msg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(chart_weekly, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_weekly_back, LV_OBJ_FLAG_HIDDEN);
}

static void transition_to_intro(void)
{
    lv_anim_del(badge_pulse, NULL);
    hide_all_widgets();

    current_state = SLEEP_STATE_INTRO;
    confirm_mode = false;
    focus_index = 0;
    prev_focus_index = -1;

    lv_obj_clear_flag(lbl_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_start_session, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_weekly_review, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(lbl_title, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(btn_start_session, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(btn_weekly_review, LV_OPA_COVER, 0);

    lv_obj_set_style_transform_width(btn_start_session, 0, 0);
    lv_obj_set_style_transform_height(btn_start_session, 0, 0);
    lv_obj_set_style_border_width(btn_start_session, 0, 0);
    lv_obj_set_style_transform_width(btn_weekly_review, 0, 0);
    lv_obj_set_style_transform_height(btn_weekly_review, 0, 0);
    lv_obj_set_style_border_width(btn_weekly_review, 0, 0);

    update_focus_styles();
    ESP_LOGI(TAG, "Transitioned to Intro state");
}

static void transition_to_start(void)
{
    hide_all_widgets();

    current_state = SLEEP_STATE_START;
    confirm_mode = false;
    focus_index = 0;
    prev_focus_index = -1;

    lv_label_set_text(lbl_greeting, get_greeting());

    lv_obj_clear_flag(lbl_greeting, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(clock_frame, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_clock, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_start_now, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_back, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(lbl_greeting, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(clock_frame, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(lbl_clock, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(btn_start_now, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(btn_back, LV_OPA_COVER, 0);

    lv_obj_set_style_transform_width(btn_start_now, 0, 0);
    lv_obj_set_style_transform_height(btn_start_now, 0, 0);
    lv_obj_set_style_border_width(btn_start_now, 0, 0);
    lv_obj_set_style_transform_width(btn_back, 0, 0);
    lv_obj_set_style_transform_height(btn_back, 0, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);

    update_time_cb(NULL);
    update_focus_styles();
    ESP_LOGI(TAG, "Transitioned to Start state");
}

static void transition_to_active(void)
{
    hide_all_widgets();

    current_state = SLEEP_STATE_ACTIVE;
    confirm_mode = false;
    focus_index = 0;
    prev_focus_index = -1;

    lv_label_set_text(lbl_awake_text, "AWAKE?");
    lv_obj_set_style_bg_color(btn_awake, LV_COLOR_SUCCESS, 0);
    lv_obj_add_flag(btn_nevermind, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(lbl_tracking, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_awake, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_awake_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(badge_pulse, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_goodnight, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_active_clock, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(lbl_tracking, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(btn_awake, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(lbl_awake_hint, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(badge_pulse, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(lbl_goodnight, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(lbl_active_clock, LV_OPA_COVER, 0);

    lv_obj_set_style_opa(badge_pulse, LV_OPA_50, 0);
    start_pulse_animation();

    update_time_cb(NULL);
    ESP_LOGI(TAG, "Transitioned to Active state");
}

static void enter_confirm_mode(void)
{
    confirm_mode = true;
    focus_index = 0;
    prev_focus_index = -1;

    lv_label_set_text(lbl_awake_text, "ARE YOU SURE?");
    lv_obj_set_style_bg_color(btn_awake, LV_COLOR_ERROR, 0);
    lv_obj_set_style_border_color(btn_awake, LV_COLOR_ERROR, 0);
    lv_obj_clear_flag(btn_nevermind, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(btn_nevermind, LV_OPA_COVER, 0);

    lv_obj_set_style_transform_width(btn_awake, 0, 0);
    lv_obj_set_style_transform_height(btn_awake, 0, 0);
    lv_obj_set_style_border_width(btn_awake, 0, 0);
    lv_obj_set_style_transform_width(btn_nevermind, 0, 0);
    lv_obj_set_style_transform_height(btn_nevermind, 0, 0);
    lv_obj_set_style_border_width(btn_nevermind, 0, 0);

    update_focus_styles();
    ESP_LOGI(TAG, "Entered confirm mode");
}

static void exit_confirm_mode(void)
{
    confirm_mode = false;
    focus_index = 0;
    prev_focus_index = -1;

    lv_label_set_text(lbl_awake_text, "AWAKE?");
    lv_obj_set_style_bg_color(btn_awake, LV_COLOR_SUCCESS, 0);
    lv_obj_set_style_border_color(btn_awake, LV_COLOR_SUCCESS, 0);
    lv_obj_add_flag(btn_nevermind, LV_OBJ_FLAG_HIDDEN);

    lv_obj_set_style_transform_width(btn_awake, 0, 0);
    lv_obj_set_style_transform_height(btn_awake, 0, 0);
    lv_obj_set_style_border_width(btn_awake, 0, 0);

    ESP_LOGI(TAG, "Exited confirm mode");
}

static void transition_to_summary(void)
{
    lv_anim_del(badge_pulse, NULL);
    hide_all_widgets();

    current_state = SLEEP_STATE_SUMMARY;
    confirm_mode = false;

    uint32_t duration = sleep_store_end_session();
    char result_buf[48];
    format_duration(result_buf, sizeof(result_buf), (uint16_t)duration);
    lv_label_set_text(lbl_result, result_buf);

    float avg = sleep_store_get_weekly_avg_hours();
    if (avg > 0) {
        lv_label_set_text_fmt(lbl_sub, "Weekly avg: %.1f hrs", avg);
    } else {
        lv_label_set_text(lbl_sub, "Start building your sleep history!");
    }

    lv_obj_clear_flag(lbl_result, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_sub, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(lbl_result, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(lbl_sub, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(lbl_hint, LV_OPA_COVER, 0);

    ESP_LOGI(TAG, "Transitioned to Summary state");
}

static void transition_to_weekly(void)
{
    hide_all_widgets();

    current_state = SLEEP_STATE_WEEKLY;
    confirm_mode = false;
    focus_index = 0;
    prev_focus_index = -1;

    float avg = sleep_store_get_weekly_avg_hours();
    lv_label_set_text_fmt(lbl_weekly_avg, "%.1f hrs", avg);

    if (avg >= 7.0f) {
        lv_label_set_text(lbl_weekly_msg, "Great consistency this week!");
    } else if (avg >= 5.0f) {
        lv_label_set_text(lbl_weekly_msg, "Try to get a bit more rest!");
    } else if (avg > 0.0f) {
        lv_label_set_text(lbl_weekly_msg, "Your sleep could use some love!");
    } else {
        lv_label_set_text(lbl_weekly_msg, "No data yet — start logging!");
    }

    uint16_t days[7];
    sleep_store_get_last_7_days(days);
    if (chart_series) {
        lv_chart_set_ext_y_array(chart_weekly, chart_series, (lv_coord_t *)days);
    }

    lv_obj_clear_flag(lbl_weekly_header, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_weekly_avg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_weekly_msg, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(chart_weekly, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_weekly_back, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(lbl_weekly_header, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(lbl_weekly_avg, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(lbl_weekly_msg, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(chart_weekly, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(btn_weekly_back, LV_OPA_COVER, 0);

    lv_obj_set_style_transform_width(btn_weekly_back, 0, 0);
    lv_obj_set_style_transform_height(btn_weekly_back, 0, 0);
    lv_obj_set_style_border_width(btn_weekly_back, 0, 0);

    ESP_LOGI(TAG, "Transitioned to Weekly state");
}

/* ============================================================
 *  SCREEN CREATE
 * ============================================================ */
lv_obj_t *screen_sleep_create(void)
{
    sleep_store_init();

    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, LV_COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- INTRO STATE ---- */
    lbl_title = lv_label_create(screen);
    lv_label_set_text(lbl_title, "Sleep");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 30);

    btn_start_session = lv_btn_create(screen);
    lv_obj_set_size(btn_start_session, 240, 240);
    lv_obj_align(btn_start_session, LV_ALIGN_CENTER, 0, -20);
    lv_obj_set_style_radius(btn_start_session, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn_start_session, LV_COLOR_PRIMARY, 0);
    lv_obj_set_style_shadow_width(btn_start_session, 0, 0);
    lv_obj_set_style_shadow_opa(btn_start_session, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_start_session, 0, 0);
    lv_obj_set_style_border_color(btn_start_session, LV_COLOR_PRIMARY_LIGHT, 0);
    lv_obj_set_style_pad_all(btn_start_session, 0, 0);
    lv_obj_t *lbl_start_session = lv_label_create(btn_start_session);
    lv_label_set_text(lbl_start_session, "START\nSESSION");
    lv_obj_set_style_text_font(lbl_start_session, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_start_session, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_text_align(lbl_start_session, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_start_session);

    btn_weekly_review = lv_btn_create(screen);
    lv_obj_set_size(btn_weekly_review, 160, 50);
    lv_obj_align(btn_weekly_review, LV_ALIGN_CENTER, 0, 150);
    lv_obj_set_style_radius(btn_weekly_review, 25, 0);
    lv_obj_set_style_bg_color(btn_weekly_review, LV_COLOR_PRIMARY_DARK, 0);
    lv_obj_set_style_shadow_width(btn_weekly_review, 0, 0);
    lv_obj_set_style_shadow_opa(btn_weekly_review, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_weekly_review, 0, 0);
    lv_obj_set_style_border_color(btn_weekly_review, LV_COLOR_PRIMARY_LIGHT, 0);
    lv_obj_set_style_pad_all(btn_weekly_review, 0, 0);
    lv_obj_t *lbl_weekly_review = lv_label_create(btn_weekly_review);
    lv_label_set_text(lbl_weekly_review, "Weekly Review");
    lv_obj_set_style_text_font(lbl_weekly_review, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_weekly_review, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_weekly_review);

    /* ---- START STATE ---- */
    lbl_greeting = lv_label_create(screen);
    lv_label_set_text(lbl_greeting, "");
    lv_obj_set_style_text_font(lbl_greeting, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_greeting, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lbl_greeting, LV_ALIGN_CENTER, 0, -90);
    lv_obj_add_flag(lbl_greeting, LV_OBJ_FLAG_HIDDEN);

    clock_frame = lv_obj_create(screen);
    lv_obj_set_size(clock_frame, 180, 180);
    lv_obj_align(clock_frame, LV_ALIGN_CENTER, 0, -30);
    lv_obj_set_style_bg_opa(clock_frame, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(clock_frame, 0, 0);
    lv_obj_set_style_shadow_width(clock_frame, 0, 0);
    lv_obj_clear_flag(clock_frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(clock_frame, LV_OBJ_FLAG_HIDDEN);

    lbl_clock = lv_label_create(screen);
    lv_label_set_text(lbl_clock, "00:00");
    lv_obj_set_style_text_font(lbl_clock, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_clock, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_clock, LV_ALIGN_CENTER, 0, -30);
    lv_obj_add_flag(lbl_clock, LV_OBJ_FLAG_HIDDEN);

    btn_start_now = lv_btn_create(screen);
    lv_obj_set_size(btn_start_now, 200, 56);
    lv_obj_align(btn_start_now, LV_ALIGN_CENTER, 0, 120);
    lv_obj_set_style_radius(btn_start_now, 28, 0);
    lv_obj_set_style_bg_color(btn_start_now, LV_COLOR_PRIMARY, 0);
    lv_obj_set_style_shadow_width(btn_start_now, 0, 0);
    lv_obj_set_style_shadow_opa(btn_start_now, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_start_now, 0, 0);
    lv_obj_set_style_border_color(btn_start_now, LV_COLOR_PRIMARY_LIGHT, 0);
    lv_obj_set_style_pad_all(btn_start_now, 0, 0);
    lv_obj_t *lbl_start_now = lv_label_create(btn_start_now);
    lv_label_set_text(lbl_start_now, "START SESSION");
    lv_obj_set_style_text_font(lbl_start_now, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_start_now, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_start_now);
    lv_obj_add_flag(btn_start_now, LV_OBJ_FLAG_HIDDEN);

    btn_back = lv_btn_create(screen);
    lv_obj_set_size(btn_back, 120, 40);
    lv_obj_align(btn_back, LV_ALIGN_CENTER, 0, 185);
    lv_obj_set_style_radius(btn_back, 20, 0);
    lv_obj_set_style_bg_color(btn_back, LV_COLOR_PRIMARY_DARK, 0);
    lv_obj_set_style_shadow_width(btn_back, 0, 0);
    lv_obj_set_style_shadow_opa(btn_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_back, 0, 0);
    lv_obj_set_style_border_color(btn_back, LV_COLOR_PRIMARY_LIGHT, 0);
    lv_obj_set_style_pad_all(btn_back, 0, 0);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_set_style_text_font(lbl_back, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_back, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_back);
    lv_obj_add_flag(btn_back, LV_OBJ_FLAG_HIDDEN);

    /* ---- ACTIVE STATE ---- */
    btn_awake = lv_btn_create(screen);
    lv_obj_set_size(btn_awake, 160, 50);
    lv_obj_align(btn_awake, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_radius(btn_awake, 25, 0);
    lv_obj_set_style_bg_color(btn_awake, LV_COLOR_SUCCESS, 0);
    lv_obj_set_style_shadow_width(btn_awake, 0, 0);
    lv_obj_set_style_shadow_opa(btn_awake, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_awake, 0, 0);
    lv_obj_set_style_border_color(btn_awake, LV_COLOR_SUCCESS, 0);
    lv_obj_set_style_pad_all(btn_awake, 0, 0);
    lbl_awake_text = lv_label_create(btn_awake);
    lv_label_set_text(lbl_awake_text, "AWAKE?");
    lv_obj_set_style_text_font(lbl_awake_text, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_awake_text, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_awake_text);
    lv_obj_add_flag(btn_awake, LV_OBJ_FLAG_HIDDEN);

    lbl_tracking = lv_label_create(screen);
    lv_label_set_text(lbl_tracking, "Tracking sleep...");
    lv_obj_set_style_text_font(lbl_tracking, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_tracking, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_tracking, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_add_flag(lbl_tracking, LV_OBJ_FLAG_HIDDEN);

    lbl_awake_hint = lv_label_create(screen);
    lv_label_set_text(lbl_awake_hint, "Press when you wake up");
    lv_obj_set_style_text_font(lbl_awake_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_awake_hint, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_awake_hint, LV_ALIGN_TOP_MID, 0, 85);
    lv_obj_add_flag(lbl_awake_hint, LV_OBJ_FLAG_HIDDEN);

    badge_pulse = lv_obj_create(screen);
    lv_obj_set_size(badge_pulse, 200, 200);
    lv_obj_align(badge_pulse, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(badge_pulse, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(badge_pulse, LV_COLOR_PRIMARY, 0);
    lv_obj_set_style_bg_opa(badge_pulse, LV_OPA_50, 0);
    lv_obj_set_style_border_width(badge_pulse, 0, 0);
    lv_obj_set_style_shadow_width(badge_pulse, 0, 0);
    lv_obj_set_style_pad_all(badge_pulse, 0, 0);
    lv_obj_clear_flag(badge_pulse, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(badge_pulse, LV_OBJ_FLAG_HIDDEN);

    lbl_goodnight = lv_label_create(badge_pulse);
    lv_label_set_text(lbl_goodnight, "Goodnight!");
    lv_obj_set_style_text_font(lbl_goodnight, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_goodnight, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_goodnight);

    lbl_active_clock = lv_label_create(screen);
    lv_label_set_text(lbl_active_clock, "00:00");
    lv_obj_set_style_text_font(lbl_active_clock, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_active_clock, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lbl_active_clock, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_add_flag(lbl_active_clock, LV_OBJ_FLAG_HIDDEN);

    btn_nevermind = lv_btn_create(screen);
    lv_obj_set_size(btn_nevermind, 140, 40);
    lv_obj_align(btn_nevermind, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_set_style_radius(btn_nevermind, 20, 0);
    lv_obj_set_style_bg_color(btn_nevermind, LV_COLOR_PRIMARY_DARK, 0);
    lv_obj_set_style_shadow_width(btn_nevermind, 0, 0);
    lv_obj_set_style_shadow_opa(btn_nevermind, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_nevermind, 0, 0);
    lv_obj_set_style_border_color(btn_nevermind, LV_COLOR_PRIMARY_LIGHT, 0);
    lv_obj_set_style_pad_all(btn_nevermind, 0, 0);
    lv_obj_t *lbl_nevermind = lv_label_create(btn_nevermind);
    lv_label_set_text(lbl_nevermind, "Nevermind");
    lv_obj_set_style_text_font(lbl_nevermind, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_nevermind, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_nevermind);
    lv_obj_add_flag(btn_nevermind, LV_OBJ_FLAG_HIDDEN);

    /* ---- SUMMARY STATE ---- */
    lbl_result = lv_label_create(screen);
    lv_label_set_text(lbl_result, "");
    lv_obj_set_style_text_font(lbl_result, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_result, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_result, LV_ALIGN_CENTER, 0, -30);
    lv_obj_add_flag(lbl_result, LV_OBJ_FLAG_HIDDEN);

    lbl_sub = lv_label_create(screen);
    lv_label_set_text(lbl_sub, "");
    lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_sub, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lbl_sub, LV_ALIGN_CENTER, 0, 15);
    lv_obj_add_flag(lbl_sub, LV_OBJ_FLAG_HIDDEN);

    lbl_hint = lv_label_create(screen);
    lv_label_set_text(lbl_hint, "Press to continue...");
    lv_obj_set_style_text_font(lbl_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_hint, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_hint, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_add_flag(lbl_hint, LV_OBJ_FLAG_HIDDEN);

    /* ---- WEEKLY STATE ---- */
    lbl_weekly_header = lv_label_create(screen);
    lv_label_set_text(lbl_weekly_header, "Your weekly average is");
    lv_obj_set_style_text_font(lbl_weekly_header, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_weekly_header, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lbl_weekly_header, LV_ALIGN_TOP_MID, 0, 35);
    lv_obj_add_flag(lbl_weekly_header, LV_OBJ_FLAG_HIDDEN);

    lbl_weekly_avg = lv_label_create(screen);
    lv_label_set_text(lbl_weekly_avg, "0.0 hrs");
    lv_obj_set_style_text_font(lbl_weekly_avg, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(lbl_weekly_avg, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_weekly_avg, LV_ALIGN_TOP_MID, 0, 65);
    lv_obj_add_flag(lbl_weekly_avg, LV_OBJ_FLAG_HIDDEN);

    lbl_weekly_msg = lv_label_create(screen);
    lv_label_set_text(lbl_weekly_msg, "");
    lv_obj_set_style_text_font(lbl_weekly_msg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_weekly_msg, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lbl_weekly_msg, LV_ALIGN_TOP_MID, 0, 110);
    lv_obj_add_flag(lbl_weekly_msg, LV_OBJ_FLAG_HIDDEN);

    chart_weekly = lv_chart_create(screen);
    lv_obj_set_size(chart_weekly, 280, 120);
    lv_obj_align(chart_weekly, LV_ALIGN_CENTER, 0, 30);
    lv_chart_set_type(chart_weekly, LV_CHART_TYPE_BAR);
    lv_chart_set_point_count(chart_weekly, 7);
    lv_chart_set_range(chart_weekly, LV_CHART_AXIS_PRIMARY_Y, 0, 720);
    chart_series = lv_chart_add_series(chart_weekly,
        LV_COLOR_PRIMARY, LV_CHART_AXIS_PRIMARY_Y);
    if (chart_series) {
        lv_coord_t zeros[7] = {0, 0, 0, 0, 0, 0, 0};
        lv_chart_set_ext_y_array(chart_weekly, chart_series, zeros);
    }
    lv_obj_add_flag(chart_weekly, LV_OBJ_FLAG_HIDDEN);

    btn_weekly_back = lv_btn_create(screen);
    lv_obj_set_size(btn_weekly_back, 110, 40);
    lv_obj_align(btn_weekly_back, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_radius(btn_weekly_back, 20, 0);
    lv_obj_set_style_bg_color(btn_weekly_back, LV_COLOR_PRIMARY_DARK, 0);
    lv_obj_set_style_shadow_width(btn_weekly_back, 0, 0);
    lv_obj_set_style_shadow_opa(btn_weekly_back, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_weekly_back, 0, 0);
    lv_obj_set_style_border_color(btn_weekly_back, LV_COLOR_PRIMARY_LIGHT, 0);
    lv_obj_set_style_pad_all(btn_weekly_back, 0, 0);
    lv_obj_t *lbl_weekly_back = lv_label_create(btn_weekly_back);
    lv_label_set_text(lbl_weekly_back, "Back");
    lv_obj_set_style_text_font(lbl_weekly_back, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_weekly_back, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_weekly_back);
    lv_obj_add_flag(btn_weekly_back, LV_OBJ_FLAG_HIDDEN);

    /* ---- Init clock timer ---- */
    update_time_cb(NULL);
    clock_timer = lv_timer_create(update_time_cb, 1000, NULL);

    transition_to_intro();

    ESP_LOGI(TAG, "Sleep screen created");
    return screen;
}

/* ============================================================
 *  ENCODER EVENT HANDLER
 * ============================================================ */
void screen_sleep_encoder_event(lv_indev_data_t *data)
{
    switch (current_state) {
    case SLEEP_STATE_INTRO:
        if (data->enc_diff != 0) {
            focus_index = (focus_index == 0) ? 1 : 0;
            update_focus_styles();
        }
        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            if (focus_index == 0) {
                transition_to_start();
            } else {
                transition_to_weekly();
            }
        }
        break;

    case SLEEP_STATE_START:
        if (data->enc_diff != 0) {
            focus_index = (focus_index == 0) ? 1 : 0;
            update_focus_styles();
        }
        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            if (focus_index == 0) {
                sleep_store_start_session();
                transition_to_active();
            } else {
                transition_to_intro();
            }
        }
        break;

    case SLEEP_STATE_ACTIVE:
        if (!confirm_mode) {
            if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
                enter_confirm_mode();
            }
        } else {
            if (data->enc_diff != 0) {
                focus_index = (focus_index == 0) ? 1 : 0;
                update_focus_styles();
            }
            if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
                if (focus_index == 0) {
                    transition_to_summary();
                } else {
                    exit_confirm_mode();
                }
            }
        }
        break;

    case SLEEP_STATE_SUMMARY:
        if (data->enc_diff != 0 ||
            (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0)) {
            transition_to_start();
        }
        break;

    case SLEEP_STATE_WEEKLY:
        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            transition_to_intro();
        }
        break;
    }
}
