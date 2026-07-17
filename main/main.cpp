#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_task_wdt.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_adc/adc_oneshot.h"
#include "I2C_Driver/I2C_Driver.h"
#include "EXIO/TCA9554PWR.h"

static const char *TAG = "PERIPH_TEST";

#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0')

/* ── Encoder pins (from main.c) ── */
#define ENCODER_CLK     GPIO_NUM_4
#define ENCODER_DT      GPIO_NUM_16
#define ENCODER_SW      GPIO_NUM_0

/* ── Speaker LEDC (use TIMER_1/CHANNEL_1 to avoid conflict with backlight) ── */
#define SPEAKER_LEDC_TIMER   LEDC_TIMER_1
#define SPEAKER_LEDC_CHANNEL LEDC_CHANNEL_1
#define SPEAKER_LEDC_MODE    LEDC_LOW_SPEED_MODE
#define SPEAKER_GPIO         GPIO_NUM_20

/* ── Battery ADC ── */
#define BAT_ADC_CHANNEL  ADC_CHANNEL_3   /* GPIO 4 — note: conflicts with encoder CLK */

/* ── PCF85063 RTC (I2C address 0x51) ── */
#define RTC_ADDR  0x51
#define RTC_SEC   0x04
#define RTC_MIN   0x05
#define RTC_HOUR  0x06
#define RTC_DAY   0x07
#define RTC_WDAY  0x08
#define RTC_MON   0x09
#define RTC_YEAR  0x0A
#define RTC_CTRL1 0x00
#define RTC_CTRL2 0x01

/* ── QMI8658A Accelerometer (I2C address 0x6B) ── */
#define QMI_ADDR      0x6B
#define QMI_WHO_AM_I  0x00
#define QMI_REVISION  0x01
#define QMI_CTRL1     0x02
#define QMI_CTRL2     0x03
#define QMI_CTRL3     0x04
#define QMI_CTRL7     0x08
#define QMI_AX_L      0x35
#define QMI_GX_L      0x3B
#define QMI_TEMP_L    0x33

/* ── BCD helpers ── */
static uint8_t bcd2dec(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t dec2bcd(uint8_t dec) { return ((dec / 10) << 4) | (dec % 10); }

/* ═══════════════════════════════════════════════════
   TEST 1: I2C Bus Scan
   ═══════════════════════════════════════════════════ */
static void test_i2c_scan(void)
{
    ESP_LOGI(TAG, "══════ TEST 1: I2C Bus Scan ══════");
    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        uint8_t dummy;
        esp_err_t ret = I2C_Read(addr << 1, 0x00, &dummy, 1);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "  Device found at 0x%02X", addr);
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGW(TAG, "  No I2C devices found!");
    } else {
        ESP_LOGI(TAG, "  Total: %d device(s)", found);
    }
    ESP_LOGI(TAG, "  Expected: TCA9554PWR=0x20, PCF85063=0x51, QMI8658A=0x6B");
}

/* ═══════════════════════════════════════════════════
   TEST 2: EXIO (TCA9554PWR) — read pins, toggle buzzer
   ═══════════════════════════════════════════════════ */
