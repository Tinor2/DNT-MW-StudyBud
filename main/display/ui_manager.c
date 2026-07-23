#include "ui_manager.h"
#include "screens/screen_home.h"
#include "screens/screen_menu.h"
#include "screens/screen_todos.h"
#include "screens/screen_breathing.h"
#include "screens/screen_idle_background.h"
#include "studybud_theme.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "UI_Manager";

static screen_id_t current_screen = SCREEN_HOME;
static lv_obj_t *screens[SCREEN_COUNT] = {0};

static void (*screen_event_handlers[SCREEN_COUNT])(lv_indev_data_t *) = {0};

#define LONG_PRESS_MS 800
#define GLOW_MAX_OPA  100
#define DISPLAY_SIZE  480
#define DISPLAY_CX    240
#define GLOW_OUTER_R  240
#define GLOW_THICKNESS 25
#define GLOW_INNER_R  (GLOW_OUTER_R - GLOW_THICKNESS)

static uint32_t press_start_tick = 0;
static bool waiting_for_release = false;
static bool long_press_fired = false;

static lv_obj_t *glow_overlay = NULL;
static lv_timer_t *glow_timer = NULL;

static void glow_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    if (!glow_overlay) return;

    if (!waiting_for_release) {
        lv_obj_set_style_opa(glow_overlay, LV_OPA_TRANSP, 0);
        if (glow_timer) {
            lv_timer_pause(glow_timer);
        }
        return;
    }

    if (long_press_fired) {
        lv_obj_set_style_opa(glow_overlay, LV_OPA_TRANSP, 0);
        if (glow_timer) lv_timer_pause(glow_timer);
        return;
    }

    uint32_t elapsed = lv_tick_elaps(press_start_tick);
    if (elapsed >= LONG_PRESS_MS) elapsed = LONG_PRESS_MS;

    lv_opa_t opa = (lv_opa_t)((uint32_t)GLOW_MAX_OPA * elapsed / LONG_PRESS_MS);
    lv_obj_set_style_opa(glow_overlay, opa, 0);
}

static void draw_glow_gradient(lv_obj_t *canvas)
{
    lv_color_t c = LV_COLOR_PRIMARY;

    for (lv_coord_t y = 0; y < DISPLAY_SIZE; y++) {
        lv_coord_t dy = y - DISPLAY_CX;
        for (lv_coord_t x = 0; x < DISPLAY_SIZE; x++) {
            lv_coord_t dx = x - DISPLAY_CX;
            float dist = sqrtf((float)(dx * dx + dy * dy));

            if (dist < GLOW_INNER_R || dist > GLOW_OUTER_R) {
                lv_canvas_set_px_opa(canvas, x, y, LV_OPA_TRANSP);
            } else {
                float t = (dist - GLOW_INNER_R) / (float)GLOW_THICKNESS;
                float curved = powf(t, 0.6f);
                lv_opa_t opa = (lv_opa_t)(75 + curved * 204);
                lv_canvas_set_px_color(canvas, x, y, c);
                lv_canvas_set_px_opa(canvas, x, y, opa);
            }
        }
    }
}

