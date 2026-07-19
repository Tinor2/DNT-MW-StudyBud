#include "encoder_input.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "display/ui_manager.h"

static const char *TAG = "Encoder";

#define ENCODER_CLK  GPIO_NUM_19
#define ENCODER_DT   GPIO_NUM_20
#define ENCODER_SW   GPIO_NUM_0

static int last_clk_state = 0;
static bool last_button_state = true;
static lv_indev_state_t enc_state = LV_INDEV_STATE_REL;

void encoder_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ENCODER_CLK) | (1ULL << ENCODER_DT) | (1ULL << ENCODER_SW),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    last_clk_state = gpio_get_level(ENCODER_CLK);
    ESP_LOGI(TAG, "Encoder initialized: CLK=%d DT=%d SW=%d",
             ENCODER_CLK, ENCODER_DT, ENCODER_SW);
}

static void encoder_read_hw(lv_indev_data_t *data)
{
    int current_clk = gpio_get_level(ENCODER_CLK);
    bool current_button = gpio_get_level(ENCODER_SW) == 0;

    /* Detect rotation on ANY CLK edge, not just rising */
    if (current_clk != last_clk_state) {
        if (gpio_get_level(ENCODER_DT) != current_clk) {
            data->enc_diff = 1;
            ESP_LOGI(TAG, "ROT CW");
        } else {
            data->enc_diff = -1;
            ESP_LOGI(TAG, "ROT CCW");
        }
    } else {
        data->enc_diff = 0;
    }
    last_clk_state = current_clk;

    /* Detect button press/release */
    if (current_button && !last_button_state) {
        enc_state = LV_INDEV_STATE_PR;
        ESP_LOGI(TAG, "BUTTON PRESS");
    } else if (!current_button && last_button_state) {
        enc_state = LV_INDEV_STATE_REL;
        ESP_LOGI(TAG, "BUTTON RELEASE");
    }
    last_button_state = current_button;

    data->state = enc_state;
}

void encoder_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    /* Read hardware */
    encoder_read_hw(data);

    /* Forward directly to UI manager for custom navigation */
    ui_manager_encoder_event(data);

    /* Prevent LVGL from also processing this input */
    data->enc_diff = 0;
    data->state = LV_INDEV_STATE_REL;
}
