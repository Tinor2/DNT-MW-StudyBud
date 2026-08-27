#include "demo_mode.h"
#include "app_state.h"
#include "screens/screen_demo.h"
#include "utils/points_store.h"
#include "utils/sleep_store.h"
#include "utils/session_store.h"
#include "utils/sedentary_store.h"
#include "utils/timer_store.h"
#include "utils/persistence.h"
#include "studybud_theme.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

static const char *TAG = "Demo_Mode";

/* ---- Checkpoint types ---- */

typedef enum {
    CHECKPOINT_INSTRUCTION,
    CHECKPOINT_WAIT_ACTION,
} checkpoint_type_t;

typedef struct {
    screen_id_t screen;
    checkpoint_type_t type;
    const char *instruction;
    const char *hint;
    bool (*is_done)(void);
} checkpoint_t;

/* ---- Forward declarations ---- */

static void go_to_checkpoint(uint8_t idx);
static void inflate_demo_seeds(int base_pts);
static bool check_breathing_done(void);
static bool check_water_added(void);
static bool check_todo_toggled(void);
static bool check_sleep_ended(void);
static bool check_timer_started(void);
static bool check_settings_adjusted(void);

/* ---- Checkpoint sequence ---- */

static const checkpoint_t checkpoints[] = {
    { SCREEN_TAMAGOTCHI, CHECKPOINT_INSTRUCTION,
      "This is your tamagotchi\npet. It grows as you\ncollect seeds.",
      "press to continue", NULL },
    { SCREEN_TAMAGOTCHI, CHECKPOINT_INSTRUCTION,
      "You earn seeds by\ncompleting healthy habits.\n\nLet's try a few!",
      "press to continue", NULL },
    { SCREEN_BREATHING, CHECKPOINT_INSTRUCTION,
      "First, try a breathing\nexercise to calm down\nand focus.",
      "press to continue", NULL },
    { SCREEN_BREATHING, CHECKPOINT_WAIT_ACTION,
      "Press the encoder to\nstart the exercise, then\nwait for 3 cycles.",
      "wait for cycles to finish", check_breathing_done },
    { SCREEN_WATER, CHECKPOINT_INSTRUCTION,
      "Nice work! +10 seeds.\nNext up: track your\nwater intake.",
      "press to continue", NULL },
    { SCREEN_WATER, CHECKPOINT_WAIT_ACTION,
      "Use the encoder to add\n2 glasses of water.",
      "add 2 glasses", check_water_added },
    { SCREEN_TODOS, CHECKPOINT_INSTRUCTION,
      "Great! +10 seeds per\nglass. Now try your\nhomework list.",
      "press to continue", NULL },
    { SCREEN_TODOS, CHECKPOINT_WAIT_ACTION,
      "Rotate to a task and\npress to check it off.",
      "complete a todo", check_todo_toggled },
    { SCREEN_TAMAGOTCHI, CHECKPOINT_INSTRUCTION,
      "Let's see how your\npet is doing!",
      "press to continue", NULL },
    { SCREEN_TAMAGOTCHI, CHECKPOINT_INSTRUCTION,
      "Look! Your pet grew\nand you reached\na new level!",
      "press to continue", NULL },
    { SCREEN_SLEEP, CHECKPOINT_INSTRUCTION,
      "Now let's log some\nsleep. This helps track\nyour rest patterns.",
      "press to continue", NULL },
    { SCREEN_SLEEP, CHECKPOINT_WAIT_ACTION,
      "Press to start tracking,\nwait a moment, then\npress again to stop.",
      "start then stop sleep", check_sleep_ended },
    { SCREEN_TIMER_PRESETS, CHECKPOINT_INSTRUCTION,
      "Sleep logged! +5 seeds.\nNow try the focus timer\nfor studying.",
      "press to continue", NULL },
    { SCREEN_TIMER_PRESETS, CHECKPOINT_WAIT_ACTION,
      "Rotate to a preset\nlike Pomodoro and press\nto start the timer.",
      "start a timer", check_timer_started },
    { SCREEN_SEDENTARY, CHECKPOINT_INSTRUCTION,
      "Timer started! +25 seeds.\nThis is the stretch\nbreak screen.",
      "press to continue", NULL },
    { SCREEN_SEDENTARY, CHECKPOINT_INSTRUCTION,
      "The app reminds you\nto stretch after sitting\nfor too long.",
      "press to continue", NULL },
    { SCREEN_SETTINGS, CHECKPOINT_INSTRUCTION,
      "Last stop: device\nsettings. Adjust volume,\nbrightness, and more.",
      "press to continue", NULL },
    { SCREEN_SETTINGS, CHECKPOINT_WAIT_ACTION,
      "Rotate to brightness\nor volume and press to\nadjust the slider.",
      "adjust a setting", check_settings_adjusted },
    { SCREEN_TAMAGOTCHI, CHECKPOINT_INSTRUCTION,
      "Let's see your final\nresult!",
      "press to continue", NULL },
    { SCREEN_TAMAGOTCHI, CHECKPOINT_INSTRUCTION,
      "Amazing! Through all\nthose habits, your pet\nis now fully grown!",
      "press to end demo", NULL },
};

