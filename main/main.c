#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "studybud";

#define ENCODER_CLK  GPIO_NUM_4
#define ENCODER_DT   GPIO_NUM_16
#define ENCODER_SW   GPIO_NUM_0

#define SPEAKER_PIN  GPIO_NUM_20

static int encoder_position = 0;
static int last_clk_state;
static bool last_button_state = true;

#define LEDC_TIMER     LEDC_TIMER_1
#define LEDC_CHANNEL   LEDC_CHANNEL_1
#define LEDC_MODE      LEDC_LOW_SPEED_MODE
#define LEDC_RESOLUTION LEDC_TIMER_8_BIT

static void play_tone(int frequency, int duration_ms)
{
    ledc_set_freq(LEDC_MODE, LEDC_TIMER, frequency);
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 128);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);

    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

static void configure_gpio(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ENCODER_CLK) | (1ULL << ENCODER_DT) | (1ULL << ENCODER_SW),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

static void configure_ledc(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = LEDC_RESOLUTION,
        .freq_hz = 2000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t channel_conf = {
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = SPEAKER_PIN,
        .duty = 0,
        .hpoint = 0,
    };
    ledc_channel_config(&channel_conf);
}

void app_main(void)
{
    configure_gpio();
    configure_ledc();

    last_clk_state = gpio_get_level(ENCODER_CLK);

    ESP_LOGI(TAG, "System Ready");

    while (1) {
        int current_clk_state = gpio_get_level(ENCODER_CLK);

        if (current_clk_state != last_clk_state && current_clk_state == 1) {
            if (gpio_get_level(ENCODER_DT) != current_clk_state) {
                encoder_position++;
                ESP_LOGI(TAG, "Clockwise: %d", encoder_position);
                play_tone(1500, 50);
            } else {
                encoder_position--;
                ESP_LOGI(TAG, "Counter-Clockwise: %d", encoder_position);
                play_tone(800, 50);
            }
        }

        last_clk_state = current_clk_state;

        bool current_button_state = gpio_get_level(ENCODER_SW);

        if (current_button_state == 0 && last_button_state == 1) {
            ESP_LOGI(TAG, "Button Pressed");
            play_tone(2000, 200);
        }

        last_button_state = current_button_state;

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
