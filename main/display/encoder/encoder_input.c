#include "encoder_input.h"
#include "lvgl.h"
#include "driver/gpio.h"
#include "driver/pulse_cnt.h"
#include "esp_log.h"
#include "esp_task_wdt.h"

static const char *TAG = "encoder";

#define ENCODER_CLK  GPIO_NUM_19
#define ENCODER_DT   GPIO_NUM_20
#define ENCODER_SW   GPIO_NUM_0

static pcnt_unit_handle_t pcnt_unit = NULL;
static bool last_button_state = true;
static lv_indev_state_t enc_state = LV_INDEV_STATE_REL;
static int last_step = 0;

void encoder_init(void)
{
    gpio_config_t btn_conf = {
        .pin_bit_mask = (1ULL << ENCODER_SW),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&btn_conf);

    pcnt_unit_config_t unit_cfg = {
        .low_limit  = -32768,
        .high_limit =  32767,
        .clk_src   = PCNT_CLK_SRC_DEFAULT,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_cfg, &pcnt_unit));

    pcnt_glitch_filter_config_t filter_cfg = {
        .max_glitch_ns = 10000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_cfg));

    pcnt_chan_config_t chan_a_cfg = {
        .edge_gpio_num  = ENCODER_CLK,
        .level_gpio_num = ENCODER_DT,
    };
    pcnt_channel_handle_t chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_cfg, &chan_a));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_a,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_INVERSE,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP));

    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));

    last_button_state = gpio_get_level(ENCODER_SW) == 0;
    ESP_LOGI(TAG, "PCNT encoder initialized (CLK=19, DT=20, SW=0, glitch filter=10us)");
}

void encoder_input_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    int count = 0;
    ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit, &count));

    int step = count / 2;

    if (step != last_step) {
        data->enc_diff = step - last_step;
        last_step = step;
    } else {
        data->enc_diff = 0;
    }

    bool current_button = gpio_get_level(ENCODER_SW) == 0;
    if (current_button && !last_button_state) {
        enc_state = LV_INDEV_STATE_PR;
    } else if (!current_button && last_button_state) {
        enc_state = LV_INDEV_STATE_REL;
    }
    last_button_state = current_button;

    data->state = enc_state;
}
