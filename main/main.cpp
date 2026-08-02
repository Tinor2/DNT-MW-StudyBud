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
#include "display/utils/persistence.h"
#include "display/utils/sleep_store.h"
#include "display/utils/session_store.h"
#include "display/utils/points_store.h"
#include "networking/wifi_manager.h"
#include "networking/web_server.h"
#include "networking/app_state.h"

static const char *TAG = "StudyBud";
#define WIFI_SSID      "Optus_0253C6"
// #define WIFI_SSID      "RJA-BYOD"
#define WIFI_PASSWORD  "chumssawerMg9QT"
// #define WIFI_PASSWORD  "Rusts@il#427"

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting StudyBud");

    app_state_init(NULL);

    ESP_LOGI(TAG, "Initializing WiFi...");
    esp_err_t wifi_ret = wifi_manager_init(WIFI_SSID, WIFI_PASSWORD);
    if (wifi_ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connected, starting web server...");
        web_server_init();
    } else {
        ESP_LOGW(TAG, "WiFi failed, continuing without network");
    }

    ESP_LOGI(TAG, "Initializing I2C...");
    I2C_Init();

    ESP_LOGI(TAG, "Initializing EXIO (TCA9554PWR)...");
    EXIO_Init();

    ESP_LOGI(TAG, "Initializing LCD...");
    LCD_Init();
    vTaskDelay(pdMS_TO_TICKS(200));

    ESP_LOGI(TAG, "Initializing LVGL...");
    LVGL_Driver_init();

    ESP_LOGI(TAG, "Initializing stores...");
    sleep_store_init();
    sleep_store_seed_demo();
    session_store_init();
    points_store_init();

    ESP_LOGI(TAG, "Initializing SD card persistence...");
    persistence_init();
    Set_Backlight((uint8_t)app_state_get()->settings.brightness);

    ESP_LOGI(TAG, "Initializing UI Manager...");
    ui_manager_init();

    ESP_LOGI(TAG, "StudyBud ready");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        LVGL_Driver_loop();
    }
}
