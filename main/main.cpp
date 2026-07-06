#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
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
    esp_lcd_panel_draw_bitmap(panel_handle, x1, y1, x2, y2, buf);
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

extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "Starting display test (no peripherals needed)");

    ESP_LOGI(TAG, "Initializing I2C...");
    I2C_Init();

    ESP_LOGI(TAG, "Initializing EXIO (TCA9554PWR)...");
    EXIO_Init();

    ESP_LOGI(TAG, "Initializing LCD...");
    LCD_Init();

    ESP_LOGI(TAG, "Display initialized, drawing pattern...");
    draw_test_pattern();

    uint32_t counter = 0;
    while (1) {
        counter++;
        ESP_LOGI(TAG, "Screen alive - count: %lu", counter);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
