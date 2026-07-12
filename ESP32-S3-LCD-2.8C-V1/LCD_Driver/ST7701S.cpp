#include "ST7701S.h"
#include "hal/lcd_types.h"

#define SPI_WRITE_COMM(cmd) ST7701S_WriteCommand(cmd)
#define SPI_WRITE_DATA(data) ST7701S_WriteData(data)

static const char *LCD_TAG = "LCD";
static ST7701S_handle st7701s = NULL;

static ST7701S_handle ST7701S_newObject(int SDA, int SCL, int CS, spi_host_device_t channel_select, uint8_t method_select)
{
    ST7701S_handle handle = (ST7701S_handle)heap_caps_calloc(1, sizeof(ST7701S), MALLOC_CAP_DEFAULT);
    handle->method_select = method_select;

    handle->spi_io_config_t.miso_io_num = -1;
    handle->spi_io_config_t.mosi_io_num = SDA;
    handle->spi_io_config_t.sclk_io_num = SCL;
    handle->spi_io_config_t.quadwp_io_num = -1;
    handle->spi_io_config_t.quadhd_io_num = -1;
    handle->spi_io_config_t.max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE;

    ESP_ERROR_CHECK(spi_bus_initialize(channel_select, &handle->spi_io_config_t, SPI_DMA_CH_AUTO));

    handle->st7701s_protocol_config_t.command_bits = 1;
    handle->st7701s_protocol_config_t.address_bits = 8;
    handle->st7701s_protocol_config_t.clock_speed_hz = 4000000;
    handle->st7701s_protocol_config_t.mode = 0;
    handle->st7701s_protocol_config_t.spics_io_num = CS;
    handle->st7701s_protocol_config_t.queue_size = 1;

    ESP_ERROR_CHECK(spi_bus_add_device(channel_select, &handle->st7701s_protocol_config_t, &handle->spi_device));

    return handle;
}

static void ST7701S_WriteCommand(uint8_t cmd)
{
    spi_transaction_t spi_tran = {};
    spi_tran.cmd = 0;
    spi_tran.addr = cmd;
    spi_device_transmit(st7701s->spi_device, &spi_tran);
}

static void ST7701S_WriteData(uint8_t data)
{
    spi_transaction_t spi_tran = {};
    spi_tran.cmd = 1;
    spi_tran.addr = data;
    spi_device_transmit(st7701s->spi_device, &spi_tran);
}

