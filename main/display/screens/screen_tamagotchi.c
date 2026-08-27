#include "screen_tamagotchi.h"
#include "ui_manager.h"
#include "studybud_theme.h"
#include "color_palette.h"
#include "../utils/points_store.h"
#include "../utils/persistence.h"
#include "../app_state.h"
#include "plant_1_sprite.h"
#include "plant_2_sprite.h"
#include "plant_3_sprite.h"
#include "tamagotchi_backdrop.h"
#include "app_logo.h"
#include "timer_logo.h"
#include "breathing_logo.h"
#include "sleeping_logo.h"
#include "todo_logo.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

static const char *TAG = "Screen_Tamagotchi";

void sdl_driver_screenshot(const char *path);

#define FOCUS_ANIM_MS    300

/* Hard-coded green base for the Tamagotchi (RGB 75, 188, 51).
   Every other colour on this screen is derived from this base. */
#define ACCENT       LV_COLOR_TAMAGOTCHI
#define ACCENT_LIGHT lighten_color(ACCENT, 0.40f)
#define ACCENT_DARK  darken_color(ACCENT, 0.30f)

#define TITLE_TEXT   darken_text_color(ACCENT, 0.5f)

/* Radial list constants (mirror screen_menu / screen_todos) */
#define DISPLAY_R     240
#define DISPLAY_CY    240
#define MAX_ROW_W     420
#define MIN_VISIBLE_W 120
#define FADE_ZONE     60

/* Opacity tweening is relative to the visible goals area (~300px below the
 * summary card), not the full 480px screen.  The summary card ends at y≈182
 * and the screen bottom is y=480, giving a visible band of ≈298px centred
 * at y≈331 with a half-height of ≈149. */
#define OPA_CY        331
#define OPA_R         149
#define OPA_FADE      37

#define GOALS_TOP       220
#define GOALS_SPACING   64
#define GOALS_COUNT     (MAX_DAILY_GOALS + 2)
/* Height of the invisible spacer below the GOALS list. It makes the content
 * taller than the 480px screen so the focused row can scroll to center (with
 * the list alone the content is ~300px, leaving the focused row in the fade
 * zone at opa<255 and rendering its white card muddy). */
#define GOALS_PAD_BOTTOM 450

#define STREAKS_TOP       190
#define STREAKS_SPACING   90
#define STREAK_ROW_H      68
#define STREAKS_ROWS      (STREAK_COUNT + 1)

typedef enum {
    TAMA_STATE_PET,
    TAMA_STATE_GOALS,
    TAMA_STATE_STREAKS
} tama_state_t;

static tama_state_t current_state = TAMA_STATE_PET;
static lv_obj_t *screen = NULL;

static int focus_index = 0;
static int prev_focus_index = -1;
static lv_obj_t *focus_list[8];
static int focus_count = 0;
static lv_obj_t *focused_row = NULL;
static lv_timer_t *radial_timer = NULL;

/* --- State: PET --- */
static lv_obj_t *lbl_title = NULL;
static lv_obj_t *chip_level = NULL;
static lv_obj_t *lbl_chip_level = NULL;
static lv_obj_t *bar_level = NULL;
static lv_obj_t *pet_stats = NULL;
static lv_obj_t *pill_today = NULL;
static lv_obj_t *lbl_pill_today = NULL;
static lv_obj_t *pill_streak = NULL;
static lv_obj_t *lbl_pill_streak = NULL;
static lv_obj_t *bg_img = NULL;
static lv_obj_t *bg_overlay = NULL;
static lv_obj_t *plant_img = NULL;
static lv_obj_t *lbl_pet_hint = NULL;
static lv_obj_t *btn_goals = NULL;

/* --- State: GOALS --- */
static lv_obj_t *goals_container = NULL;
static lv_obj_t *summary_card = NULL;
static lv_obj_t *goals_header = NULL;
static lv_obj_t *lbl_seeds_num = NULL;
static lv_obj_t *lbl_seeds_word = NULL;
static lv_obj_t *lbl_summary_meta = NULL;
static lv_obj_t *lbl_prog_left = NULL;
static lv_obj_t *lbl_prog_right = NULL;
static lv_obj_t *bar_progress = NULL;
static lv_obj_t *lbl_section = NULL;
static lv_obj_t *lbl_section_count = NULL;
static lv_obj_t *goal_rows[MAX_DAILY_GOALS] = {NULL};
static lv_obj_t *goal_check[MAX_DAILY_GOALS] = {NULL};
static lv_obj_t *goal_text[MAX_DAILY_GOALS] = {NULL};
static lv_obj_t *goals_nav_streaks = NULL;
static lv_obj_t *goals_nav_back = NULL;
static lv_obj_t *goals_arrow_up = NULL;
static lv_obj_t *goals_arrow_down = NULL;
static lv_obj_t *goals_scroll_rows[GOALS_COUNT] = {NULL};

/* --- State: STREAKS --- */
static lv_obj_t *streaks_container = NULL;
static lv_obj_t *streak_rows[STREAK_COUNT] = {NULL};
static lv_obj_t *streak_icon[STREAK_COUNT] = {NULL};
static bool      streak_icon_is_img[STREAK_COUNT] = {false};
static lv_obj_t *streak_icon_sym[STREAK_COUNT] = {NULL};
static lv_obj_t *streak_name[STREAK_COUNT] = {NULL};
static lv_obj_t *streak_days[STREAK_COUNT] = {NULL};
static lv_obj_t *streak_bar[STREAK_COUNT] = {NULL};
static lv_obj_t *streak_mult[STREAK_COUNT] = {NULL};
static lv_obj_t *streaks_back_row = NULL;
static lv_obj_t *streaks_arrow_up = NULL;
static lv_obj_t *streaks_arrow_down = NULL;
static lv_obj_t *streaks_scroll_rows[STREAKS_ROWS] = {NULL};

/* Per-app logo artwork (order matches streak_activity_t).
 * NULL entries fall back to an LVGL symbol. */
