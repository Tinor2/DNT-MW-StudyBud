#include "screen_breathing.h"
#include "ui_manager.h"
#include "studybud_theme.h"
#include "../utils/session_store.h"
#include "networking/app_state.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdio.h>

static const char *TAG = "Screen_Breathing";

#define CYCLE_TIME_MS    3500
#define MOTION_PCT       60
#define MOTION_MS        (CYCLE_TIME_MS * MOTION_PCT / 100)
#define HOLD_MS          (CYCLE_TIME_MS - MOTION_MS)
#define TRANSITION_TIME  300
#define FOCUS_ANIM_MS    250
#define TAP_THRESHOLD_MS 200

#define BADGE_BASE_SIZE  100
#define BADGE_MAX_SIZE   280
#define BADGE_EXPAND     ((BADGE_MAX_SIZE - BADGE_BASE_SIZE) / 2)

#define MIN_CYCLES 1
#define MAX_CYCLES 40

typedef enum {
    STATE_SELECTION,
    STATE_INSTRUCTION,
    STATE_ACTIVE,
    STATE_COMPLETE
} breathing_state_t;

static breathing_state_t current_state = STATE_SELECTION;
static lv_obj_t *screen;

/* --- State A: Selection --- */
static lv_obj_t *lbl_title;
static lv_obj_t *btn_begin;
static lv_obj_t *btn_cycles;
static lv_obj_t *lbl_cycles_text;

/* --- State B: Instruction --- */
static lv_obj_t *lbl_inst_count;
static lv_obj_t *lbl_inst_moment;
static lv_obj_t *lbl_inst_follow;
static lv_obj_t *lbl_inst_press;

/* --- State C: Active --- */
static lv_obj_t *badge;
static lv_obj_t *lbl_badge_text;
static lv_obj_t *lbl_counter;

/* --- State D: Completion --- */
static lv_obj_t *lbl_result;
static lv_obj_t *btn_home;
static lv_obj_t *btn_restart;

/* --- State --- */
static int cycle_limit = 3;
static int cycle_count = 0;
static bool cycles_edit_mode = false;
static int focus_index = 0;
static int prev_focus_index = -1;
static bool current_inhale = true;

/* --- Input tracking --- */
static uint32_t pr_tick = 0;
static bool pr_active = false;

/* ============================================================
 *  FORWARD DECLARATIONS
 * ============================================================ */
static void update_focus_styles(void);
static void animate_style(lv_obj_t *obj, lv_anim_exec_xcb_t exec_cb,
                          int32_t from, int32_t to, uint32_t time, uint32_t delay);
static void transition_to_instruction(void);
static void transition_to_active(void);
static void transition_to_complete(void);
static void reset_to_selection(void);
static void start_breath_phase(bool inhale);
static void update_cycles_text(void);

/* ============================================================
 *  ANIMATION EXEC CALLBACKS
 * ============================================================ */
static void anim_set_width(void *var, int32_t val)
{
    lv_obj_set_style_transform_width((lv_obj_t *)var, val, 0);
}

static void anim_set_height(void *var, int32_t val)
{
    lv_obj_set_style_transform_height((lv_obj_t *)var, val, 0);
}

static void anim_set_opa(void *var, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)val, 0);
}

static void anim_set_shadow(void *var, int32_t val)
{
    lv_obj_set_style_shadow_width((lv_obj_t *)var, val, 0);
}

static void anim_set_shadow_opa(void *var, int32_t val)
{
    lv_obj_set_style_shadow_opa((lv_obj_t *)var, (lv_opa_t)val, 0);
}

static void anim_set_border_width(void *var, int32_t val)
{
    lv_obj_set_style_border_width((lv_obj_t *)var, val, 0);
}

static void anim_set_zoom(void *var, int32_t val)
{
    lv_obj_set_style_transform_zoom((lv_obj_t *)var, val, 0);
}

