#include "LVGL_Driver.h"
#include "studybud_theme.h"
#include "ST7701S.h"
#include "encoder_input.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_heap_caps.h"

static const char *TAG = "LVGL_Driver";

static lv_disp_draw_buf_t disp_buf;
static lv_disp_drv_t disp_drv;
static lv_indev_drv_t indev_drv;
static esp_timer_handle_t lvgl_tick_timer = NULL;

static void *buf1 = NULL;
static void *buf2 = NULL;

static SemaphoreHandle_t sem_vsync_end;
static SemaphoreHandle_t sem_gui_ready;

static bool on_vsync_event(esp_lcd_panel_handle_t panel,
                           const esp_lcd_rgb_panel_event_data_t *event_data,
                           void *user_data)
{
    BaseType_t high_task_awoken = pdFALSE;
    if (xSemaphoreTakeFromISR(sem_gui_ready, &high_task_awoken) == pdTRUE) {
        xSemaphoreGiveFromISR(sem_vsync_end, &high_task_awoken);
    }
    return high_task_awoken == pdTRUE;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel = (esp_lcd_panel_handle_t)drv->user_data;
    int x1 = area->x1;
    int y1 = area->y1;
    int x2 = area->x2;
    int y2 = area->y2;

    xSemaphoreGive(sem_gui_ready);
    xSemaphoreTake(sem_vsync_end, portMAX_DELAY);

    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2 + 1, y2 + 1, color_map);
    lv_disp_flush_ready(drv);
}

static void lvgl_tick_cb(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

void LVGL_Driver_init(void)
{
    ESP_LOGI(TAG, "Initializing LVGL");

    /* Initialize encoder hardware first */
    encoder_init();

    /* Create VSYNC semaphores */
    sem_vsync_end = xSemaphoreCreateBinary();
    assert(sem_vsync_end);
    sem_gui_ready = xSemaphoreCreateBinary();
    assert(sem_gui_ready);

    /* Register VSYNC callback on the LCD panel (registered after LCD_Init) */
    ESP_LOGI(TAG, "Registering VSYNC callback");
    esp_lcd_rgb_panel_event_callbacks_t cbs = {
        .on_vsync = on_vsync_event,
    };
    ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL));

    /* Initialize LVGL library */
    lv_init();

    /* Allocate draw buffers from PSRAM (2 full frames) */
    ESP_LOGI(TAG, "Allocating draw buffers from PSRAM (%dx%d, %d bytes each)",
             LCD_H_RES, LCD_V_RES, LCD_H_RES * LCD_V_RES * sizeof(lv_color_t));
    buf1 = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf1);
    buf2 = heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    assert(buf2);

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LCD_H_RES * LCD_V_RES);

    /* Register display driver */
    ESP_LOGI(TAG, "Registering display driver");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    /* Register encoder input device */
    ESP_LOGI(TAG, "Registering encoder input device");
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.disp = disp;
    indev_drv.read_cb = encoder_input_read;
    lv_indev_drv_register(&indev_drv);

    /* Start tick timer (2ms period) */
    const esp_timer_create_args_t tick_args = {
        .callback = &lvgl_tick_cb,
        .name = "lvgl_tick"
    };
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "LVGL driver initialized successfully");
}

void LVGL_Driver_loop(void)
{
    lv_timer_handler();
}
