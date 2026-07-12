#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_cache.h"
#include "I2C_Driver/I2C_Driver.h"
#include "EXIO/TCA9554PWR.h"
#include "LCD_Driver/ST7701S.h"

static const char *TAG = "display_test";

#define H_RES  EXAMPLE_LCD_H_RES
#define V_RES  EXAMPLE_LCD_V_RES

static void fill_area_rgb565(int x1, int y1, int x2, int y2, uint16_t color)
{
    size_t w = x2 - x1;
    size_t h = y2 - y1;
    size_t buf_size = w * h * 2;
    uint16_t *buf = (uint16_t *)heap_caps_malloc(buf_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!buf) {
        ESP_LOGE(TAG, "Failed to allocate %zu bytes for fill buffer", buf_size);
        return;
    }
    for (size_t i = 0; i < w * h; i++) {
        buf[i] = color;
    }
    esp_err_t ret = esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2, y2, buf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "draw_bitmap failed: %s", esp_err_to_name(ret));
    }
    free(buf);
}

static void draw_test_pattern(void)
{
    ESP_LOGI(TAG, "Drawing test pattern...");

    fill_area_rgb565(0,   0,   240, 240, 0xF800); // Red
    fill_area_rgb565(240, 0,   480, 240, 0x07E0); // Green
    fill_area_rgb565(0,   240, 240, 480, 0x001F); // Blue
    fill_area_rgb565(240, 240, 480, 480, 0xFFFF); // White

    ESP_LOGI(TAG, "Test pattern drawn");
}

static void fill_screen_direct(uint16_t color)
{
    void *fb0 = NULL, *fb1 = NULL;
    esp_err_t ret = esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 1, &fb0, &fb1);
    if (ret != ESP_OK || !fb0) {
        ESP_LOGE(TAG, "get_frame_buffer failed: %s", esp_err_to_name(ret));
        return;
    }
    size_t pixel_count = H_RES * V_RES;
    uint16_t *buf = (uint16_t *)fb0;
    for (size_t i = 0; i < pixel_count; i++) {
        buf[i] = color;
    }

    esp_cache_msync(fb0, pixel_count * 2, ESP_CACHE_MSYNC_FLAG_DIR_C2M);
    ESP_LOGI(TAG, "Direct fill complete");
}

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting display test (no peripherals needed)");

    ESP_LOGI(TAG, "Initializing I2C...");
    I2C_Init();

    ESP_LOGI(TAG, "Initializing EXIO (TCA9554PWR)...");
    EXIO_Init();

    ESP_LOGI(TAG, "Initializing LCD...");
    LCD_Init();

    vTaskDelay(pdMS_TO_TICKS(200));

    ESP_LOGI(TAG, "Drawing test pattern via draw_bitmap...");
    draw_test_pattern();

    uint32_t counter = 0;
    while (1) {
        counter++;
        ESP_LOGI(TAG, "Screen alive - count: %lu", counter);
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (counter == 10) {
            ESP_LOGI(TAG, "Trying direct frame buffer fill (RED)...");
            fill_screen_direct(0xF800);
        }
        if (counter == 15) {
            ESP_LOGI(TAG, "Trying direct frame buffer fill (BLUE)...");
            fill_screen_direct(0x001F);
        }
    }
}