static const lv_img_dsc_t *streak_logos[STREAK_COUNT] = {
    &timer_logo,      /* FOCUS     */
    &app_logo,        /* WATER     */
    &breathing_logo,  /* BREATHING */
    &todo_logo,       /* GOALS     */
    &sleeping_logo,   /* SLEEP     */
    NULL              /* MOVE      */
};

static const char *streak_names[STREAK_COUNT] = {
    "Focus", "Water", "Breathing", "Daily goals", "Sleep", "Move"
};

static void update_focus_styles(void);
static void apply_radial_scroll(void);
static void scroll_to_focused(bool animate);
static void transition_to_pet(void);
static void transition_to_goals(void);
static void transition_to_streaks(void);
static void set_focus_list(lv_obj_t **list, int count);

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

static void format_seeds(char *buf, size_t len, int n)
{
    if (n < 0) n = 0;
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", n);
    size_t tlen = strlen(tmp);

    int commas = (int)(tlen > 3 ? (tlen - 1) / 3 : 0);
    if ((size_t)commas >= len) {
        snprintf(buf, len, "%d", n);
        return;
    }

    int out = 0;
    for (size_t i = 0; i < tlen; i++) {
        if (out >= (int)len - 1) break;
        if (i > 0 && ((tlen - i) % 3) == 0) buf[out++] = ',';
        buf[out++] = tmp[i];
    }
    buf[out] = '\0';
}

static void format_multiplier(char *buf, size_t len, int streak)
{
    int bp = 100 + 10 * (streak - 1);
    if (bp < 100) bp = 100;
    snprintf(buf, len, "%d.%d×", bp / 100, (bp / 10) % 10);
}

static int next_milestone(int streak)
{
    static const int milestones[] = {3, 7, 14, 30};
    for (size_t i = 0; i < sizeof(milestones) / sizeof(milestones[0]); i++) {
        if (streak < milestones[i]) return milestones[i];
    }
    return 0;
}

static int streak_percent(int streak)
{
    int m = next_milestone(streak);
    if (m == 0) return 100;
    int pct = (streak * 100) / m;
    if (pct > 100) pct = 100;
    return pct;
}

/* ============================================================
 * DATA REFRESH
 * ============================================================ */
static const lv_img_dsc_t *stage_sprite_for_level(int level)
{
    if (level >= 4) return &plant_3_sprite;
    if (level >= 2) return &plant_2_sprite;
    return &plant_1_sprite;
}

static void refresh_pet_widgets(void)
{
    if (!chip_level || !lbl_chip_level) return;

    points_state_t *ps = points_store_get_state();
    int level = points_store_get_level();

    char seeds[16];
    format_seeds(seeds, sizeof(seeds), ps->total_points);
    lv_label_set_text_fmt(lbl_chip_level, "Level %d · %s seeds", level, seeds);

    if (plant_img) {
        lv_img_set_src(plant_img, stage_sprite_for_level(level));
    }

    if (bar_level) {
        int into = points_store_get_level_progress();
        int need = points_store_get_level_threshold();
        lv_bar_set_range(bar_level, 0, need);
        lv_bar_set_value(bar_level, into, LV_ANIM_OFF);
    }

    if (lbl_pill_today) {
        lv_label_set_text_fmt(lbl_pill_today, "Today +%d seeds", ps->today_points);
    }

    if (lbl_pill_streak) {
        int best = 0;
        int best_i = -1;
        for (int i = 0; i < STREAK_COUNT; i++) {
            if (ps->streaks[i].streak > best) {
                best = ps->streaks[i].streak;
                best_i = i;
            }
        }
        if (best_i >= 0) {
            lv_label_set_text_fmt(lbl_pill_streak, "%s · %dd", streak_names[best_i], best);
        } else {
            lv_label_set_text(lbl_pill_streak, "Start a streak");
        }
    }
}

static void refresh_goals_widgets(void)
{
    if (!summary_card) return;

    points_state_t *ps = points_store_get_state();
    int level = points_store_get_level();
    int into = points_store_get_level_progress();
    int need = points_store_get_level_threshold();

    char seeds[16];
    char prog[16];
    format_seeds(seeds, sizeof(seeds), ps->total_points);
    format_seeds(prog, sizeof(prog), into);

    lv_label_set_text(lbl_seeds_num, seeds);
    lv_label_set_text_fmt(lbl_summary_meta, "%d earned today · Level %d", ps->today_points, level);
    lv_label_set_text_fmt(lbl_prog_left, "Level %d", level);
    lv_label_set_text_fmt(lbl_prog_right, "%s / %d", prog, need);

    if (bar_progress) {
        lv_bar_set_range(bar_progress, 0, need);
        lv_bar_set_value(bar_progress, into, LV_ANIM_OFF);
    }

    int done = 0;
    for (int i = 0; i < MAX_DAILY_GOALS; i++) {
        daily_goal_t *g = &ps->goals[i];
        bool has = (g->label[0] != '\0');

        if (goal_rows[i]) {
            lv_obj_t *row = goal_rows[i];
            if (g->done) {
                lv_obj_set_style_bg_color(row, ACCENT_LIGHT, 0);
                lv_obj_set_style_bg_opa(row, LV_OPA_30, 0);
            } else {
                lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
            }
        }

        if (goal_text[i]) {
            lv_obj_t *txt = goal_text[i];
            if (has) {
                lv_label_set_text(txt, g->label);
                lv_obj_set_style_text_color(txt, g->done ? LV_COLOR_TEXT_MUTED : LV_COLOR_TEXT, 0);
                lv_label_set_long_mode(txt, LV_LABEL_LONG_DOT);
            } else {
                lv_label_set_text(txt, "No goal set yet");
                lv_obj_set_style_text_color(txt, LV_COLOR_TEXT_MUTED, 0);
            }
        }

        if (goal_check[i]) {
            if (g->done) {
                lv_label_set_text(goal_check[i], LV_SYMBOL_OK);
                lv_obj_set_style_bg_color(goal_check[i], LV_COLOR_SUCCESS, 0);
                lv_obj_set_style_border_color(goal_check[i], LV_COLOR_SUCCESS, 0);
                lv_obj_set_style_text_color(goal_check[i], LV_COLOR_BG_CARD, 0);
            } else {
                lv_label_set_text(goal_check[i], "");
                lv_obj_set_style_bg_color(goal_check[i], LV_COLOR_BG_CARD, 0);
                lv_obj_set_style_border_color(goal_check[i], LV_COLOR_BORDER, 0);
            }
            done += g->done ? 1 : 0;
        }
    }

    if (lbl_section_count) {
        lv_label_set_text_fmt(lbl_section_count, "%d/%d done", done, MAX_DAILY_GOALS);
    }
}