static void test_exio(void)
{
    ESP_LOGI(TAG, "══════ TEST 2: EXIO (TCA9554PWR) ══════");

    /* Read all input pins */
    uint8_t all_inputs = Read_EXIOS();
    ESP_LOGI(TAG, "  All input pins: 0x%02X (binary: " BYTE_TO_BINARY_PATTERN ")",
             all_inputs, BYTE_TO_BINARY(all_inputs));

    for (int pin = 1; pin <= 8; pin++) {
        uint8_t val = Read_EXIO(pin);
        ESP_LOGI(TAG, "  EXIO%d = %d", pin, val);
    }

    /* Test buzzer (EXIO8): toggle on/off with beeps */
    ESP_LOGI(TAG, "  Testing buzzer (EXIO8)...");
    for (int i = 0; i < 3; i++) {
        Set_EXIO(TCA9554_EXIO8, 1);
        ESP_LOGI(TAG, "    Buzzer ON");
        vTaskDelay(pdMS_TO_TICKS(200));
        Set_EXIO(TCA9554_EXIO8, 0);
        ESP_LOGI(TAG, "    Buzzer OFF");
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    ESP_LOGI(TAG, "  Buzzer test complete");
}

/* ═══════════════════════════════════════════════════
   TEST 3: PCF85063 RTC — set time, read back
   ═══════════════════════════════════════════════════ */
static void test_rtc(void)
{
    ESP_LOGI(TAG, "══════ TEST 3: PCF85063 RTC ══════");

    /* Check if RTC is present on I2C */
    uint8_t ctrl1;
    esp_err_t ret = I2C_Read(RTC_ADDR << 1, RTC_CTRL1, &ctrl1, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "  RTC not found at 0x%02X! (err=%s)", RTC_ADDR, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "  RTC CTRL1 = 0x%02X", ctrl1);

    /* Read current time first */
    uint8_t secs[7];
    ret = I2C_Read(RTC_ADDR << 1, RTC_SEC, secs, 7);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  Current time: %02d-%02d-%02d %02d:%02d:%02d (dow=%d)",
                 bcd2dec(secs[5]) + 2000, bcd2dec(secs[4]), bcd2dec(secs[3]),
                 bcd2dec(secs[2]), bcd2dec(secs[1]), bcd2dec(secs[0]),
                 bcd2dec(secs[4]));
    }

    /* Set a known time: 2026-07-15 12:00:00 Tuesday */
    uint8_t set_time[] = {
        dec2bcd(0),   /* seconds */
        dec2bcd(0),   /* minutes */
        dec2bcd(12),  /* hours */
        dec2bcd(15),  /* day */
        dec2bcd(2),   /* weekday (2=Tuesday) */
        dec2bcd(7),   /* month */
        dec2bcd(56),  /* year (2026-1970=56) */
    };
    ret = I2C_Write(RTC_ADDR << 1, RTC_SEC, set_time, 7);
    ESP_LOGI(TAG, "  Set time: 2026-07-15 12:00:00 — %s", esp_err_to_name(ret));

    vTaskDelay(pdMS_TO_TICKS(1200));

    /* Read back */
    ret = I2C_Read(RTC_ADDR << 1, RTC_SEC, secs, 7);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "  Read back:  %02d-%02d-%02d %02d:%02d:%02d (dow=%d)",
                 bcd2dec(secs[5]) + 2000, bcd2dec(secs[4]), bcd2dec(secs[3]),
                 bcd2dec(secs[2]), bcd2dec(secs[1]), bcd2dec(secs[0]),
                 bcd2dec(secs[4]));
        if (bcd2dec(secs[0]) > 0) {
            ESP_LOGI(TAG, "  RTC is ticking — OK");
        } else {
            ESP_LOGW(TAG, "  RTC seconds did not advance");
        }
    }
}

/* ═══════════════════════════════════════════════════
   TEST 4: QMI8658A Accelerometer/Gyroscope
   ═══════════════════════════════════════════════════ */
