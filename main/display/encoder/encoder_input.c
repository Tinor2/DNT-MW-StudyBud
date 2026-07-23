#include "encoder_input.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define ENCODER_CLK  GPIO_NUM_4
#define ENCODER_DT   GPIO_NUM_16
#define ENCODER_SW   GPIO_NUM_0

static int encoder_position = 0;
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
}

void encoder_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    int current_clk = gpio_get_level(ENCODER_CLK);
    bool current_button = gpio_get_level(ENCODER_SW) == 0;

    /* Detect rotation on CLK rising edge */
    if (current_clk != last_clk_state && current_clk == 1) {
        if (gpio_get_level(ENCODER_DT) != current_clk) {
            encoder_position++;
            data->enc_diff = 1;
        } else {
            encoder_position--;
            data->enc_diff = -1;
        }
    } else {
        data->enc_diff = 0;
    }
    last_clk_state = current_clk;

    /* Detect button press/release */
    if (current_button && !last_button_state) {
        enc_state = LV_INDEV_STATE_PR;
    } else if (!current_button && last_button_state) {
        enc_state = LV_INDEV_STATE_REL;
    }
    last_button_state = current_button;

    data->state = enc_state;
}