static void refresh_streaks_widgets(void)
{
    if (!streaks_container) return;

    static const char *symbols[] = {
        LV_SYMBOL_PLAY, LV_SYMBOL_TINT, LV_SYMBOL_REFRESH, LV_SYMBOL_OK, LV_SYMBOL_EYE_OPEN,
        LV_SYMBOL_PLUS
    };

    /* Per-app theme colours. Where a logo exists we derive the theme from the
     * logo artwork (same approach as the menu); the logo-less "Move" app uses
     * a fixed warm amber. */
    lv_color_t theme[STREAK_COUNT];
    for (int i = 0; i < STREAK_COUNT; i++) {
        theme[i] = streak_logos[i] ? compute_dominant_color(streak_logos[i])
                                   : lv_color_hex(0xDE8E36);
    }

    points_state_t *ps = points_store_get_state();

    for (int i = 0; i < STREAK_COUNT; i++) {
        streak_t *st = &ps->streaks[i];

        if (streak_name[i]) lv_label_set_text(streak_name[i], streak_names[i]);
        if (streak_icon[i]) {
            lv_obj_set_style_bg_color(streak_icon[i], lighten_color(theme[i], 0.72f), 0);
            if (!streak_icon_is_img[i] && streak_icon_sym[i]) {
                lv_label_set_text(streak_icon_sym[i], symbols[i]);
                lv_obj_set_style_text_color(streak_icon_sym[i], theme[i], 0);
            }
        }
        if (streak_days[i]) {
            if (st->streak > 0) {
                lv_label_set_text_fmt(streak_days[i], "%d day%s", st->streak,
                                      st->streak == 1 ? "" : "s");
                lv_obj_set_style_text_color(streak_days[i], TITLE_TEXT, 0);
            } else {
                lv_label_set_text(streak_days[i], "Not started");
                lv_obj_set_style_text_color(streak_days[i], LV_COLOR_TEXT_MUTED, 0);
            }
        }
        if (streak_bar[i]) {
            lv_obj_set_style_bg_color(streak_bar[i], lighten_color(theme[i], 0.55f), 0);
            lv_obj_set_style_bg_color(streak_bar[i], theme[i], LV_PART_INDICATOR);
            lv_bar_set_range(streak_bar[i], 0, 100);
            lv_bar_set_value(streak_bar[i], streak_percent(st->streak), LV_ANIM_OFF);
        }
        if (streak_mult[i]) {
            if (st->streak > 0) {
                char mult[16];
                format_multiplier(mult, sizeof(mult), st->streak);
                int m = next_milestone(st->streak);
                if (m > 0) {
                    lv_label_set_text_fmt(streak_mult[i], "Next: %dd · %s seeds", m, mult);
                } else {
                    lv_label_set_text_fmt(streak_mult[i], "Max streak · %s seeds", mult);
                }
            } else {
                lv_label_set_text(streak_mult[i], "Complete it once to start");
            }
        }
    }
}

/* ============================================================
 * FOCUS STYLES
 * ============================================================ */
static void set_focus_list(lv_obj_t **list, int count)
{
    focus_count = count;
    for (int i = 0; i < count; i++) focus_list[i] = list[i];
}

static void update_focus_styles(void)
{
    bool in_list = (current_state != TAMA_STATE_PET);
    if (focus_index != prev_focus_index) {
        for (int i = 0; i < focus_count; i++) {
            lv_obj_t *obj = focus_list[i];
            if (!obj) continue;
            if (i == focus_index) {
                animate_style(obj, (lv_anim_exec_xcb_t)anim_set_border_width,
                              0, 4, FOCUS_ANIM_MS, 0);
                lv_obj_set_style_border_color(obj, ACCENT_LIGHT, 0);
                if (!in_list) {
                    animate_style(obj, (lv_anim_exec_xcb_t)anim_set_opa,
                                  LV_OPA_80, LV_OPA_COVER, FOCUS_ANIM_MS, 0);
                }
            } else {
                animate_style(obj, (lv_anim_exec_xcb_t)anim_set_border_width,
                              4, 0, FOCUS_ANIM_MS, 0);
                if (!in_list) {
                    animate_style(obj, (lv_anim_exec_xcb_t)anim_set_opa,
                                  LV_OPA_COVER, LV_OPA_80, FOCUS_ANIM_MS, 0);
                }
            }
        }
        prev_focus_index = focus_index;
        focused_row = (focus_count > 0) ? focus_list[focus_index] : NULL;
    }
}

/* ============================================================
 * RADIAL SCROLL (mirrors screen_menu / screen_todos)
 * ============================================================ */
static lv_obj_t *scroll_container_for(tama_state_t state)
{
    if (state == TAMA_STATE_GOALS)   return goals_container;
    if (state == TAMA_STATE_STREAKS) return streaks_container;
    return NULL;
}

static lv_obj_t **scroll_rows_for(tama_state_t state)
{
    if (state == TAMA_STATE_GOALS)   return (lv_obj_t **)goals_scroll_rows;
    if (state == TAMA_STATE_STREAKS) return (lv_obj_t **)streaks_scroll_rows;
    return NULL;
}

static int scroll_count_for(tama_state_t state)
{
    if (state == TAMA_STATE_GOALS)   return GOALS_COUNT;
    if (state == TAMA_STATE_STREAKS) return STREAKS_ROWS;
    return 0;
}