static void create_glow_overlay(void)
{
    lv_obj_t *top_layer = lv_layer_top();

    glow_overlay = lv_canvas_create(top_layer);

    lv_color_t *buf = lv_mem_alloc(DISPLAY_SIZE * DISPLAY_SIZE * sizeof(lv_color32_t));
    lv_canvas_set_buffer(glow_overlay, buf, DISPLAY_SIZE, DISPLAY_SIZE, LV_IMG_CF_TRUE_COLOR_ALPHA);

    lv_canvas_fill_bg(glow_overlay, LV_COLOR_PRIMARY, LV_OPA_TRANSP);
    draw_glow_gradient(glow_overlay);

    lv_obj_align(glow_overlay, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_opa(glow_overlay, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(glow_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(glow_overlay, LV_OBJ_FLAG_SCROLLABLE);

    glow_timer = lv_timer_create(glow_timer_cb, 16, NULL);
    lv_timer_pause(glow_timer);
}

void ui_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing UI Manager");

    /* Create default theme */
    lv_disp_t *disp = lv_disp_get_default();
    lv_theme_t *theme = lv_theme_default_init(
        disp,
        LV_COLOR_PRIMARY,
        LV_COLOR_SECONDARY,
        false,
        LV_FONT_DEFAULT
    );
    lv_disp_set_theme(disp, theme);

    /* Create glow overlay (on top layer, above all screens) */
    create_glow_overlay();

    /* Create all screens */
    screens[SCREEN_HOME] = screen_home_create();
    screens[SCREEN_MENU] = screen_menu_create();
    screens[SCREEN_TODOS] = screen_todos_create();
    screens[SCREEN_BREATHING] = screen_breathing_create();
    screens[SCREEN_BACKGROUNDS] = screen_idle_background_create();

    /* Register event handlers */
    screen_event_handlers[SCREEN_HOME] = screen_home_encoder_event;
    screen_event_handlers[SCREEN_MENU] = screen_menu_encoder_event;
    screen_event_handlers[SCREEN_TODOS] = screen_todos_encoder_event;
    screen_event_handlers[SCREEN_BREATHING] = screen_breathing_encoder_event;
    screen_event_handlers[SCREEN_BACKGROUNDS] = screen_idle_background_encoder_event;

    /* Load home screen as default */
    lv_scr_load(screens[SCREEN_HOME]);
    current_screen = SCREEN_HOME;

    ESP_LOGI(TAG, "UI Manager initialized, showing Home screen");
}

void ui_manager_switch_screen(screen_id_t screen)
{
    if (screen >= SCREEN_COUNT || !screens[screen]) {
        ESP_LOGW(TAG, "Invalid screen ID: %d", screen);
        return;
    }

    ESP_LOGI(TAG, "Switching to screen %d", screen);
    lv_scr_load_anim(screens[screen], LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);
    current_screen = screen;
}

void ui_manager_encoder_event(lv_indev_data_t *data)
{
    /* Forward rotation immediately, even while button is held */
    if (data->enc_diff != 0 && screen_event_handlers[current_screen]) {
        lv_indev_data_t fwd;
        fwd.enc_diff = data->enc_diff;
        fwd.state = LV_INDEV_STATE_REL;
        screen_event_handlers[current_screen](&fwd);
    }

    /* Button just pressed: start long-press timer, don't forward to screen yet */
    if (data->state == LV_INDEV_STATE_PR && !waiting_for_release) {
        press_start_tick = lv_tick_get();
        waiting_for_release = true;
        long_press_fired = false;

        if (current_screen != SCREEN_MENU) {
            if (glow_timer) lv_timer_resume(glow_timer);
        }
        return;
    }

    /* Button still held: check if long press threshold crossed */
    if (waiting_for_release && data->state == LV_INDEV_STATE_PR) {
        if (!long_press_fired && lv_tick_elaps(press_start_tick) >= LONG_PRESS_MS) {
            long_press_fired = true;
            if (current_screen != SCREEN_MENU) {
                ESP_LOGI(TAG, "Long press -> menu");
                ui_manager_switch_screen(SCREEN_MENU);
            }
        }
        return;
    }

    /* Button released: fire short press action if it wasn't a long press */
    if (data->state == LV_INDEV_STATE_REL && waiting_for_release) {
        bool was_long = long_press_fired;
        waiting_for_release = false;
        press_start_tick = 0;
        long_press_fired = false;

        if (glow_timer) lv_timer_pause(glow_timer);
        lv_obj_set_style_opa(glow_overlay, LV_OPA_TRANSP, 0);

        if (was_long) return;

        /* Short press: forward to screen handler */
        if (current_screen == SCREEN_MENU) {
            screen_id_t selected = screen_menu_get_selection();
            if (selected != SCREEN_COUNT) {
                ui_manager_switch_screen(selected);
                return;
            }
        }
        if (screen_event_handlers[current_screen]) {
            lv_indev_data_t pr = {0};
            pr.state = LV_INDEV_STATE_PR;
            pr.enc_diff = 0;
            screen_event_handlers[current_screen](&pr);
        }
        return;
    }
}

screen_id_t ui_manager_get_current_screen(void)
{
    return current_screen;
}
