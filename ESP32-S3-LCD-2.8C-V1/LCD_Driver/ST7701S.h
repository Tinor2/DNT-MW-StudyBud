#pragma once

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_err.h"
#include "esp_log.h"
#include "TCA9554PWR.h"

#define LCD_MOSI                   1
#define LCD_SCLK                   2
#define LCD_CS                     -1

#define EXAMPLE_LCD_H_RES          480
#define EXAMPLE_LCD_V_RES          480
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ (18 * 1000 * 1000)

#define EXAMPLE_PIN_NUM_BK_LIGHT   6
#define EXAMPLE_PIN_NUM_HSYNC      38
#define EXAMPLE_PIN_NUM_VSYNC      39
#define EXAMPLE_PIN_NUM_DE         40
#define EXAMPLE_PIN_NUM_PCLK       41
#define EXAMPLE_PIN_NUM_DATA0      5
#define EXAMPLE_PIN_NUM_DATA1      45
#define EXAMPLE_PIN_NUM_DATA2      48
#define EXAMPLE_PIN_NUM_DATA3      47
#define EXAMPLE_PIN_NUM_DATA4      21
#define EXAMPLE_PIN_NUM_DATA5      14
#define EXAMPLE_PIN_NUM_DATA6      13
#define EXAMPLE_PIN_NUM_DATA7      12
#define EXAMPLE_PIN_NUM_DATA8      11
#define EXAMPLE_PIN_NUM_DATA9      10
#define EXAMPLE_PIN_NUM_DATA10     9
#define EXAMPLE_PIN_NUM_DATA11     46
#define EXAMPLE_PIN_NUM_DATA12     3
#define EXAMPLE_PIN_NUM_DATA13     8
#define EXAMPLE_PIN_NUM_DATA14     18
#define EXAMPLE_PIN_NUM_DATA15     17
#define EXAMPLE_PIN_NUM_DISP_EN    -1

#define LEDC_TIMER                 LEDC_TIMER_0
#define LEDC_MODE                  LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL               LEDC_CHANNEL_0
#define LEDC_DUTY_RES              LEDC_TIMER_13_BIT
#define LEDC_FREQUENCY             4000

typedef struct {
    uint8_t method_select;
    spi_device_handle_t spi_device;
    spi_bus_config_t spi_io_config_t;
    spi_device_interface_config_t st7701s_protocol_config_t;
} ST7701S;

typedef ST7701S *ST7701S_handle;

void LCD_Init(void);
void Backlight_Init(void);
void Set_Backlight(uint8_t Light);
extern esp_lcd_panel_handle_t panel_handle;