static void ST7701S_screen_init(unsigned char type)
{
    if (type != 1) return;

    SPI_WRITE_COMM(0xFF); SPI_WRITE_DATA(0x77); SPI_WRITE_DATA(0x01);
    SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x13);
    SPI_WRITE_COMM(0xEF); SPI_WRITE_DATA(0x08);
    SPI_WRITE_COMM(0xFF); SPI_WRITE_DATA(0x77); SPI_WRITE_DATA(0x01);
    SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x10);
    SPI_WRITE_COMM(0xC0); SPI_WRITE_DATA(0x3B); SPI_WRITE_DATA(0x00);
    SPI_WRITE_COMM(0xC1); SPI_WRITE_DATA(0x10); SPI_WRITE_DATA(0x0C);
    SPI_WRITE_COMM(0xC2); SPI_WRITE_DATA(0x07); SPI_WRITE_DATA(0x0A);
    SPI_WRITE_COMM(0xC7); SPI_WRITE_DATA(0x00);
    SPI_WRITE_COMM(0xCC); SPI_WRITE_DATA(0x10);
    SPI_WRITE_COMM(0xCD); SPI_WRITE_DATA(0x08);
    SPI_WRITE_COMM(0xB0); SPI_WRITE_DATA(0x05); SPI_WRITE_DATA(0x12);
    SPI_WRITE_DATA(0x98); SPI_WRITE_DATA(0x0E); SPI_WRITE_DATA(0x0F);
    SPI_WRITE_DATA(0x07); SPI_WRITE_DATA(0x07); SPI_WRITE_DATA(0x09);
    SPI_WRITE_DATA(0x09); SPI_WRITE_DATA(0x23); SPI_WRITE_DATA(0x05);
    SPI_WRITE_DATA(0x52); SPI_WRITE_DATA(0x0F); SPI_WRITE_DATA(0x67);
    SPI_WRITE_DATA(0x2C); SPI_WRITE_DATA(0x11);
    SPI_WRITE_COMM(0xB1); SPI_WRITE_DATA(0x0B); SPI_WRITE_DATA(0x11);
    SPI_WRITE_DATA(0x97); SPI_WRITE_DATA(0x0C); SPI_WRITE_DATA(0x12);
    SPI_WRITE_DATA(0x06); SPI_WRITE_DATA(0x06); SPI_WRITE_DATA(0x08);
    SPI_WRITE_DATA(0x08); SPI_WRITE_DATA(0x22); SPI_WRITE_DATA(0x03);
    SPI_WRITE_DATA(0x51); SPI_WRITE_DATA(0x11); SPI_WRITE_DATA(0x66);
    SPI_WRITE_DATA(0x2B); SPI_WRITE_DATA(0x0F);
    SPI_WRITE_COMM(0xFF); SPI_WRITE_DATA(0x77); SPI_WRITE_DATA(0x01);
    SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x11);
    SPI_WRITE_COMM(0xB0); SPI_WRITE_DATA(0x5D);
    SPI_WRITE_COMM(0xB1); SPI_WRITE_DATA(0x3E);
    SPI_WRITE_COMM(0xB2); SPI_WRITE_DATA(0x81);
    SPI_WRITE_COMM(0xB3); SPI_WRITE_DATA(0x80);
    SPI_WRITE_COMM(0xB5); SPI_WRITE_DATA(0x4E);
    SPI_WRITE_COMM(0xB7); SPI_WRITE_DATA(0x85);
    SPI_WRITE_COMM(0xB8); SPI_WRITE_DATA(0x20);
    SPI_WRITE_COMM(0xC1); SPI_WRITE_DATA(0x78);
    SPI_WRITE_COMM(0xC2); SPI_WRITE_DATA(0x78);
    SPI_WRITE_COMM(0xD0); SPI_WRITE_DATA(0x88);
    SPI_WRITE_COMM(0xE0); SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x02);
    SPI_WRITE_COMM(0xE1); SPI_WRITE_DATA(0x06); SPI_WRITE_DATA(0x30);
    SPI_WRITE_DATA(0x08); SPI_WRITE_DATA(0x30); SPI_WRITE_DATA(0x05);
    SPI_WRITE_DATA(0x30); SPI_WRITE_DATA(0x07); SPI_WRITE_DATA(0x30);
    SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x33); SPI_WRITE_DATA(0x33);
    SPI_WRITE_COMM(0xE2); SPI_WRITE_DATA(0x11); SPI_WRITE_DATA(0x11);
    SPI_WRITE_DATA(0x33); SPI_WRITE_DATA(0x33); SPI_WRITE_DATA(0xF4);
    SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x00);
    SPI_WRITE_DATA(0xF4); SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x00);
    SPI_WRITE_COMM(0xE3); SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x00);
    SPI_WRITE_DATA(0x11); SPI_WRITE_DATA(0x11);
    SPI_WRITE_COMM(0xE4); SPI_WRITE_DATA(0x44); SPI_WRITE_DATA(0x44);
    SPI_WRITE_COMM(0xE5); SPI_WRITE_DATA(0x0D); SPI_WRITE_DATA(0xF5);
    SPI_WRITE_DATA(0x30); SPI_WRITE_DATA(0xF0); SPI_WRITE_DATA(0x0F);
    SPI_WRITE_DATA(0xF7); SPI_WRITE_DATA(0x30); SPI_WRITE_DATA(0xF0);
    SPI_WRITE_DATA(0x09); SPI_WRITE_DATA(0xF1); SPI_WRITE_DATA(0x30);
    SPI_WRITE_DATA(0xF0); SPI_WRITE_DATA(0x0B); SPI_WRITE_DATA(0xF3);
    SPI_WRITE_DATA(0x30); SPI_WRITE_DATA(0xF0);
    SPI_WRITE_COMM(0xE6); SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x00);
    SPI_WRITE_DATA(0x11); SPI_WRITE_DATA(0x11);
    SPI_WRITE_COMM(0xE7); SPI_WRITE_DATA(0x44); SPI_WRITE_DATA(0x44);
    SPI_WRITE_COMM(0xE8); SPI_WRITE_DATA(0x0C); SPI_WRITE_DATA(0xF4);
    SPI_WRITE_DATA(0x30); SPI_WRITE_DATA(0xF0); SPI_WRITE_DATA(0x0E);
    SPI_WRITE_DATA(0xF6); SPI_WRITE_DATA(0x30); SPI_WRITE_DATA(0xF0);
    SPI_WRITE_DATA(0x08); SPI_WRITE_DATA(0xF0); SPI_WRITE_DATA(0x30);
    SPI_WRITE_DATA(0xF0); SPI_WRITE_DATA(0x0A); SPI_WRITE_DATA(0xF2);
    SPI_WRITE_DATA(0x30); SPI_WRITE_DATA(0xF0);
    SPI_WRITE_COMM(0xE9); SPI_WRITE_DATA(0x36); SPI_WRITE_DATA(0x01);
    SPI_WRITE_COMM(0xEB); SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x01);
    SPI_WRITE_DATA(0xE4); SPI_WRITE_DATA(0xE4); SPI_WRITE_DATA(0x44);
    SPI_WRITE_DATA(0x88); SPI_WRITE_DATA(0x40);
    SPI_WRITE_COMM(0xED); SPI_WRITE_DATA(0xFF); SPI_WRITE_DATA(0x10);
    SPI_WRITE_DATA(0xAF); SPI_WRITE_DATA(0x76); SPI_WRITE_DATA(0x54);
    SPI_WRITE_DATA(0x2B); SPI_WRITE_DATA(0xCF); SPI_WRITE_DATA(0xFF);
    SPI_WRITE_DATA(0xFF); SPI_WRITE_DATA(0xFC); SPI_WRITE_DATA(0xB2);
    SPI_WRITE_DATA(0x45); SPI_WRITE_DATA(0x67); SPI_WRITE_DATA(0xFA);
    SPI_WRITE_DATA(0x01); SPI_WRITE_DATA(0xFF);
    SPI_WRITE_COMM(0xEF); SPI_WRITE_DATA(0x08); SPI_WRITE_DATA(0x08);
    SPI_WRITE_DATA(0x08); SPI_WRITE_DATA(0x45); SPI_WRITE_DATA(0x3F);
    SPI_WRITE_DATA(0x54);
    SPI_WRITE_COMM(0xFF); SPI_WRITE_DATA(0x77); SPI_WRITE_DATA(0x01);
    SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x00); SPI_WRITE_DATA(0x00);
    SPI_WRITE_COMM(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    SPI_WRITE_COMM(0x3A); SPI_WRITE_DATA(0x66);
    SPI_WRITE_COMM(0x36); SPI_WRITE_DATA(0x00);
    SPI_WRITE_COMM(0x35); SPI_WRITE_DATA(0x00);
    SPI_WRITE_COMM(0x29);
}

