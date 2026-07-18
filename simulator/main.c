#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <time.h>

#include "lvgl.h"
#include "SDL.h"
#include "SDL_Driver.h"
#include "esp_log.h"

#include "../main/display/studybud_theme.h"
#include "../main/display/ui_manager.h"

static const char *TAG = "Simulator";

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    printf("=== StudyBud LVGL Simulator ===\n");
    printf("Controls:\n");
    printf("  Left/Right arrows = Encoder rotate\n");
    printf("  Enter             = Encoder press\n");
    printf("  Escape / Q        = Quit\n\n");

    lv_init();
    sdl_driver_init();

    time_t now;
    time(&now);
    struct tm *tm_info = localtime(&now);
    char time_buf[8];
    strftime(time_buf, sizeof(time_buf), "%H:%M", tm_info);
    ESP_LOGI(TAG, "Current time: %s", time_buf);

    ui_manager_init();

    ESP_LOGI(TAG, "Starting main loop");

    while (1) {
        uint32_t ms_delay = lv_timer_handler();
        sdl_driver_present();
        SDL_Delay(ms_delay < 5 ? 5 : ms_delay);
    }

    return 0;
}
