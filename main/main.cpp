#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "I2C_Driver/I2C_Driver.h"
#include "EXIO/TCA9554PWR.h"
#include "LCD_Driver/ST7701S.h"
#include "display/lvgl_driver/LVGL_Driver.h"
#include "display/ui_manager.h"

static const char *TAG = "StudyBud";

static const char *screen_names[] = {
    "Home", "Menu", "Timer", "TimerPresets",
    "Todos", "Water", "Breathing", "Sedentary",
    "Settings", "Notifications"
};

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  StudyBud LVGL Build Starting");
    ESP_LOGI(TAG, "========================================");

    ESP_LOGI(TAG, "Initializing I2C...");
    I2C_Init();

    ESP_LOGI(TAG, "Initializing EXIO (TCA9554PWR)...");
    EXIO_Init();

    ESP_LOGI(TAG, "Initializing LCD...");
    LCD_Init();
    vTaskDelay(pdMS_TO_TICKS(200));

    ESP_LOGI(TAG, "Initializing LVGL...");
    LVGL_Driver_init();

    ESP_LOGI(TAG, "Initializing UI Manager...");
    ui_manager_init();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  StudyBud ready — entering main loop");
    ESP_LOGI(TAG, "  Rotate encoder to navigate menus");
    ESP_LOGI(TAG, "  Press encoder to select");
    ESP_LOGI(TAG, "  Long press to go back to menu");
    ESP_LOGI(TAG, "========================================");

    uint32_t loop_count = 0;
    uint32_t last_status_tick = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        LVGL_Driver_loop();

        loop_count++;

        /* Print status every 1 second */
        uint32_t now = lv_tick_get();
        if (now - last_status_tick >= 1000) {
            last_status_tick = now;
            screen_id_t scr = ui_manager_get_current_screen();
            const char *name = (scr < SCREEN_COUNT) ? screen_names[scr] : "???";
            ESP_LOGI(TAG, "STATUS | screen=%s(%d) | tick=%lu | loops=%lu",
                     name, (int)scr, (unsigned long)now, (unsigned long)loop_count);
        }
    }
}
