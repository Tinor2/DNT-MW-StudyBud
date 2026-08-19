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
