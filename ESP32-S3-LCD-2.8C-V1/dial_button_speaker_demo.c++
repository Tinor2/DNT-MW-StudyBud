#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

// ---------- Rotary Encoder Pins ----------
#define ENCODER_CLK_GPIO  (gpio_num_t)43
#define ENCODER_DT_GPIO   (gpio_num_t)44
#define ENCODER_SW_GPIO   (gpio_num_t)0

// ---------- Speaker Pin ----------
#define SPEAKER_GPIO      20

// ---------- Encoder Variables ----------
static int encoderPosition = 0;
static int lastCLKState;

// ---------- Button Variables ----------
static bool lastButtonState = true;

// ---------- PWM Audio Settings ----------
#define AUDIO_LEDC_TIMER    LEDC_TIMER_0
#define AUDIO_LEDC_CHANNEL  LEDC_CHANNEL_0
#define AUDIO_LEDC_MODE     LEDC_LOW_SPEED_MODE
#define AUDIO_RESOLUTION    LEDC_TIMER_8_BIT

static const char *TAG = "demo";

static void playTone(int frequency, int duration_ms)
{
    ledc_set_freq(AUDIO_LEDC_MODE, AUDIO_LEDC_TIMER, frequency);
    ledc_set_duty(AUDIO_LEDC_MODE, AUDIO_LEDC_CHANNEL, 128);
    ledc_update_duty(AUDIO_LEDC_MODE, AUDIO_LEDC_CHANNEL);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ledc_set_duty(AUDIO_LEDC_MODE, AUDIO_LEDC_CHANNEL, 0);
    ledc_update_duty(AUDIO_LEDC_MODE, AUDIO_LEDC_CHANNEL);
}

void app_main(void)
{
    // ---------- Configure Encoder & Button GPIOs ----------
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << ENCODER_CLK_GPIO) |
                        (1ULL << ENCODER_DT_GPIO)  |
                        (1ULL << ENCODER_SW_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    lastCLKState = gpio_get_level(ENCODER_CLK_GPIO);

    // ---------- Configure LED PWM for Speaker ----------
    ledc_timer_config_t ledc_timer = {
        .speed_mode      = AUDIO_LEDC_MODE,
        .duty_resolution = AUDIO_RESOLUTION,
        .timer_num       = AUDIO_LEDC_TIMER,
        .freq_hz         = 2000,
        .clk_cfg         = LEDC_AUTO_CLK,
        .deconfigure     = false,
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .gpio_num   = SPEAKER_GPIO,
        .speed_mode = AUDIO_LEDC_MODE,
        .channel    = AUDIO_LEDC_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = AUDIO_LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags      = { .output_invert = 0 },
        .deconfigure = false,
    };
    ledc_channel_config(&ledc_channel);

    ESP_LOGI(TAG, "System Ready");

    while (1) {
        // ==========================
        // ROTARY ENCODER
        // ==========================
        int currentCLKState = gpio_get_level(ENCODER_CLK_GPIO);

        if (currentCLKState != lastCLKState && currentCLKState == 1) {
            if (gpio_get_level(ENCODER_DT_GPIO) != currentCLKState) {
                encoderPosition++;
                ESP_LOGI(TAG, "Clockwise: %d", encoderPosition);
                playTone(1500, 50);
            } else {
                encoderPosition--;
                ESP_LOGI(TAG, "Counter-Clockwise: %d", encoderPosition);
                playTone(800, 50);
            }
        }

        lastCLKState = currentCLKState;

        // ==========================
        // PUSH BUTTON
        // ==========================
        bool currentButtonState = gpio_get_level(ENCODER_SW_GPIO);

        if (currentButtonState == 0 && lastButtonState == 1) {
            ESP_LOGI(TAG, "Button Pressed");
            playTone(2000, 200);
        }

        lastButtonState = currentButtonState;

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}