static void anim_set_color_blend(void *var, int32_t val)
{
    lv_color_t from = LV_COLOR_BREATHING;
    lv_color_t to   = LV_COLOR_INFO;
    lv_color_t c = lv_color_mix(to, from, (uint8_t)val);
    lv_obj_set_style_bg_color((lv_obj_t *)var, c, 0);
}

/* ============================================================
 *  CYCLES TEXT HELPER
 * ============================================================ */
static void update_cycles_text(void)
{
    char buf[24];
    snprintf(buf, sizeof(buf), "Cycles   %d", cycle_limit);
    lv_label_set_text(lbl_cycles_text, buf);
}

/* ============================================================
 *  BREATH PHASE LOGIC
 * ============================================================ */
static void breath_hold_cb(lv_timer_t *timer)
{
    (void)timer;
    if (current_state != STATE_ACTIVE) return;

    /* After OUT hold → cycle complete → increment counter */
    if (!current_inhale) {
        cycle_count++;
        lv_label_set_text_fmt(lbl_counter, "%d", cycle_count);
        if (cycle_count >= cycle_limit) {
            transition_to_complete();
            return;
        }
    }

    start_breath_phase(!current_inhale);
}

static void breath_motion_ready_cb(lv_anim_t *a)
{
    (void)a;
    if (current_state != STATE_ACTIVE) return;

    lv_timer_t *t = lv_timer_create(breath_hold_cb, HOLD_MS, NULL);
    t->repeat_count = 1;
}

static void start_breath_phase(bool inhale)
{
    if (current_state != STATE_ACTIVE) return;

    current_inhale = inhale;
    lv_anim_del(badge, NULL);
    lv_label_set_text(lbl_badge_text, inhale ? "IN" : "OUT");

    int32_t from_w = inhale ? 0 : BADGE_EXPAND;
    int32_t to_w   = inhale ? BADGE_EXPAND : 0;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, badge);
    lv_anim_set_exec_cb(&a, anim_set_width);
    lv_anim_set_values(&a, from_w, to_w);
    lv_anim_set_time(&a, MOTION_MS);
    lv_anim_set_path_cb(&a, inhale ? lv_anim_path_ease_out : lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&a, breath_motion_ready_cb);
    lv_anim_start(&a);

    lv_anim_t a2;
    lv_anim_init(&a2);
    lv_anim_set_var(&a2, badge);
    lv_anim_set_exec_cb(&a2, anim_set_height);
    lv_anim_set_values(&a2, from_w, to_w);
    lv_anim_set_time(&a2, MOTION_MS);
    lv_anim_set_path_cb(&a2, inhale ? lv_anim_path_ease_out : lv_anim_path_ease_in);
    lv_anim_start(&a2);

    lv_anim_t ac;
    lv_anim_init(&ac);
    lv_anim_set_var(&ac, badge);
    lv_anim_set_exec_cb(&ac, anim_set_color_blend);
    lv_anim_set_values(&ac, inhale ? 0 : 255, inhale ? 255 : 0);
    lv_anim_set_time(&ac, MOTION_MS);
    lv_anim_set_path_cb(&ac, lv_anim_path_ease_in_out);
    lv_anim_start(&ac);
}

/* ============================================================
 *  STATE TRANSITIONS
 * ============================================================ */