static void apply_radial_scroll(void)
{
    lv_obj_t *container = scroll_container_for(current_state);
    if (!container) return;

    lv_obj_t **rows = scroll_rows_for(current_state);
    int count = scroll_count_for(current_state);

    lv_obj_update_layout(container);

    bool any_hidden_top = false;
    bool any_hidden_bottom = false;

    for (int i = 0; i < count; i++) {
        lv_obj_t *row = rows[i];
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
        if (focused_row) {
            lv_coord_t focus_mid = focused_row->coords.y1 + lv_obj_get_height(focused_row) / 2;
            lv_coord_t opa_dy = mid_y - focus_mid;
            lv_coord_t opa_ady = opa_dy < 0 ? -opa_dy : opa_dy;
            if (opa_ady < OPA_FADE) {
                opa = LV_OPA_COVER;
            } else {
                float fade = 1.0f - (float)(opa_ady - OPA_FADE) / (float)(OPA_R - OPA_FADE);
                if (fade < 0.15f) fade = 0.15f;
                opa = (lv_opa_t)(fade * 255);
            }
        } else {
            opa = LV_OPA_COVER;
        }
        lv_obj_set_style_opa(row, opa, 0);
    }

    lv_obj_t *arrow_up = (current_state == TAMA_STATE_GOALS) ? goals_arrow_up : streaks_arrow_up;
    lv_obj_t *arrow_down = (current_state == TAMA_STATE_GOALS) ? goals_arrow_down : streaks_arrow_down;
    if (any_hidden_top) {
        if (arrow_up) lv_obj_clear_flag(arrow_up, LV_OBJ_FLAG_HIDDEN);
    } else if (arrow_up) {
        lv_obj_add_flag(arrow_up, LV_OBJ_FLAG_HIDDEN);
    }
    if (any_hidden_bottom) {
        if (arrow_down) lv_obj_clear_flag(arrow_down, LV_OBJ_FLAG_HIDDEN);
    } else if (arrow_down) {
        lv_obj_add_flag(arrow_down, LV_OBJ_FLAG_HIDDEN);
    }
}

static void radial_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    apply_radial_scroll();
}

static void scroll_cb(lv_event_t *e)
{
    (void)e;
    apply_radial_scroll();
}

static void initial_scroll_cb(lv_timer_t *timer)
{
    (void)timer;
    scroll_to_focused(false);
    update_focus_styles();
}

static void scroll_to_focused(bool animate)
{
    if (!focused_row) return;
    lv_obj_t *container = scroll_container_for(current_state);
    if (container) {
        lv_obj_update_layout(container);
        lv_obj_scroll_to_view(focused_row, animate ? LV_ANIM_ON : LV_ANIM_OFF);
    }
    apply_radial_scroll();
}

/* ============================================================
 * STATE TRANSITIONS
 * ============================================================ */
static void hide_all_widgets(void)
{
    if (lbl_title) lv_obj_add_flag(lbl_title, LV_OBJ_FLAG_HIDDEN);
    if (chip_level) lv_obj_add_flag(chip_level, LV_OBJ_FLAG_HIDDEN);
    if (bar_level) lv_obj_add_flag(bar_level, LV_OBJ_FLAG_HIDDEN);
    if (pet_stats) lv_obj_add_flag(pet_stats, LV_OBJ_FLAG_HIDDEN);
    if (lbl_pet_hint) lv_obj_add_flag(lbl_pet_hint, LV_OBJ_FLAG_HIDDEN);
    if (btn_goals) lv_obj_add_flag(btn_goals, LV_OBJ_FLAG_HIDDEN);

    if (summary_card) lv_obj_add_flag(summary_card, LV_OBJ_FLAG_HIDDEN);
    if (goals_header) lv_obj_add_flag(goals_header, LV_OBJ_FLAG_HIDDEN);
    if (lbl_section) lv_obj_add_flag(lbl_section, LV_OBJ_FLAG_HIDDEN);
    if (lbl_section_count) lv_obj_add_flag(lbl_section_count, LV_OBJ_FLAG_HIDDEN);

    if (goals_container) lv_obj_add_flag(goals_container, LV_OBJ_FLAG_HIDDEN);
    if (goals_arrow_up) lv_obj_add_flag(goals_arrow_up, LV_OBJ_FLAG_HIDDEN);
    if (goals_arrow_down) lv_obj_add_flag(goals_arrow_down, LV_OBJ_FLAG_HIDDEN);

    if (streaks_container) lv_obj_add_flag(streaks_container, LV_OBJ_FLAG_HIDDEN);
    if (streaks_arrow_up) lv_obj_add_flag(streaks_arrow_up, LV_OBJ_FLAG_HIDDEN);
    if (streaks_arrow_down) lv_obj_add_flag(streaks_arrow_down, LV_OBJ_FLAG_HIDDEN);
}

static void show_widget(lv_obj_t *obj)
{
    if (!obj) return;
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
}

static void transition_to_pet(void)
{
    hide_all_widgets();

    current_state = TAMA_STATE_PET;
    focus_index = 0;
    prev_focus_index = -1;
    focused_row = NULL;
    if (radial_timer) lv_timer_pause(radial_timer);

    lv_label_set_text(lbl_title, "Tamagotchi");
    refresh_pet_widgets();

    lv_obj_t *list[] = { btn_goals };
    set_focus_list(list, 1);

    show_widget(lbl_title);
    show_widget(chip_level);
    show_widget(bar_level);
    show_widget(pet_stats);
    show_widget(plant_img);
    show_widget(lbl_pet_hint);
    show_widget(btn_goals);

    if (bg_overlay) lv_obj_add_flag(bg_overlay, LV_OBJ_FLAG_HIDDEN);

    update_focus_styles();
    ESP_LOGI(TAG, "Transitioned to Pet state");
}

