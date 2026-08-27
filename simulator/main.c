#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>
#include <unistd.h>

#include "lvgl.h"
#include "SDL.h"
#include "SDL_Driver.h"
#include "esp_log.h"

#include "../main/display/studybud_theme.h"
#include "../main/display/ui_manager.h"
#include "../main/display/app_state.h"
#include "../main/display/utils/points_store.h"

static const char *TAG = "Simulator";

#define LONG_PRESS_MS 800

static const char *g_script = NULL;
static int g_script_pos = 0;
static bool g_script_pending_release = false;
static bool g_script_holding = false;
static uint32_t g_hold_start_ms = 0;
static uint32_t g_last_op_ms = 0;
static bool g_headless = false;

static void inject_encoder(int enc_diff, lv_indev_state_t state)
{
    lv_indev_data_t data;
    memset(&data, 0, sizeof(data));
    data.enc_diff = enc_diff;
    data.state = state;
    ui_manager_encoder_event(&data);
}

static int g_shot_num = 0;

static void run_script_op(void)
{
    char op = g_script[g_script_pos++];
    switch (op) {
    case 'r': inject_encoder(1, LV_INDEV_STATE_REL); break;
    case 'l': inject_encoder(-1, LV_INDEV_STATE_REL); break;
    case 'p':
        inject_encoder(0, LV_INDEV_STATE_PR);
        g_script_pending_release = true;
        break;
    case 'h':
        inject_encoder(0, LV_INDEV_STATE_PR);
        g_script_holding = true;
        g_hold_start_ms = lv_tick_get();
        break;
    case 's': {
        char path[64];
        snprintf(path, sizeof(path), "/tmp/studybud_%03d.bmp", g_shot_num++);
        sdl_driver_screenshot(path);
        break;
    }
    case 'w':
        g_last_op_ms = lv_tick_get() + 1500;
        break;
    default: break;
    }
}

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--script=", 9) == 0) {
            g_script = argv[i] + 9;
        }
        if (strcmp(argv[i], "--headless") == 0) {
            g_headless = true;
        }
    }

    printf("=== StudyBud LVGL Simulator ===\n");
    if (g_headless) printf("  Mode: HEADLESS (no display)\n");
    printf("Controls:\n");
    printf("  Left/Right arrows = Encoder rotate\n");
    printf("  Enter             = Encoder press\n");
    printf("  S                 = Save screenshot (bmp)\n");
    printf("  Escape / Q        = Quit\n");
    if (g_script) printf("  Scripted input: %s\n\n", g_script);
    else printf("\n");

    lv_init();
    if (g_headless) sdl_driver_set_headless(true);
    sdl_driver_init();

    if (g_script) {
        sdl_driver_set_script_mode(true);
    }

    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    char time_buf[8];
    strftime(time_buf, sizeof(time_buf), "%H:%M", tm_info);
    ESP_LOGI(TAG, "Current time: %s", time_buf);

    app_state_init(NULL);

    /* Seed demo todos so the simulator is not blank on start */
    {
        app_state_t *st = app_state_get();
        struct { const char *text; int priority; bool done; } demo_todos[] = {
            { "Review maths notes",        2, false },
            { "Complete physics homework", 1, false },
            { "Read English chapter",      0, false },
            { "Organise study desk",       3, true  },
            { "Print assignment draft",    0, false },
            { "Email teacher about extension", 1, false },
        };
        int n = sizeof(demo_todos) / sizeof(demo_todos[0]);
        for (int i = 0; i < n; i++) {
            todo_item_t *t = &st->todos[st->todo_count];
            t->id = st->next_todo_id++;
            strncpy(t->text, demo_todos[i].text, MAX_TODO_LEN - 1);
            t->text[MAX_TODO_LEN - 1] = '\0';
            t->done = demo_todos[i].done;
            t->priority = demo_todos[i].priority;
            t->order = st->todo_count;
            t->points_awarded = demo_todos[i].done;
            st->todo_count++;
        }
        printf("[sim] Seeded %d demo todos\n", n);
    }

    /* Seed demo water, points, and timer presets */
    {
        app_state_t *st = app_state_get();

        st->water.glasses = 4;
        st->water.goal = 8;

        st->presets[0].id = 1;
        strncpy(st->presets[0].name, "Pomodoro", MAX_NAME_LEN - 1);
        st->presets[0].focus_ms = 25 * 60 * 1000;
        st->presets[0].break_ms = 5 * 60 * 1000;
        st->presets[0].is_pomodoro = true;

        st->presets[1].id = 2;
        strncpy(st->presets[1].name, "Deep Work", MAX_NAME_LEN - 1);
        st->presets[1].focus_ms = 50 * 60 * 1000;
        st->presets[1].break_ms = 10 * 60 * 1000;
        st->presets[1].is_pomodoro = false;

        st->presets[2].id = 3;
        strncpy(st->presets[2].name, "Quick Review", MAX_NAME_LEN - 1);
        st->presets[2].focus_ms = 15 * 60 * 1000;
        st->presets[2].break_ms = 3 * 60 * 1000;
        st->presets[2].is_pomodoro = false;

        st->preset_count = 3;
        st->next_preset_id = 4;

        points_store_init();
        points_state_t *ps = points_store_get_state();
        ps->total_points = 185;
        ps->today_points = 35;
        strncpy(ps->day_key, "2026-08-27", sizeof(ps->day_key) - 1);
        ps->todos_done_today = 1;
        ps->water_today = 4;
        ps->breathing_today = 2;

        ps->streaks[STREAK_FOCUS].streak = 3;
        ps->streaks[STREAK_WATER].streak = 5;
        ps->streaks[STREAK_BREATHING].streak = 2;

        strncpy(ps->goals[0].label, "Drink 8 glasses", MAX_GOAL_LEN - 1);
        ps->goals[0].metric = GOAL_METRIC_WATER;
        ps->goals[0].target = 8;
        ps->goals[0].done = false;

        strncpy(ps->goals[1].label, "Complete 1 focus session", MAX_GOAL_LEN - 1);
        ps->goals[1].metric = GOAL_METRIC_FOCUS;
        ps->goals[1].target = 1;
        ps->goals[1].done = false;

        strncpy(ps->goals[2].label, "Do a breathing exercise", MAX_GOAL_LEN - 1);
        ps->goals[2].metric = GOAL_METRIC_BREATHING;
        ps->goals[2].target = 1;
        ps->goals[2].done = false;

        printf("[sim] Seeded water (%d/%d), points (%d total, %d today), 3 presets, 3 daily goals\n",
               st->water.glasses, st->water.goal, ps->total_points, ps->today_points);
    }

    ui_manager_init();

    ESP_LOGI(TAG, "Starting main loop");

    while (1) {
        uint32_t ms_delay = lv_timer_handler();
        sdl_driver_present();

        uint32_t now = lv_tick_get();
        bool script_done = g_script && g_script_pos >= (int)strlen(g_script);

        if (g_script && !script_done) {
            if (g_script_holding) {
                if (now - g_hold_start_ms >= LONG_PRESS_MS + 100) {
                    inject_encoder(0, LV_INDEV_STATE_REL);
                    g_script_holding = false;
                    g_last_op_ms = now;
                    printf("[script] rel (long)\n"); fflush(stdout);
                } else {
                    inject_encoder(0, LV_INDEV_STATE_PR);
                }
            } else if (g_script_pending_release) {
                inject_encoder(0, LV_INDEV_STATE_REL);
                g_script_pending_release = false;
                g_last_op_ms = now;
                printf("[script] rel\n"); fflush(stdout);
            } else if (now - g_last_op_ms >= 500) {
                printf("[script] op=%c pos=%d t=%lu\n", g_script[g_script_pos], g_script_pos,
                       (unsigned long)now); fflush(stdout);
                run_script_op();
                g_last_op_ms = now;
            }
        }

        if (g_headless && g_script && script_done) {
            printf("[headless] Script complete, exiting.\n");
            break;
        }

        if (g_headless) {
            usleep((ms_delay < 5 ? 5 : ms_delay) * 1000);
        } else {
            SDL_Delay(ms_delay < 5 ? 5 : ms_delay);
        }
    }

    return 0;
}