static void transition_to_instruction(void)
{
    current_state = STATE_INSTRUCTION;
    session_store_increment_breath_count();

    int nth = session_store_get_breath_count();
    lv_label_set_text_fmt(lbl_inst_count, "It's your %d%s time today!", nth,
                          nth == 1 ? "st" : nth == 2 ? "nd" : nth == 3 ? "rd" : "th");

    lv_obj_add_flag(lbl_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_begin, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_cycles, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(lbl_inst_count, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_inst_moment, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_inst_follow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_inst_press, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(lbl_inst_count, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(lbl_inst_moment, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(lbl_inst_follow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_opa(lbl_inst_press, LV_OPA_TRANSP, 0);

    animate_style(lbl_inst_count, (lv_anim_exec_xcb_t)anim_set_opa,
                  LV_OPA_TRANSP, LV_OPA_COVER, TRANSITION_TIME, 0);
    animate_style(lbl_inst_moment, (lv_anim_exec_xcb_t)anim_set_opa,
                  LV_OPA_TRANSP, LV_OPA_COVER, TRANSITION_TIME, 100);
    animate_style(lbl_inst_follow, (lv_anim_exec_xcb_t)anim_set_opa,
                  LV_OPA_TRANSP, LV_OPA_COVER, TRANSITION_TIME, 200);
    animate_style(lbl_inst_press, (lv_anim_exec_xcb_t)anim_set_opa,
                  LV_OPA_TRANSP, LV_OPA_COVER, TRANSITION_TIME, 300);

    ESP_LOGI(TAG, "Transitioned to Instruction state");
}

static void transition_to_active(void)
{
    current_state = STATE_ACTIVE;
    cycle_count = 0;
    app_state_get()->breathing_active = true;

    lv_obj_add_flag(lbl_inst_count, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_inst_moment, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_inst_follow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_inst_press, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(badge, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_badge_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_counter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(badge, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(lbl_badge_text, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(lbl_counter, LV_OPA_40, 0);

    lv_label_set_text_fmt(lbl_counter, "%d", cycle_count);
    lv_obj_set_style_transform_width(badge, 0, 0);
    lv_obj_set_style_transform_height(badge, 0, 0);
    lv_obj_set_style_bg_color(badge, LV_COLOR_BREATHING, 0);

    start_breath_phase(true);
    app_state_broadcast_screen_change(SCREEN_BREATHING);
    ESP_LOGI(TAG, "Transitioned to Active state");
}

static void transition_to_complete(void)
{
    current_state = STATE_COMPLETE;
    app_state_get()->breathing_active = false;

    lv_anim_del(badge, NULL);
    lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_badge_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_counter, LV_OBJ_FLAG_HIDDEN);

    lv_label_set_text_fmt(lbl_result, "You completed %d cycles!", cycle_count);

    lv_obj_clear_flag(lbl_result, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_home, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(lbl_result, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(btn_home, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(btn_restart, LV_OPA_COVER, 0);

    focus_index = 0;
    prev_focus_index = -1;
    update_focus_styles();

    cycles_edit_mode = false;
    ESP_LOGI(TAG, "Transitioned to Complete state");
}

static void reset_to_selection(void)
{
    lv_anim_del(badge, NULL);
    lv_anim_del(NULL, NULL);

    lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_badge_text, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_counter, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_inst_count, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_inst_moment, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_inst_follow, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_inst_press, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_result, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_home, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(lbl_title, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_begin, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(btn_cycles, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(lbl_title, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(btn_begin, LV_OPA_COVER, 0);
    lv_obj_set_style_opa(btn_cycles, LV_OPA_COVER, 0);

    lv_obj_set_style_transform_width(btn_begin, 0, 0);
    lv_obj_set_style_transform_height(btn_begin, 0, 0);
    lv_obj_set_style_border_width(btn_begin, 0, 0);
    lv_obj_set_style_transform_width(btn_cycles, 0, 0);
    lv_obj_set_style_transform_height(btn_cycles, 0, 0);
    lv_obj_set_style_border_width(btn_cycles, 0, 0);

    focus_index = 0;
    prev_focus_index = -1;
    cycle_count = 0;
    cycle_limit = 3;
    cycles_edit_mode = false;
    current_state = STATE_SELECTION;

    update_cycles_text();
    update_focus_styles();
    ESP_LOGI(TAG, "Reset to Selection state");
}

/* ============================================================
 *  FOCUS STYLES — consistent outline on both buttons
 * ============================================================ */
static void update_focus_styles(void)
{
    if (current_state == STATE_SELECTION) {
        if (focus_index != prev_focus_index) {
            lv_obj_t *focused = (focus_index == 0) ? btn_begin : btn_cycles;
            lv_obj_t *defocused = (focus_index == 0) ? btn_cycles : btn_begin;

            animate_style(focused, (lv_anim_exec_xcb_t)anim_set_border_width,
                          0, 3, FOCUS_ANIM_MS, 0);

            animate_style(defocused, (lv_anim_exec_xcb_t)anim_set_border_width,
                          3, 0, FOCUS_ANIM_MS, 0);

            prev_focus_index = focus_index;
        }

        lv_obj_set_style_border_color(btn_begin, LV_COLOR_PRIMARY_LIGHT, 0);
        lv_obj_set_style_border_color(btn_cycles, LV_COLOR_PRIMARY_LIGHT, 0);

        if (cycles_edit_mode) {
            lv_obj_set_style_border_color(btn_cycles, LV_COLOR_PRIMARY_LIGHT, 0);
            lv_obj_set_style_border_width(btn_cycles, 3, 0);
        }
    } else if (current_state == STATE_COMPLETE) {
        if (focus_index != prev_focus_index) {
            lv_obj_t *focused = (focus_index == 0) ? btn_home : btn_restart;
            lv_obj_t *defocused = (focus_index == 0) ? btn_restart : btn_home;

            lv_obj_set_style_bg_color(focused, LV_COLOR_PRIMARY_LIGHT, 0);
            lv_obj_set_style_bg_color(defocused, LV_COLOR_PRIMARY_DARK, 0);

            animate_style(focused, (lv_anim_exec_xcb_t)anim_set_border_width,
                          0, 3, FOCUS_ANIM_MS, 0);
            animate_style(defocused, (lv_anim_exec_xcb_t)anim_set_border_width,
                          3, 0, FOCUS_ANIM_MS, 0);

            lv_obj_set_style_border_color(focused, LV_COLOR_PRIMARY_LIGHT, 0);
            lv_obj_set_style_border_color(defocused, LV_COLOR_PRIMARY_DARK, 0);

            prev_focus_index = focus_index;
        }
    }
}

/* ============================================================
 *  HELPER: animate a style property on an object
 * ============================================================ */
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
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

/* ============================================================
 *  SCREEN CREATE
 * ============================================================ */
lv_obj_t *screen_breathing_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, LV_COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- State A: Selection ---- */
    lbl_title = lv_label_create(screen);
    lv_label_set_text(lbl_title, "Breathing");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 20);

    /* Large centered BEGIN button — 280×280 */
    btn_begin = lv_btn_create(screen);
    lv_obj_set_size(btn_begin, 280, 280);
    lv_obj_align(btn_begin, LV_ALIGN_CENTER, 0, -35);
    lv_obj_set_style_radius(btn_begin, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(btn_begin, LV_COLOR_PRIMARY, 0);
    lv_obj_set_style_shadow_width(btn_begin, 0, 0);
    lv_obj_set_style_shadow_opa(btn_begin, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_begin, 0, 0);
    lv_obj_set_style_border_color(btn_begin, LV_COLOR_PRIMARY_LIGHT, 0);
    lv_obj_set_style_pad_all(btn_begin, 0, 0);
    lv_obj_t *lbl_begin = lv_label_create(btn_begin);
    lv_label_set_text(lbl_begin, "BEGIN");
    lv_obj_set_style_text_font(lbl_begin, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(lbl_begin, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_begin);

    /* Cycles capsule button — 150×50, text inside */
    btn_cycles = lv_btn_create(screen);
    lv_obj_set_size(btn_cycles, 150, 50);
    lv_obj_align(btn_cycles, LV_ALIGN_CENTER, 0, 165);
    lv_obj_set_style_radius(btn_cycles, 25, 0);
    lv_obj_set_style_bg_color(btn_cycles, LV_COLOR_PRIMARY_DARK, 0);
    lv_obj_set_style_shadow_width(btn_cycles, 0, 0);
    lv_obj_set_style_shadow_opa(btn_cycles, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_cycles, 0, 0);
    lv_obj_set_style_border_color(btn_cycles, LV_COLOR_PRIMARY_LIGHT, 0);
    lv_obj_set_style_pad_all(btn_cycles, 0, 0);

    lbl_cycles_text = lv_label_create(btn_cycles);
    lv_label_set_text(lbl_cycles_text, "Cycles   3");
    lv_obj_set_style_text_font(lbl_cycles_text, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_cycles_text, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_cycles_text);

    /* ---- State B: Instruction ---- */
    lbl_inst_count = lv_label_create(screen);
    lv_label_set_text(lbl_inst_count, "");
    lv_obj_set_style_text_font(lbl_inst_count, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_inst_count, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_inst_count, LV_ALIGN_CENTER, 0, -60);
    lv_obj_add_flag(lbl_inst_count, LV_OBJ_FLAG_HIDDEN);

    lbl_inst_moment = lv_label_create(screen);
    lv_label_set_text(lbl_inst_moment, "Take a moment for yourself.");
    lv_obj_set_style_text_font(lbl_inst_moment, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_inst_moment, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lbl_inst_moment, LV_ALIGN_CENTER, 0, -10);
    lv_obj_add_flag(lbl_inst_moment, LV_OBJ_FLAG_HIDDEN);

    lbl_inst_follow = lv_label_create(screen);
    lv_label_set_text(lbl_inst_follow, "Follow the onscreen prompt.");
    lv_obj_set_style_text_font(lbl_inst_follow, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_inst_follow, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_inst_follow, LV_ALIGN_CENTER, 0, 30);
    lv_obj_add_flag(lbl_inst_follow, LV_OBJ_FLAG_HIDDEN);

    lbl_inst_press = lv_label_create(screen);
    lv_label_set_text(lbl_inst_press, "Press encoder dial to begin...");
    lv_obj_set_style_text_font(lbl_inst_press, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_inst_press, LV_COLOR_PRIMARY, 0);
    lv_obj_align(lbl_inst_press, LV_ALIGN_CENTER, 0, 70);
    lv_obj_add_flag(lbl_inst_press, LV_OBJ_FLAG_HIDDEN);

    /* ---- State C: Active ---- */
    /* Counter number — large faded watermark BEHIND the badge */
    lbl_counter = lv_label_create(screen);
    lv_label_set_text(lbl_counter, "0");
    lv_obj_set_style_text_font(lbl_counter, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(lbl_counter, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_opa(lbl_counter, LV_OPA_60, 0);
    lv_obj_set_style_transform_zoom(lbl_counter, 640, 0);
    lv_obj_align(lbl_counter, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(lbl_counter, LV_OBJ_FLAG_HIDDEN);

    /* Badge circle — expands from 100 to 280, semi-transparent so counter shows through */
    badge = lv_obj_create(screen);
    lv_obj_set_size(badge, BADGE_BASE_SIZE, BADGE_BASE_SIZE);
    lv_obj_align(badge, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(badge, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(badge, LV_COLOR_BREATHING, 0);
    lv_obj_set_style_bg_opa(badge, 200, 0);
    lv_obj_set_style_border_width(badge, 0, 0);
    lv_obj_set_style_shadow_width(badge, 0, 0);
    lv_obj_set_style_pad_all(badge, 0, 0);
    lv_obj_clear_flag(badge, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(badge, LV_OBJ_FLAG_HIDDEN);

    lbl_badge_text = lv_label_create(badge);
    lv_label_set_text(lbl_badge_text, "IN");
    lv_obj_set_style_text_font(lbl_badge_text, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_badge_text, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_badge_text);
    lv_obj_add_flag(lbl_badge_text, LV_OBJ_FLAG_HIDDEN);

    /* ---- State D: Completion ---- */
    lbl_result = lv_label_create(screen);
    lv_label_set_text(lbl_result, "");
    lv_obj_set_style_text_font(lbl_result, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_result, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_result, LV_ALIGN_CENTER, 0, -50);
    lv_obj_add_flag(lbl_result, LV_OBJ_FLAG_HIDDEN);

    btn_home = lv_btn_create(screen);
    lv_obj_set_size(btn_home, 160, 40);
    lv_obj_align(btn_home, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_style_radius(btn_home, 12, 0);
    lv_obj_set_style_bg_color(btn_home, LV_COLOR_PRIMARY, 0);
    lv_obj_set_style_shadow_width(btn_home, 0, 0);
    lv_obj_set_style_border_width(btn_home, 0, 0);
    lv_obj_set_style_border_color(btn_home, LV_COLOR_PRIMARY_LIGHT, 0);
    lv_obj_t *lbl_home = lv_label_create(btn_home);
    lv_label_set_text(lbl_home, "Home");
    lv_obj_set_style_text_font(lbl_home, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_home, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_home);
    lv_obj_add_flag(btn_home, LV_OBJ_FLAG_HIDDEN);

    btn_restart = lv_btn_create(screen);
    lv_obj_set_size(btn_restart, 160, 40);
    lv_obj_align(btn_restart, LV_ALIGN_CENTER, 0, 60);
    lv_obj_set_style_radius(btn_restart, 12, 0);
    lv_obj_set_style_bg_color(btn_restart, LV_COLOR_PRIMARY_DARK, 0);
    lv_obj_set_style_shadow_width(btn_restart, 0, 0);
    lv_obj_set_style_border_width(btn_restart, 0, 0);
    lv_obj_set_style_border_color(btn_restart, LV_COLOR_PRIMARY_LIGHT, 0);
    lv_obj_t *lbl_restart = lv_label_create(btn_restart);
    lv_label_set_text(lbl_restart, "Restart Exercise");
    lv_obj_set_style_text_font(lbl_restart, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_restart, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_restart);
    lv_obj_add_flag(btn_restart, LV_OBJ_FLAG_HIDDEN);

    focus_index = 0;
    prev_focus_index = -1;
    update_focus_styles();

    ESP_LOGI(TAG, "Breathing screen created");
    return screen;
}

/* ============================================================
 *  ENCODER EVENT HANDLER
 * ============================================================ */
void screen_breathing_encoder_event(lv_indev_data_t *data)
{
    /* --- State C: Active — detect short tap to abort --- */
    if (current_state == STATE_ACTIVE) {
        if (data->state == LV_INDEV_STATE_REL) {
            if (pr_active && pr_tick != 0) {
                uint32_t held = lv_tick_elaps(pr_tick);
                if (held < TAP_THRESHOLD_MS) {
                    transition_to_complete();
                }
            }
            pr_active = false;
            pr_tick = 0;
        } else if (data->state == LV_INDEV_STATE_PR) {
            if (!pr_active) {
                pr_active = true;
                pr_tick = lv_tick_get();
            }
        }
        return;
    }

    /* --- State A: Selection --- */
    if (current_state == STATE_SELECTION) {
        if (data->enc_diff != 0 && !cycles_edit_mode) {
            focus_index = (focus_index == 0) ? 1 : 0;
            update_focus_styles();
        }

        if (data->enc_diff != 0 && cycles_edit_mode) {
            cycle_limit += data->enc_diff;
            if (cycle_limit < MIN_CYCLES) cycle_limit = MAX_CYCLES;
            if (cycle_limit > MAX_CYCLES) cycle_limit = MIN_CYCLES;
            update_cycles_text();
        }

        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            if (focus_index == 0) {
                transition_to_instruction();
            } else {
                cycles_edit_mode = !cycles_edit_mode;
                update_focus_styles();
            }
        }
        return;
    }

    /* --- State B: Instruction — any press starts breathing --- */
    if (current_state == STATE_INSTRUCTION) {
        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            transition_to_active();
        }
        return;
    }

    /* --- State D: Completion --- */
    if (current_state == STATE_COMPLETE) {
        if (data->enc_diff != 0) {
            focus_index = (focus_index == 0) ? 1 : 0;
            update_focus_styles();
        }

        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            if (focus_index == 0) {
                ui_manager_switch_screen(SCREEN_HOME);
            } else {
                reset_to_selection();
            }
        }
        return;
    }
}
