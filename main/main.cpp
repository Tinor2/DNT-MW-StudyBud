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
#include "networking/wifi_manager.h"
#include "networking/web_server.h"

static const char *TAG = "StudyBud";

static const wifi_credential_t wifi_credentials[] = {
    {
        .ssid = "Optus_0253C6",
        .auth_mode = WIFI_CRED_PSK,
        .password = "chumssawerMg9QT",
        .identity = NULL,
        .username = NULL,
        .ent_password = NULL,
    },
    {
        .ssid = "RJA-BYOD",
        .auth_mode = WIFI_CRED_ENTERPRISE,
        .password = NULL,
        .identity = "rbhandari",
        .username = "rbhandari",
        .ent_password = "Rusts@il#427",
    },
};

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

    ESP_LOGI(TAG, "Initializing WiFi...");
    int wifi_count = sizeof(wifi_credentials) / sizeof(wifi_credentials[0]);
    esp_err_t wifi_ret = wifi_manager_init(wifi_credentials, wifi_count);
    if (wifi_ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connected");
        web_server_init();
    } else {
        ESP_LOGW(TAG, "WiFi failed, continuing without network");
    }

    ESP_LOGI(TAG, "StudyBud ready");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        LVGL_Driver_loop();
    }
}