#define CHECKPOINT_COUNT (sizeof(checkpoints) / sizeof(checkpoints[0]))

/* ---- Completion check baselines ---- */

static int s_water_baseline = 0;
static int s_settings_baseline = 0;
static bool s_todo_was_done = false;
static bool s_breathing_seen_active = false;
static bool s_sleep_seen_active = false;

static bool check_breathing_done(void)
{
    if (app_state_get()->breathing_active) s_breathing_seen_active = true;
    return s_breathing_seen_active && !app_state_get()->breathing_active;
}

static bool check_water_added(void)
{
    return app_state_get()->water.glasses >= s_water_baseline + 2;
}

static bool check_todo_toggled(void)
{
    app_state_t *s = app_state_get();
    for (int i = 0; i < s->todo_count; i++) {
        if (s->todos[i].done && !s_todo_was_done) return true;
    }
    return false;
}

static bool check_sleep_ended(void)
{
    if (sleep_store_is_active()) s_sleep_seen_active = true;
    return s_sleep_seen_active && !sleep_store_is_active();
}

static bool check_timer_started(void)
{
    return app_state_get()->timer.is_running;
}

static bool check_settings_adjusted(void)
{
    app_state_t *s = app_state_get();
    return s->settings.brightness != s_settings_baseline;
}

/* ---- State ---- */

typedef struct {
    bool valid;
    app_state_t app;
    points_state_t points;
    sedentary_state_t sedentary;
    screen_id_t previous_screen;
} demo_backup_t;

static bool s_active = false;
static uint8_t s_checkpoint = 0;
static bool s_task_mode = false;
static demo_backup_t s_backup;

/* Overlay */
static lv_obj_t *s_overlay = NULL;
static lv_obj_t *s_inst_label = NULL;
static lv_obj_t *s_hint_label = NULL;
static lv_obj_t *s_progress_bar = NULL;

/* Task completion polling timer */
static lv_timer_t *s_task_check_timer = NULL;

/* ---- Overlay (compact card for circular display) ---- */