static void transition_to_goals(void)
{
    hide_all_widgets();

    current_state = TAMA_STATE_GOALS;
    focus_index = 0;
    prev_focus_index = -1;
    focused_row = goals_scroll_rows[0];
    if (radial_timer) lv_timer_resume(radial_timer);

    lv_label_set_text(lbl_title, "Goals & Seeds");
    refresh_goals_widgets();

    lv_obj_t *list[] = { goals_scroll_rows[0], goals_scroll_rows[1], goals_scroll_rows[2],
                         goals_scroll_rows[3], goals_scroll_rows[4] };
    set_focus_list(list, GOALS_COUNT);

    show_widget(lbl_title);
    show_widget(summary_card);
    // show_widget(goals_header);
    show_widget(goals_container);
    show_widget(goals_arrow_down);

    if (bg_overlay) lv_obj_clear_flag(bg_overlay, LV_OBJ_FLAG_HIDDEN);

    update_focus_styles();
    scroll_to_focused(false);
    ESP_LOGI(TAG, "Transitioned to Goals state");
}

static void transition_to_streaks(void)
{
    hide_all_widgets();

    current_state = TAMA_STATE_STREAKS;
    focus_index = 0;
    prev_focus_index = -1;
    focused_row = streaks_scroll_rows[0];
    if (radial_timer) lv_timer_resume(radial_timer);

    lv_label_set_text(lbl_title, "Streaks");
    refresh_streaks_widgets();

    lv_obj_t *list[] = { streaks_scroll_rows[0], streaks_scroll_rows[1], streaks_scroll_rows[2],
                         streaks_scroll_rows[3], streaks_scroll_rows[4], streaks_scroll_rows[5],
                         streaks_scroll_rows[6] };
    set_focus_list(list, STREAKS_ROWS);

    show_widget(lbl_title);
    show_widget(streaks_container);
    show_widget(streaks_arrow_up);
    show_widget(streaks_arrow_down);

    if (bg_overlay) lv_obj_clear_flag(bg_overlay, LV_OBJ_FLAG_HIDDEN);

    update_focus_styles();
    scroll_to_focused(false);
    ESP_LOGI(TAG, "Transitioned to Streaks state");
}

static void toggle_goal(int index)
{
    if (index < 0 || index >= MAX_DAILY_GOALS) return;

    points_state_t *ps = points_store_get_state();
    daily_goal_t *g = &ps->goals[index];
    if (g->label[0] == '\0') return;

    int pts = points_store_toggle_goal(index, !g->done);
    persistence_mark_dirty();
    refresh_goals_widgets();
    if (pts > 0) {
        app_state_broadcast_points_earned(pts, POINT_REASON_DAILY_GOAL, index);
    }
    app_state_broadcast_points_sync();
}

/* ============================================================
 * LIST ROW BUILDERS
 * ============================================================ */
static lv_obj_t *create_list_container(lv_obj_t *parent)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, 480, 480);
    lv_obj_align(c, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 0, 0);
    lv_obj_set_scroll_dir(c, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_y(c, LV_SCROLL_SNAP_CENTER);
    lv_obj_add_event_cb(c, scroll_cb, LV_EVENT_SCROLL, NULL);
    return c;
}

static lv_obj_t *create_nav_row(lv_obj_t *parent, const char *text)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, MAX_ROW_W, 52);
    lv_obj_set_style_bg_color(row, LV_COLOR_SURFACE, 0);
    lv_obj_set_style_radius(row, 26, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_border_color(row, ACCENT_LIGHT, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_min_width(row, 0, 0);

    lv_obj_t *lbl = lv_label_create(row);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, TITLE_TEXT, 0);
    lv_obj_center(lbl);
    return row;
}

static lv_obj_t *create_arrow(lv_obj_t *parent, bool up)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, up ? LV_SYMBOL_UP : LV_SYMBOL_DOWN);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl, TITLE_TEXT, 0);
    if (up) lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 60);
    else    lv_obj_align(lbl, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_add_flag(lbl, LV_OBJ_FLAG_HIDDEN);
    return lbl;
}