static void test_imu(void)
{
    ESP_LOGI(TAG, "══════ TEST 4: QMI8658A IMU ══════");

    /* Read WHO_AM_I */
    uint8_t who;
    esp_err_t ret = I2C_Read(QMI_ADDR << 1, QMI_WHO_AM_I, &who, 1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "  QMI8658A not found at 0x%02X! (err=%s)", QMI_ADDR, esp_err_to_name(ret));
        return;
    }
    ESP_LOGI(TAG, "  WHO_AM_I = 0x%02X (expected 0x05)", who);

    uint8_t rev;
    I2C_Read(QMI_ADDR << 1, QMI_REVISION, &rev, 1);
    ESP_LOGI(TAG, "  REVISION = 0x%02X", rev);

    /* Enable accel + gyro (CTRL7 = 0x03) */
    uint8_t ctrl7_en = 0x03;
    I2C_Write(QMI_ADDR << 1, QMI_CTRL7, &ctrl7_en, 1);
    ESP_LOGI(TAG, "  Enabled accel + gyro");
    vTaskDelay(pdMS_TO_TICKS(100));

    /* Read 5 samples */
    for (int sample = 0; sample < 5; sample++) {
        uint8_t buf[6];

        /* Accel: 6 bytes from AX_L */
        I2C_Read(QMI_ADDR << 1, QMI_AX_L, buf, 6);
        int16_t ax_raw = (int16_t)(buf[1] << 8 | buf[0]);
        int16_t ay_raw = (int16_t)(buf[3] << 8 | buf[2]);
        int16_t az_raw = (int16_t)(buf[5] << 8 | buf[4]);
        float ax = ax_raw / 4096.0f;  /* ±4G range: 4096 LSB/g */
        float ay = ay_raw / 4096.0f;
        float az = az_raw / 4096.0f;

        /* Gyro: 6 bytes from GX_L */
        I2C_Read(QMI_ADDR << 1, QMI_GX_L, buf, 6);
        int16_t gx_raw = (int16_t)(buf[1] << 8 | buf[0]);
        int16_t gy_raw = (int16_t)(buf[3] << 8 | buf[2]);
        int16_t gz_raw = (int16_t)(buf[5] << 8 | buf[4]);
        float gx = gx_raw / 64.0f;   /* ±64DPS range: 64 LSB/dps */
        float gy = gy_raw / 64.0f;
        float gz = gz_raw / 64.0f;

        ESP_LOGI(TAG, "  [%d] Accel: %+6.2fg %+6.2fg %+6.2fg | Gyro: %+7.1f %+7.1f %+7.1f dps",
                 sample, ax, ay, az, gx, gy, gz);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

/* ═══════════════════════════════════════════════════
   TEST 8: Rotary Encoder — runs forever
   ═══════════════════════════════════════════════════ */
static void test_encoder(void)
{
    ESP_LOGI(TAG, "══════ TEST 8: Rotary Encoder ══════");
    ESP_LOGI(TAG, "  Rotate knob and press button — runs until reset");

    /* Remove this task from the watchdog so the infinite poll loop doesn't trigger it */
    esp_task_wdt_delete(NULL);

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << ENCODER_CLK) | (1ULL << ENCODER_DT) | (1ULL << ENCODER_SW);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    gpio_config(&io_conf);

    int position = 0;
    int last_clk = gpio_get_level(ENCODER_CLK);
    int button_was_pressed = 0;
    int rotations = 0;
    int presses = 0;

    while (1) {
        int clk = gpio_get_level(ENCODER_CLK);
        int dt  = gpio_get_level(ENCODER_DT);
        int sw  = gpio_get_level(ENCODER_SW);

        /* Detect rotation on rising edge of CLK */
        if (clk == 1 && last_clk == 0) {
            if (dt != clk) {
                position++;
                ESP_LOGI(TAG, "  CW  → pos=%d  (total rotations: %d)", position, ++rotations);
            } else {
                position--;
                ESP_LOGI(TAG, "  CCW → pos=%d  (total rotations: %d)", position, ++rotations);
            }
        }
        last_clk = clk;

        /* Detect button press (active low) */
        if (sw == 0 && !button_was_pressed) {
            ESP_LOGI(TAG, "  BUTTON PRESSED  (total presses: %d)", ++presses);
        }
        button_was_pressed = (sw == 0);

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* ═══════════════════════════════════════════════════
   TEST 6: Speaker — play a simple melody
   ═══════════════════════════════════════════════════ */
static void speaker_init(void)
{
    ledc_timer_config_t timer_conf = {};
    timer_conf.speed_mode = SPEAKER_LEDC_MODE;
    timer_conf.duty_resolution = LEDC_TIMER_8_BIT;
    timer_conf.timer_num = SPEAKER_LEDC_TIMER;
    timer_conf.freq_hz = 2000;
    timer_conf.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timer_conf);

    ledc_channel_config_t ch_conf = {};
    ch_conf.speed_mode = SPEAKER_LEDC_MODE;
    ch_conf.gpio_num = SPEAKER_GPIO;
    ch_conf.channel = SPEAKER_LEDC_CHANNEL;
    ch_conf.timer_sel = SPEAKER_LEDC_TIMER;
    ch_conf.duty = 0;
    ch_conf.hpoint = 0;
    ledc_channel_config(&ch_conf);
}

static void play_tone(int freq, int duration_ms)
{
    ledc_set_freq(SPEAKER_LEDC_MODE, SPEAKER_LEDC_TIMER, freq);
    ledc_set_duty(SPEAKER_LEDC_MODE, SPEAKER_LEDC_CHANNEL, 128);
    ledc_update_duty(SPEAKER_LEDC_MODE, SPEAKER_LEDC_CHANNEL);
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    ledc_set_duty(SPEAKER_LEDC_MODE, SPEAKER_LEDC_CHANNEL, 0);
    ledc_update_duty(SPEAKER_LEDC_MODE, SPEAKER_LEDC_CHANNEL);
    vTaskDelay(pdMS_TO_TICKS(50));
}

static void test_speaker(void)
{
    ESP_LOGI(TAG, "══════ TEST 5: Speaker (LEDC) ══════");
    speaker_init();

    ESP_LOGI(TAG, "  Playing ascending tones...");
    int notes[] = {262, 294, 330, 349, 392, 440, 494, 523};
    for (int i = 0; i < 8; i++) {
        ESP_LOGI(TAG, "    Note: %d Hz", notes[i]);
        play_tone(notes[i], 150);
    }

    ESP_LOGI(TAG, "  Playing descending tones...");
    for (int i = 7; i >= 0; i--) {
        play_tone(notes[i], 150);
    }

    ESP_LOGI(TAG, "  Speaker test complete");
}

/* ═══════════════════════════════════════════════════
   TEST 7: Battery ADC
   ═══════════════════════════════════════════════════ */
static void test_battery(void)
{
    ESP_LOGI(TAG, "══════ TEST 6: Battery ADC ══════");
    ESP_LOGI(TAG, "  NOTE: GPIO 4 shared with encoder — ADC runs first, then encoder takes over");

    adc_oneshot_unit_handle_t adc_handle;
    adc_oneshot_unit_init_cfg_t init_cfg = {};
    init_cfg.unit_id = ADC_UNIT_1;
    esp_err_t ret = adc_oneshot_new_unit(&init_cfg, &adc_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "  ADC init failed: %s", esp_err_to_name(ret));
        return;
    }

    adc_oneshot_chan_cfg_t chan_cfg = {};
    chan_cfg.bitwidth = ADC_BITWIDTH_DEFAULT;
    chan_cfg.atten = ADC_ATTEN_DB_12;
    adc_oneshot_config_channel(adc_handle, BAT_ADC_CHANNEL, &chan_cfg);

    /* Read 10 samples and average */
    int total = 0;
    for (int i = 0; i < 10; i++) {
        int raw;
        adc_oneshot_read(adc_handle, BAT_ADC_CHANNEL, &raw);
        total += raw;
        ESP_LOGI(TAG, "  Raw ADC[%d] = %d", i, raw);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    int avg = total / 10;

    /* ESP32-S3 ADC: 3.3V reference, 12-bit (0-4095), 12dB attenuation ≈ 0-3.3V */
    /* Voltage divider correction from demo: multiply by 3 * 0.980952 */
    float voltage = (avg / 4095.0f) * 3.3f * 3.0f * 0.980952f;
    ESP_LOGI(TAG, "  Average ADC = %d → Battery voltage ≈ %.2fV", avg, voltage);

    adc_oneshot_del_unit(adc_handle);
}

/* ═══════════════════════════════════════════════════
   TEST 8: WiFi Scan
   ═══════════════════════════════════════════════════ */

static void test_wifi(void)
{
    ESP_LOGI(TAG, "══════ TEST 7: WiFi ══════");

    /* Initialize NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    /* Initialize network */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /* Scan mode */
    ESP_LOGI(TAG, "  Scanning for WiFi networks...");
    wifi_scan_config_t scan_config = {};
    scan_config.show_hidden = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    ESP_LOGI(TAG, "  Found %d access point(s):", ap_count);

    if (ap_count > 0) {
        wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc(sizeof(wifi_ap_record_t) * ap_count);
        esp_wifi_scan_get_ap_records(&ap_count, ap_records);

        for (int i = 0; i < ap_count; i++) {
            int rssi = ap_records[i].rssi;
            const char *auth = "OPEN";
            if (ap_records[i].authmode == WIFI_AUTH_WPA2_PSK) auth = "WPA2";
            else if (ap_records[i].authmode == WIFI_AUTH_WPA_PSK) auth = "WPA";
            else if (ap_records[i].authmode == WIFI_AUTH_WPA3_PSK) auth = "WPA3";
            else if (ap_records[i].authmode == WIFI_AUTH_WEP) auth = "WEP";

            ESP_LOGI(TAG, "    [%2d] %-32s  CH:%2d  RSSI:%3d  %s",
                     i + 1, (char *)ap_records[i].ssid,
                     ap_records[i].primary, rssi, auth);
        }
        free(ap_records);
    }

    esp_wifi_stop();
    ESP_LOGI(TAG, "  WiFi scan complete");
}

/* ═══════════════════════════════════════════════════
   MAIN — Run all tests sequentially
   ═══════════════════════════════════════════════════ */
extern "C" void app_main(void)
{
    ESP_LOGI(TAG, "╔══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║   STUDYBUD PERIPHERAL TEST FIRMWARE  ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════╝");
    ESP_LOGI(TAG, "");

    /* Initialize I2C bus (required for EXIO, RTC, IMU) */
    ESP_LOGI(TAG, ">>> Initializing I2C bus...");
    I2C_Init();

    /* Initialize EXIO (required for buzzer, and LCD reset/CS) */
    ESP_LOGI(TAG, ">>> Initializing EXIO...");
    EXIO_Init();

    /* Run all tests — encoder is last and runs forever */
    test_i2c_scan();
    vTaskDelay(pdMS_TO_TICKS(500));

    test_exio();
    vTaskDelay(pdMS_TO_TICKS(500));

    test_rtc();
    vTaskDelay(pdMS_TO_TICKS(500));

    test_imu();
    vTaskDelay(pdMS_TO_TICKS(500));

    test_speaker();
    vTaskDelay(pdMS_TO_TICKS(500));

    test_battery();
    vTaskDelay(pdMS_TO_TICKS(500));

    test_wifi();

    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "╔══════════════════════════════════════╗");
    ESP_LOGI(TAG, "║  ALL AUTOMATED TESTS COMPLETE        ║");
    ESP_LOGI(TAG, "║  Now entering encoder test (forever) ║");
    ESP_LOGI(TAG, "╚══════════════════════════════════════╝");

    test_encoder();  /* never returns */
}