static void ST7701S_reset(void)
{
    Set_EXIO(TCA9554_EXIO1, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    Set_EXIO(TCA9554_EXIO1, true);
    vTaskDelay(pdMS_TO_TICKS(10));
}

static void ST7701S_CS_EN(void)
{
    Set_EXIO(TCA9554_EXIO3, false);
    vTaskDelay(pdMS_TO_TICKS(10));
}

static void ST7701S_CS_Dis(void)
{
    Set_EXIO(TCA9554_EXIO3, true);
    vTaskDelay(pdMS_TO_TICKS(10));
}

esp_lcd_panel_handle_t panel_handle = NULL;

void LCD_Init(void)
{
    ST7701S_reset();
    ST7701S_CS_EN();
    vTaskDelay(pdMS_TO_TICKS(100));

    st7701s = ST7701S_newObject(LCD_MOSI, LCD_SCLK, LCD_CS, SPI2_HOST, 1);
    ST7701S_screen_init(1);

    ESP_LOGI(LCD_TAG, "Install RGB LCD panel driver");

    esp_lcd_rgb_panel_config_t panel_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .timings = {
            .pclk_hz = EXAMPLE_LCD_PIXEL_CLOCK_HZ,
            .h_res = EXAMPLE_LCD_H_RES,
            .v_res = EXAMPLE_LCD_V_RES,
            .hsync_pulse_width = 8,
            .hsync_back_porch = 10,
            .hsync_front_porch = 50,
            .vsync_pulse_width = 2,
            .vsync_back_porch = 18,
            .vsync_front_porch = 8,
            .flags = {
                .hsync_idle_low = 0,
                .vsync_idle_low = 0,
                .de_idle_high = 0,
                .pclk_active_neg = 0,
                .pclk_idle_high = 0,
            },
        },
        .data_width = 16,
        .in_color_format = LCD_COLOR_FMT_RGB565,
        .out_color_format = LCD_COLOR_FMT_RGB565,
        .num_fbs = 1,
        .user_fbs = {NULL},
        .bounce_buffer_size_px = 10 * EXAMPLE_LCD_H_RES,
        .dma_burst_size = 0,
        .hsync_gpio_num = (gpio_num_t)EXAMPLE_PIN_NUM_HSYNC,
        .vsync_gpio_num = (gpio_num_t)EXAMPLE_PIN_NUM_VSYNC,
        .de_gpio_num = (gpio_num_t)EXAMPLE_PIN_NUM_DE,
        .pclk_gpio_num = (gpio_num_t)EXAMPLE_PIN_NUM_PCLK,
        .disp_gpio_num = (gpio_num_t)EXAMPLE_PIN_NUM_DISP_EN,
        .data_gpio_nums = {
            (gpio_num_t)EXAMPLE_PIN_NUM_DATA0,  (gpio_num_t)EXAMPLE_PIN_NUM_DATA1,
            (gpio_num_t)EXAMPLE_PIN_NUM_DATA2,  (gpio_num_t)EXAMPLE_PIN_NUM_DATA3,
            (gpio_num_t)EXAMPLE_PIN_NUM_DATA4,  (gpio_num_t)EXAMPLE_PIN_NUM_DATA5,
            (gpio_num_t)EXAMPLE_PIN_NUM_DATA6,  (gpio_num_t)EXAMPLE_PIN_NUM_DATA7,
            (gpio_num_t)EXAMPLE_PIN_NUM_DATA8,  (gpio_num_t)EXAMPLE_PIN_NUM_DATA9,
            (gpio_num_t)EXAMPLE_PIN_NUM_DATA10, (gpio_num_t)EXAMPLE_PIN_NUM_DATA11,
            (gpio_num_t)EXAMPLE_PIN_NUM_DATA12, (gpio_num_t)EXAMPLE_PIN_NUM_DATA13,
            (gpio_num_t)EXAMPLE_PIN_NUM_DATA14, (gpio_num_t)EXAMPLE_PIN_NUM_DATA15,
        },
        .flags = {
            .disp_active_low = 0,
            .refresh_on_demand = 0,
            .fb_in_psram = 1,
            .double_fb = 0,
            .no_fb = 0,
            .bb_invalidate_cache = 0,
        },
    };
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));

    ESP_LOGI(LCD_TAG, "Initialize RGB LCD panel");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

    ST7701S_CS_Dis();
    Backlight_Init();
}

static void example_ledc_init(void)
{
    ledc_timer_config_t ledc_timer = {
        .speed_mode      = LEDC_MODE,
        .duty_resolution = LEDC_DUTY_RES,
        .timer_num       = LEDC_TIMER,
        .freq_hz         = LEDC_FREQUENCY,
        .clk_cfg         = LEDC_AUTO_CLK,
        .deconfigure     = false,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .gpio_num   = EXAMPLE_PIN_NUM_BK_LIGHT,
        .speed_mode = LEDC_MODE,
        .channel    = LEDC_CHANNEL,
        .intr_type  = LEDC_INTR_DISABLE,
        .timer_sel  = LEDC_TIMER,
        .duty       = 0,
        .hpoint     = 0,
        .sleep_mode = LEDC_SLEEP_MODE_NO_ALIVE_NO_PD,
        .flags      = { .output_invert = 0 },
        .deconfigure = false,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void Backlight_Init(void)
{
    example_ledc_init();
    Set_Backlight(70);
}

void Set_Backlight(uint8_t Light)
{
    if (Light > 100) Light = 100;
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, Light * 82);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}