static void create_goal_row(int index)
{
    lv_obj_t *row = lv_obj_create(goals_container);
    lv_obj_set_size(row, MAX_ROW_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_radius(row, 26, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_border_color(row, ACCENT_LIGHT, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 12, 0);
    lv_obj_set_style_pad_column(row, 14, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_min_width(row, 0, 0);
    goal_rows[index] = row;

    lv_obj_t *check = lv_label_create(row);
    lv_obj_set_size(check, 32, 32);
    lv_obj_set_style_radius(check, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(check, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_border_width(check, 2, 0);
    lv_obj_set_style_border_color(check, LV_COLOR_BORDER, 0);
    lv_obj_set_style_text_font(check, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(check, LV_COLOR_BG_CARD, 0);
    goal_check[index] = check;

    lv_obj_t *txt = lv_label_create(row);
    lv_label_set_text(txt, "");
    lv_obj_set_flex_grow(txt, 1);
    lv_obj_set_style_text_font(txt, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(txt, LV_COLOR_TEXT, 0);
    lv_label_set_long_mode(txt, LV_LABEL_LONG_DOT);
    goal_text[index] = txt;

    lv_obj_set_pos(row, (480 - MAX_ROW_W) / 2, GOALS_TOP + (index + 1) * GOALS_SPACING);
    goals_scroll_rows[index + 1] = row;
}

static void create_streak_row(int index)
{
    lv_obj_t *row = lv_obj_create(streaks_container);
    lv_obj_set_size(row, MAX_ROW_W, STREAK_ROW_H);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(row, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_radius(row, 20, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_border_color(row, ACCENT_LIGHT, 0);
    lv_obj_set_style_shadow_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 8, 0);
    lv_obj_set_style_pad_row(row, 2, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_min_width(row, 0, 0);
    streak_rows[index] = row;

    lv_obj_t *top = lv_obj_create(row);
    lv_obj_set_size(top, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(top, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(top, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(top, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(top, 0, 0);
    lv_obj_set_style_pad_all(top, 0, 0);
    lv_obj_set_style_pad_column(top, 10, 0);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *icon = lv_obj_create(top);
    lv_obj_set_size(icon, 34, 34);
    lv_obj_set_style_radius(icon, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(icon, LV_COLOR_SURFACE, 0);
    lv_obj_set_style_border_width(icon, 0, 0);
    lv_obj_set_style_shadow_width(icon, 0, 0);
    lv_obj_set_style_pad_all(icon, 0, 0);
    lv_obj_clear_flag(icon, LV_OBJ_FLAG_SCROLLABLE);
    if (streak_logos[index]) {
        lv_obj_t *logo = lv_img_create(icon);
        lv_img_set_src(logo, streak_logos[index]);
        lv_obj_center(logo);
        streak_icon_is_img[index] = true;
    } else {
        lv_obj_t *sym = lv_label_create(icon);
        lv_label_set_text(sym, "");
        lv_obj_set_style_text_font(sym, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(sym, LV_COLOR_BG_CARD, 0);
        lv_obj_center(sym);
        streak_icon_sym[index] = sym;
    }
    streak_icon[index] = icon;

    lv_obj_t *name = lv_label_create(top);
    lv_label_set_text(name, "");
    lv_obj_set_style_text_font(name, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(name, LV_COLOR_TEXT, 0);
    lv_label_set_long_mode(name, LV_LABEL_LONG_DOT);
    lv_obj_set_flex_grow(name, 1);
    streak_name[index] = name;

    lv_obj_t *days = lv_label_create(top);
    lv_label_set_text(days, "");
    lv_obj_set_style_text_font(days, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(days, TITLE_TEXT, 0);
    lv_label_set_long_mode(days, LV_LABEL_LONG_DOT);
    streak_days[index] = days;

    lv_obj_t *bottom = lv_obj_create(row);
    lv_obj_set_size(bottom, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(bottom, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bottom, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(bottom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bottom, 0, 0);
    lv_obj_set_style_pad_all(bottom, 0, 0);
    lv_obj_set_style_pad_column(bottom, 10, 0);
    lv_obj_clear_flag(bottom, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *bar = lv_bar_create(bottom);
    lv_obj_set_flex_grow(bar, 1);
    lv_obj_set_height(bar, 6);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bar, LV_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar, LV_COLOR_INFO, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_bar_set_range(bar, 0, 100);
    lv_bar_set_value(bar, 0, LV_ANIM_OFF);
    streak_bar[index] = bar;

    lv_obj_t *mult = lv_label_create(bottom);
    lv_label_set_text(mult, "");
    lv_obj_set_style_text_font(mult, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(mult, LV_COLOR_TEXT_MUTED, 0);
    lv_label_set_long_mode(mult, LV_LABEL_LONG_DOT);
    streak_mult[index] = mult;

    lv_obj_set_pos(row, (480 - MAX_ROW_W) / 2, STREAKS_TOP + index * STREAKS_SPACING);
    streaks_scroll_rows[index] = row;
}

/* ============================================================
 * SCREEN CREATE
 * ============================================================ */
lv_obj_t *screen_tamagotchi_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, pastel_color(ACCENT), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* ---- persistent backdrop (always visible across all sub-states) ---- */
    bg_img = lv_img_create(screen);
    lv_img_set_src(bg_img, &tamagotchi_backdrop);
    lv_obj_align(bg_img, LV_ALIGN_CENTER, 0, 0);

    bg_overlay = lv_obj_create(screen);
    lv_obj_set_size(bg_overlay, 480, 480);
    lv_obj_align(bg_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(bg_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(bg_overlay, LV_OPA_40, 0);
    lv_obj_set_style_border_width(bg_overlay, 0, 0);
    lv_obj_set_style_shadow_width(bg_overlay, 0, 0);
    lv_obj_set_style_pad_all(bg_overlay, 0, 0);
    lv_obj_clear_flag(bg_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(bg_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(bg_overlay, LV_OBJ_FLAG_HIDDEN);

    /* ---- plant (below all UI widgets in z-order) ---- */
    plant_img = lv_img_create(screen);
    lv_img_set_src(plant_img, &plant_1_sprite);
    lv_obj_align(plant_img, LV_ALIGN_BOTTOM_MID, 0, 10);
    lv_obj_add_flag(plant_img, LV_OBJ_FLAG_HIDDEN);

    /* ---- shared title ---- */
    lbl_title = lv_label_create(screen);
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, TITLE_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_add_flag(lbl_title, LV_OBJ_FLAG_HIDDEN);

    /* ---- GOALS state ---- */
    goals_container = create_list_container(screen);
    lv_obj_add_flag(goals_container, LV_OBJ_FLAG_HIDDEN);

    goals_nav_streaks = create_nav_row(goals_container, "Streaks");
    lv_obj_set_pos(goals_nav_streaks, (480 - MAX_ROW_W) / 2, GOALS_TOP);
    goals_scroll_rows[0] = goals_nav_streaks;

    for (int i = 0; i < MAX_DAILY_GOALS; i++) {
        create_goal_row(i);
    }

    goals_nav_back = create_nav_row(goals_container, "Back");
    lv_obj_set_pos(goals_nav_back, (480 - MAX_ROW_W) / 2,
                   GOALS_TOP + (MAX_DAILY_GOALS + 1) * GOALS_SPACING);
    goals_scroll_rows[MAX_DAILY_GOALS + 1] = goals_nav_back;

    /* Invisible spacer below the list so the GOALS content is taller than the
     * 480px screen. Without it the focused row can't be scrolled to center and
     * stays in the radial fade zone (opa<255), rendering the white cards muddy. */
    lv_obj_t *goals_spacer = lv_obj_create(goals_container);
    lv_obj_set_size(goals_spacer, MAX_ROW_W, GOALS_PAD_BOTTOM);
    lv_obj_set_pos(goals_spacer, (480 - MAX_ROW_W) / 2,
                   GOALS_TOP + (MAX_DAILY_GOALS + 1) * GOALS_SPACING + 52);
    lv_obj_set_style_bg_opa(goals_spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(goals_spacer, 0, 0);
    lv_obj_clear_flag(goals_spacer, LV_OBJ_FLAG_SCROLLABLE);

    goals_arrow_down = create_arrow(screen, false);

    summary_card = lv_obj_create(screen);
    lv_obj_set_size(summary_card, 340, 118);
    lv_obj_align(summary_card, LV_ALIGN_TOP_MID, 0, 64);
    lv_obj_set_style_bg_color(summary_card, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_radius(summary_card, 22, 0);
    lv_obj_set_style_border_width(summary_card, 0, 0);
    lv_obj_set_style_shadow_width(summary_card, 0, 0);
    lv_obj_set_style_pad_all(summary_card, 0, 0);
    lv_obj_clear_flag(summary_card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(summary_card, LV_OBJ_FLAG_HIDDEN);

    lbl_seeds_num = lv_label_create(summary_card);
    lv_label_set_text(lbl_seeds_num, "0");
    lv_obj_set_style_text_font(lbl_seeds_num, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(lbl_seeds_num, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_seeds_num, LV_ALIGN_TOP_LEFT, 20, 12);

    lbl_seeds_word = lv_label_create(summary_card);
    lv_label_set_text(lbl_seeds_word, "seeds");
    lv_obj_set_style_text_font(lbl_seeds_word, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_seeds_word, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(lbl_seeds_word, LV_ALIGN_TOP_LEFT, 150, 24);

    lbl_summary_meta = lv_label_create(summary_card);
    lv_label_set_text(lbl_summary_meta, "0 earned today · Level 1");
    lv_obj_set_style_text_font(lbl_summary_meta, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_summary_meta, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lbl_summary_meta, LV_ALIGN_TOP_LEFT, 20, 52);

    lbl_prog_left = lv_label_create(summary_card);
    lv_label_set_text(lbl_prog_left, "Level 1");
    lv_obj_set_style_text_font(lbl_prog_left, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_prog_left, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lbl_prog_left, LV_ALIGN_BOTTOM_LEFT, 20, -4);

    lbl_prog_right = lv_label_create(summary_card);
    lv_label_set_text(lbl_prog_right, "0 / 100");
    lv_obj_set_style_text_font(lbl_prog_right, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_prog_right, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(lbl_prog_right, LV_ALIGN_BOTTOM_RIGHT, -20, -4);

    bar_progress = lv_bar_create(summary_card);
    lv_obj_set_size(bar_progress, 300, 12);
    lv_obj_align(bar_progress, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_set_style_radius(bar_progress, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bar_progress, LV_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(bar_progress, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar_progress, ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_progress, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_bar_set_range(bar_progress, 0, 100);
    lv_bar_set_value(bar_progress, 0, LV_ANIM_OFF);

    goals_header = lv_obj_create(screen);
    lv_obj_set_size(goals_header, 340, 30);
    lv_obj_align(goals_header, LV_ALIGN_TOP_MID, 0, 190);
    lv_obj_set_style_bg_color(goals_header, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_bg_opa(goals_header, LV_OPA_80, 0);
    lv_obj_set_style_radius(goals_header, 15, 0);
    lv_obj_set_style_border_width(goals_header, 0, 0);
    lv_obj_set_style_shadow_width(goals_header, 0, 0);
    lv_obj_set_style_pad_hor(goals_header, 16, 0);
    lv_obj_set_style_pad_ver(goals_header, 0, 0);
    lv_obj_clear_flag(goals_header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(goals_header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(goals_header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(goals_header, LV_OBJ_FLAG_HIDDEN);

    lbl_section = lv_label_create(goals_header);
    lv_label_set_text(lbl_section, "Today's goals");
    lv_obj_set_style_text_font(lbl_section, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(lbl_section, TITLE_TEXT, 0);
    lv_obj_set_flex_grow(lbl_section, 1);

    lbl_section_count = lv_label_create(goals_header);
    lv_label_set_text(lbl_section_count, "0/3 done");
    lv_obj_set_style_text_font(lbl_section_count, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_section_count, LV_COLOR_TEXT_MUTED, 0);

    /* ---- STREAKS state ---- */
    streaks_container = create_list_container(screen);
    lv_obj_add_flag(streaks_container, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < STREAK_COUNT; i++) {
        create_streak_row(i);
    }

    streaks_back_row = create_nav_row(streaks_container, "Back");
    lv_obj_set_pos(streaks_back_row, (480 - MAX_ROW_W) / 2,
                   STREAKS_TOP + STREAK_COUNT * STREAKS_SPACING);
    streaks_scroll_rows[STREAK_COUNT] = streaks_back_row;

    streaks_arrow_up = create_arrow(screen, true);
    streaks_arrow_down = create_arrow(screen, false);

    /* ---- PET state ---- */
    chip_level = lv_obj_create(screen);
    lv_obj_align(chip_level, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_size(chip_level, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(chip_level, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_bg_opa(chip_level, LV_OPA_30, 0);
    lv_obj_set_style_radius(chip_level, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(chip_level, 0, 0);
    lv_obj_set_style_shadow_width(chip_level, 0, 0);
    lv_obj_set_style_pad_ver(chip_level, 4, 0);
    lv_obj_set_style_pad_hor(chip_level, 12, 0);
    lv_obj_clear_flag(chip_level, LV_OBJ_FLAG_SCROLLABLE);
    lbl_chip_level = lv_label_create(chip_level);
    lv_label_set_text(lbl_chip_level, "Level 1 · 0 seeds");
    lv_obj_set_style_text_font(lbl_chip_level, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_chip_level, TITLE_TEXT, 0);
    lv_obj_center(lbl_chip_level);
    lv_obj_add_flag(chip_level, LV_OBJ_FLAG_HIDDEN);

    bar_level = lv_bar_create(screen);
    lv_obj_set_size(bar_level, 160, 4);
    lv_obj_align(bar_level, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_set_style_radius(bar_level, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bar_level, LV_COLOR_SURFACE, 0);
    lv_obj_set_style_bg_opa(bar_level, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(bar_level, ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_radius(bar_level, LV_RADIUS_CIRCLE, LV_PART_INDICATOR);
    lv_bar_set_range(bar_level, 0, 100);
    lv_bar_set_value(bar_level, 0, LV_ANIM_OFF);
    lv_obj_add_flag(bar_level, LV_OBJ_FLAG_HIDDEN);

    pet_stats = lv_obj_create(screen);
    lv_obj_set_size(pet_stats, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(pet_stats, LV_ALIGN_TOP_MID, 0, 96);
    lv_obj_set_style_bg_opa(pet_stats, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pet_stats, 0, 0);
    lv_obj_set_style_shadow_width(pet_stats, 0, 0);
    lv_obj_set_style_pad_all(pet_stats, 0, 0);
    lv_obj_set_style_pad_column(pet_stats, 8, 0);
    lv_obj_clear_flag(pet_stats, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(pet_stats, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(pet_stats, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    pill_today = lv_obj_create(pet_stats);
    lv_obj_set_size(pill_today, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_bg_color(pill_today, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_bg_opa(pill_today, LV_OPA_30, 0);
    lv_obj_set_style_radius(pill_today, 12, 0);
    lv_obj_set_style_border_width(pill_today, 0, 0);
    lv_obj_set_style_shadow_width(pill_today, 0, 0);
    lv_obj_set_style_pad_hor(pill_today, 10, 0);
    lv_obj_clear_flag(pill_today, LV_OBJ_FLAG_SCROLLABLE);
    lbl_pill_today = lv_label_create(pill_today);
    lv_label_set_text(lbl_pill_today, "Today +0 seeds");
    lv_obj_set_style_text_font(lbl_pill_today, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_pill_today, LV_COLOR_TEXT, 0);
    lv_obj_center(lbl_pill_today);

    pill_streak = lv_obj_create(pet_stats);
    lv_obj_set_size(pill_streak, LV_SIZE_CONTENT, 24);
    lv_obj_set_style_bg_color(pill_streak, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_bg_opa(pill_streak, LV_OPA_30, 0);
    lv_obj_set_style_radius(pill_streak, 12, 0);
    lv_obj_set_style_border_width(pill_streak, 0, 0);
    lv_obj_set_style_shadow_width(pill_streak, 0, 0);
    lv_obj_set_style_pad_hor(pill_streak, 10, 0);
    lv_obj_clear_flag(pill_streak, LV_OBJ_FLAG_SCROLLABLE);
    lbl_pill_streak = lv_label_create(pill_streak);
    lv_label_set_text(lbl_pill_streak, "Start a streak");
    lv_obj_set_style_text_font(lbl_pill_streak, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl_pill_streak, LV_COLOR_TEXT, 0);
    lv_obj_center(lbl_pill_streak);
    lv_obj_add_flag(pet_stats, LV_OBJ_FLAG_HIDDEN);

    lbl_pet_hint = lv_label_create(screen);
    lv_label_set_text(lbl_pet_hint, "Your plant grows as you earn seeds");
    lv_obj_set_style_text_font(lbl_pet_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_pet_hint, darken_text_color(ACCENT, 0.5f), 0);
    lv_obj_set_style_text_opa(lbl_pet_hint, LV_OPA_30, 0);
    lv_obj_align(lbl_pet_hint, LV_ALIGN_TOP_MID, 0, 130);
    lv_obj_add_flag(lbl_pet_hint, LV_OBJ_FLAG_HIDDEN);

    btn_goals = lv_btn_create(screen);
    lv_obj_set_size(btn_goals, 120, 40);
    lv_obj_align(btn_goals, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_radius(btn_goals, 20, 0);
    lv_obj_set_style_bg_color(btn_goals, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_bg_opa(btn_goals, LV_OPA_40, 0);
    lv_obj_set_style_shadow_width(btn_goals, 0, 0);
    lv_obj_set_style_shadow_opa(btn_goals, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_goals, 0, 0);
    lv_obj_set_style_border_color(btn_goals, ACCENT_LIGHT, 0);
    lv_obj_set_style_pad_all(btn_goals, 0, 0);
    lv_obj_t *lbl_goals = lv_label_create(btn_goals);
    lv_label_set_text(lbl_goals, "Goals");
    lv_obj_set_style_text_font(lbl_goals, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_goals, LV_COLOR_BG_CARD, 0);
    lv_obj_center(lbl_goals);
    lv_obj_add_flag(btn_goals, LV_OBJ_FLAG_HIDDEN);

    /* ---- radial scroll timer (paused until a list state is entered) ---- */
    radial_timer = lv_timer_create(radial_timer_cb, 50, NULL);
    lv_timer_pause(radial_timer);

    transition_to_pet();

    lv_timer_t *init_timer = lv_timer_create(initial_scroll_cb, 50, NULL);
    init_timer->repeat_count = 1;

    ESP_LOGI(TAG, "Tamagotchi screen created");
    return screen;
}

void screen_tamagotchi_refresh(void)
{
    if (!screen) return;
    transition_to_pet();
}

/* ============================================================
 * ENCODER EVENT HANDLER
 * ============================================================ */
void screen_tamagotchi_encoder_event(lv_indev_data_t *data)
{
    if (!data) return;

    bool is_pressed = (data->state == LV_INDEV_STATE_PR);

    switch (current_state) {
    case TAMA_STATE_PET:
        if (is_pressed && data->enc_diff == 0) {
            transition_to_goals();
        }
        break;

    case TAMA_STATE_GOALS:
        if (data->enc_diff != 0) {
            focus_index += data->enc_diff;
            if (focus_index < 0) focus_index = focus_count - 1;
            if (focus_index >= focus_count) focus_index = 0;
            update_focus_styles();
            scroll_to_focused(true);
        }
        if (is_pressed && data->enc_diff == 0) {
            if (focus_index == 0) {
                transition_to_streaks();
            } else if (focus_index >= 1 && focus_index <= MAX_DAILY_GOALS) {
                toggle_goal(focus_index - 1);
            } else if (focus_index == MAX_DAILY_GOALS + 1) {
                transition_to_pet();
            }
        }
        break;

    case TAMA_STATE_STREAKS:
        if (data->enc_diff != 0) {
            focus_index += data->enc_diff;
            if (focus_index < 0) focus_index = focus_count - 1;
            if (focus_index >= focus_count) focus_index = 0;
            update_focus_styles();
            scroll_to_focused(true);
        }
        if (is_pressed && data->enc_diff == 0) {
            transition_to_goals();
        }
        break;
    }
}