static void overlay_create(void)
{
    if (s_overlay) return;

    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_overlay, 280, 100);
    lv_obj_align(s_overlay, LV_ALIGN_TOP_MID, 0, 50);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_80, 0);
    lv_obj_set_style_radius(s_overlay, 16, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_style_shadow_width(s_overlay, 0, 0);
    lv_obj_set_style_pad_all(s_overlay, 12, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);

    s_inst_label = lv_label_create(s_overlay);
    lv_label_set_text(s_inst_label, "");
    lv_obj_set_style_text_color(s_inst_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_inst_label, &lv_font_montserrat_14, 0);
    lv_label_set_long_mode(s_inst_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_inst_label, 256);
    lv_obj_align(s_inst_label, LV_ALIGN_TOP_MID, 0, 0);

    s_hint_label = lv_label_create(s_overlay);
    lv_label_set_text(s_hint_label, "");
    lv_obj_set_style_text_color(s_hint_label, LV_COLOR_PRIMARY, 0);
    lv_obj_set_style_text_font(s_hint_label, &lv_font_montserrat_14, 0);
    lv_obj_align(s_hint_label, LV_ALIGN_BOTTOM_MID, 0, 0);

    s_progress_bar = lv_bar_create(s_overlay);
    lv_obj_set_size(s_progress_bar, 256, 4);
    lv_obj_align(s_progress_bar, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_bar_set_range(s_progress_bar, 0, (int)CHECKPOINT_COUNT);
    lv_bar_set_value(s_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(s_progress_bar, lv_color_hex(0x333355), 0);
    lv_obj_set_style_bg_opa(s_progress_bar, LV_OPA_60, 0);
    lv_obj_set_style_bg_color(s_progress_bar, LV_COLOR_PRIMARY, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(s_progress_bar, LV_OPA_60, LV_PART_INDICATOR);
    lv_obj_set_style_radius(s_progress_bar, 2, 0);
    lv_obj_set_style_radius(s_progress_bar, 2, LV_PART_INDICATOR);
}

static void overlay_update(void)
{
    if (!s_inst_label || !s_hint_label || !s_progress_bar) return;
    const checkpoint_t *cp = &checkpoints[s_checkpoint];
    lv_label_set_text(s_inst_label, cp->instruction);
    lv_label_set_text(s_hint_label, cp->hint);
    lv_bar_set_value(s_progress_bar, s_checkpoint + 1, LV_ANIM_ON);
}

static void overlay_show(void)
{
    if (s_overlay) lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void overlay_hide(void)
{
    if (s_overlay) lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

static void overlay_destroy(void)
{
    if (s_overlay) {
        lv_obj_del(s_overlay);
        s_overlay = NULL;
        s_inst_label = NULL;
        s_hint_label = NULL;
        s_progress_bar = NULL;
    }
}

/* ---- Task check timer ---- */

static void task_check_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!s_active || !s_task_mode) return;
    const checkpoint_t *cp = &checkpoints[s_checkpoint];
    if (cp->type != CHECKPOINT_WAIT_ACTION || !cp->is_done) return;
    if (cp->is_done()) {
        ESP_LOGI(TAG, "Task completed at checkpoint %d", s_checkpoint);

        /* Award points directly — the broadcast stubs in the simulator
           don't call points_store, so we must do it here. */
        if (cp->is_done == check_breathing_done) {
            int pts = points_store_award_breathing(3);
            inflate_demo_seeds(pts);
            ESP_LOGI(TAG, "Awarded %d seeds for breathing (demo 3.2x)", pts);
        } else if (cp->is_done == check_water_added) {
            app_state_t *s = app_state_get();
            int glasses = s->water.glasses - s_water_baseline;
            for (int i = 0; i < glasses; i++) {
                int pts = points_store_award_water(s->water.goal);
                inflate_demo_seeds(pts);
            }
            ESP_LOGI(TAG, "Awarded seeds for %d glasses of water (demo 3.2x)", glasses);
        } else if (cp->is_done == check_todo_toggled) {
            int pts = points_store_award_todo();
            inflate_demo_seeds(pts);
            ESP_LOGI(TAG, "Awarded %d seeds for todo (demo 3.2x)", pts);
        } else if (cp->is_done == check_sleep_ended) {
            int pts = points_store_award_sleep_tracked(420);
            inflate_demo_seeds(pts);
            ESP_LOGI(TAG, "Awarded %d seeds for sleep (demo 3.2x)", pts);
        } else if (cp->is_done == check_timer_started) {
            int pts = points_store_award_focus();
            inflate_demo_seeds(pts);
            ESP_LOGI(TAG, "Awarded %d seeds for focus (demo 3.2x)", pts);
        }

        /* Exit task mode, hide overlay, advance to next checkpoint.
           go_to_checkpoint() will show the next instruction overlay
           and switch screens as needed. */
        s_task_mode = false;
        overlay_hide();
        uint8_t next = s_checkpoint + 1;
        if (next < CHECKPOINT_COUNT) {
            go_to_checkpoint(next);
        }
    }
}

/* ---- Backup / restore ---- */

static void backup_state(void)
{
    memset(&s_backup, 0, sizeof(s_backup));
    s_backup.valid = true;
    s_backup.previous_screen = ui_manager_get_current_screen();
    memcpy(&s_backup.app, app_state_get(), sizeof(app_state_t));
    memcpy(&s_backup.points, points_store_get_state(), sizeof(points_state_t));
    memcpy(&s_backup.sedentary, sedentary_store_get_state(), sizeof(sedentary_state_t));
    ESP_LOGI(TAG, "State backed up");
}

static void restore_state(void)
{
    if (!s_backup.valid) return;
    memcpy(app_state_get(), &s_backup.app, sizeof(app_state_t));
    memcpy(points_store_get_state(), &s_backup.points, sizeof(points_state_t));
    memcpy(sedentary_store_get_state(), &s_backup.sedentary, sizeof(sedentary_state_t));
    timer_store_init();
    ESP_LOGI(TAG, "State restored");
    s_backup.valid = false;
}

/* ---- Seed minimal state ---- */

static void seed_app_state_minimal(void)
{
    app_state_t *s = app_state_get();
    s->water.glasses = 0;
    s->water.goal = 8;

    s->todo_count = 3;
    s->next_todo_id = 4;
    s->todos[0].id = 1;
    strncpy(s->todos[0].text, "Read chapter 5", MAX_TODO_LEN);
    s->todos[0].done = false; s->todos[0].priority = 1;
    s->todos[0].order = 0; s->todos[0].points_awarded = false;
    s->todos[1].id = 2;
    strncpy(s->todos[1].text, "Write essay outline", MAX_TODO_LEN);
    s->todos[1].done = false; s->todos[1].priority = 2;
    s->todos[1].order = 1; s->todos[1].points_awarded = false;
    s->todos[2].id = 3;
    strncpy(s->todos[2].text, "Clean room", MAX_TODO_LEN);
    s->todos[2].done = false; s->todos[2].priority = 0;
    s->todos[2].order = 2; s->todos[2].points_awarded = false;

    s->preset_count = 3;
    s->next_preset_id = 4;
    s->presets[0].id = 1;
    strncpy(s->presets[0].name, "Pomodoro", MAX_NAME_LEN);
    s->presets[0].focus_ms = 25*60*1000; s->presets[0].break_ms = 5*60*1000;
    s->presets[0].is_pomodoro = true;
    s->presets[1].id = 2;
    strncpy(s->presets[1].name, "Quick Sprint", MAX_NAME_LEN);
    s->presets[1].focus_ms = 10*60*1000; s->presets[1].break_ms = 0;
    s->presets[1].is_pomodoro = false;
    s->presets[2].id = 3;
    strncpy(s->presets[2].name, "Deep Work", MAX_NAME_LEN);
    s->presets[2].focus_ms = 45*60*1000; s->presets[2].break_ms = 10*60*1000;
    s->presets[2].is_pomodoro = true;

    s->exercise_count = 2;
    strncpy(s->exercises[0].name, "Calm Box", MAX_NAME_LEN);
    s->exercises[0].inhale_ms = 4000; s->exercises[0].hold_ms = 4000;
    s->exercises[0].exhale_ms = 4000; s->exercises[0].hold2_ms = 4000;
    strncpy(s->exercises[1].name, "4-7-8 Relax", MAX_NAME_LEN);
    s->exercises[1].inhale_ms = 4000; s->exercises[1].hold_ms = 7000;
    s->exercises[1].exhale_ms = 8000; s->exercises[1].hold2_ms = 0;

    s->settings.brightness = 50;
    s->settings.volume = 50;
    s->settings.idle_timeout = 60;
    s->settings.reading_light = 0;
    s->timer.is_running = false;

    s_water_baseline = 0;
    s_todo_was_done = false;
    s_settings_baseline = 50;
    s_breathing_seen_active = false;
    s_sleep_seen_active = false;
}

static void seed_sleep_history(void)
{
    uint16_t minutes[] = {420, 450, 480, 360, 470, 440, 465};
    int16_t starts[] = {1320, 1300, 1290, 1380, 1310, 1340, 1300};
    sleep_store_set_history_entries(minutes, starts, 7);
}

static void seed_sedentary(void)
{
    sedentary_state_t *ss = sedentary_store_get_state();
    ss->enabled = true;
    ss->interval_min = 45;
    ss->running = false;
    ss->pending_alert = false;
    ss->snoozing = false;
    ss->breaks_today = 0;
}

static void inflate_demo_seeds(int base_pts)
{
    int extra = base_pts * 3;
    if (extra > 0) {
        points_store_admin_add(extra);
    }
}

static void seed_all(void)
{
    seed_app_state_minimal();
    points_store_admin_reset();
    points_store_set_goal(0, "Drink 8 glasses of water", GOAL_METRIC_WATER, 8);
    points_store_set_goal(1, "Complete a focus session", GOAL_METRIC_FOCUS, 1);
    points_store_set_goal(2, "Do 3 breathing exercises", GOAL_METRIC_BREATHING, 3);
    seed_sleep_history();
    seed_sedentary();
    session_store_set_breath_count(0);
    timer_store_init();
}

/* ---- Navigation ---- */

static void go_to_checkpoint(uint8_t idx)
{
    if (idx >= CHECKPOINT_COUNT) {
        demo_mode_stop(true);
        return;
    }

    s_checkpoint = idx;
    const checkpoint_t *cp = &checkpoints[s_checkpoint];

    if (s_task_mode) {
        s_task_mode = false;
    }

    screen_id_t cur = ui_manager_get_current_screen();
    if (cur != cp->screen) {
        ui_manager_switch_screen(cp->screen);
    }

    overlay_show();
    overlay_update();

    if (cp->type == CHECKPOINT_WAIT_ACTION) {
        /* Record baselines */
        app_state_t *s = app_state_get();
        s_water_baseline = s->water.glasses;
        s_settings_baseline = s->settings.brightness;
        s_breathing_seen_active = false;
        s_sleep_seen_active = false;
        s_todo_was_done = false;
        for (int i = 0; i < s->todo_count; i++) {
            if (s->todos[i].done) { s_todo_was_done = true; break; }
        }
        /* Enter task mode — overlay hides, input passes through */
        s_task_mode = true;
        overlay_hide();
    }

    ESP_LOGI(TAG, "Checkpoint %d/%d: screen %d, type %d",
             s_checkpoint + 1, (int)CHECKPOINT_COUNT, cp->screen, cp->type);
}

/* ---- Public API ---- */

bool demo_mode_can_start(char *reason, size_t reason_len)
{
    if (s_active) {
        if (reason) snprintf(reason, reason_len, "Demo already running");
        return false;
    }
    if (sleep_store_is_active()) {
        if (reason) snprintf(reason, reason_len, "Stop sleep tracking first");
        return false;
    }
    if (app_state_get()->timer.is_running) {
        if (reason) snprintf(reason, reason_len, "Stop the timer first");
        return false;
    }
    if (sedentary_store_get_state()->pending_alert) {
        if (reason) snprintf(reason, reason_len, "Dismiss the stretch break first");
        return false;
    }
    return true;
}

bool demo_mode_is_active(void) { return s_active; }
bool demo_mode_is_task_active(void) { return s_active && s_task_mode; }
uint8_t demo_mode_current_step(void) { return s_checkpoint; }
uint8_t demo_mode_total_steps(void) { return (uint8_t)CHECKPOINT_COUNT; }

screen_id_t demo_mode_current_screen(void)
{
    if (!s_active) return SCREEN_MENU;
    return checkpoints[s_checkpoint].screen;
}

bool demo_mode_start(void)
{
    if (s_active) return false;
    char reason[128] = {0};
    if (!demo_mode_can_start(reason, sizeof(reason))) return false;

    persistence_save();
    backup_state();
    persistence_set_suspended(true);
    seed_all();

    s_active = true;
    s_checkpoint = 0;
    s_task_mode = false;

    overlay_create();
    go_to_checkpoint(0);

    /* Start polling timer for task completion (100ms interval) */
    s_task_check_timer = lv_timer_create(task_check_cb, 100, NULL);

    ESP_LOGI(TAG, "Interactive demo started");
    return true;
}

void demo_mode_stop(bool restore_state_flag)
{
    if (!s_active) return;

    if (s_task_check_timer) {
        lv_timer_del(s_task_check_timer);
        s_task_check_timer = NULL;
    }

    overlay_destroy();
    s_task_mode = false;

    if (restore_state_flag) restore_state();
    persistence_set_suspended(false);

    s_active = false;
    s_checkpoint = 0;
    ui_manager_switch_screen(SCREEN_MENU);

    if (restore_state_flag) {
        app_state_broadcast_points_sync();
        app_state_broadcast_water_sync();
        app_state_broadcast_todo_sync();
        app_state_broadcast_timer_sync();
        app_state_broadcast_breathing_sync();
        app_state_broadcast_settings_sync();
    }
    ESP_LOGI(TAG, "Demo stopped");
}

void demo_mode_advance(void)
{
    if (!s_active) return;
    uint8_t next = s_checkpoint + 1;
    if (next >= CHECKPOINT_COUNT) {
        demo_mode_stop(true);
        return;
    }
    go_to_checkpoint(next);
}

void demo_mode_check_task_complete(void)
{
    if (!s_active || !s_task_mode) return;
    const checkpoint_t *cp = &checkpoints[s_checkpoint];
    if (cp->type != CHECKPOINT_WAIT_ACTION || !cp->is_done) return;
    if (cp->is_done()) {
        ESP_LOGI(TAG, "Task completed at checkpoint %d (encoder)", s_checkpoint);

        /* Award points directly (same as task_check_cb) */
        if (cp->is_done == check_breathing_done) {
            int pts = points_store_award_breathing(3);
            inflate_demo_seeds(pts);
        } else if (cp->is_done == check_water_added) {
            app_state_t *s = app_state_get();
            int glasses = s->water.glasses - s_water_baseline;
            for (int i = 0; i < glasses; i++) {
                int pts = points_store_award_water(s->water.goal);
                inflate_demo_seeds(pts);
            }
        } else if (cp->is_done == check_todo_toggled) {
            int pts = points_store_award_todo();
            inflate_demo_seeds(pts);
        } else if (cp->is_done == check_sleep_ended) {
            int pts = points_store_award_sleep_tracked(420);
            inflate_demo_seeds(pts);
        } else if (cp->is_done == check_timer_started) {
            int pts = points_store_award_focus();
            inflate_demo_seeds(pts);
        }

        s_task_mode = false;
        overlay_hide();
        uint8_t next = s_checkpoint + 1;
        if (next < CHECKPOINT_COUNT) {
            go_to_checkpoint(next);
        }
    }
}
