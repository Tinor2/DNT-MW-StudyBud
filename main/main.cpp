#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "I2C_Driver/I2C_Driver.h"
#include "EXIO/TCA9554PWR.h"
#include "LCD_Driver/ST7701S.h"
#include "display/lvgl_driver/LVGL_Driver.h"
#include "display/ui_manager.h"

static const char *TAG = "StudyBud";

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting StudyBud");

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

    ESP_LOGI(TAG, "StudyBud ready");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        LVGL_Driver_loop();
    }
}
