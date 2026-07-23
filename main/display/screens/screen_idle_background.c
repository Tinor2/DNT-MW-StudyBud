#include "screen_idle_background.h"
#include "ui_manager.h"
#include "studybud_theme.h"
#include "idle_bg_tree.h"
#include "esp_log.h"
#include <math.h>

static const char *TAG = "Screen_IdleBG";

#define DISPLAY_R    240
#define DISPLAY_CY   240
#define ROW_SPACING  80
#define MAX_ROW_W    400
#define MIN_VISIBLE_W 100
#define ROW_INNER_PAD 12

typedef struct {
    const char *name;
    const lv_img_dsc_t *img;
} bg_item_t;

static const bg_item_t backgrounds[] = {
    { "Blue Sky Tree", &idle_bg_tree },
};
static const int bg_count = sizeof(backgrounds) / sizeof(backgrounds[0]);

static lv_obj_t *screen;
static lv_obj_t *bg_container;
static lv_obj_t *arrow_up_label;
static lv_obj_t *arrow_down_label;
static lv_obj_t *lbl_title;

static lv_obj_t *row_objects[8];
static lv_obj_t *row_labels[8];
static lv_obj_t *row_previews[8];

static int selected_index = 0;
static bool viewing = false;
static lv_obj_t *fullscreen_img = NULL;
static lv_obj_t *view_hint = NULL;

static void update_focus_styles(void);
static void update_arrow_visibility(void);
static void apply_radial_scroll(void);
static void radial_timer_cb(lv_timer_t *timer);
static void initial_scroll_cb(lv_timer_t *timer);
static void enter_view_mode(void);
static void exit_view_mode(void);
static void anim_set_opa(void *var, int32_t val);

static lv_obj_t *focused_row = NULL;
static lv_timer_t *radial_timer = NULL;

static lv_obj_t *pulse_ring = NULL;

static lv_color_t compute_avg_color(const lv_img_dsc_t *img)
{
    const uint8_t *data = img->data;
    int total_pixels = img->header.w * img->header.h;
    int step = total_pixels / 256;
    if (step < 1) step = 1;

    uint32_t r_sum = 0, g_sum = 0, b_sum = 0;
    int count = 0;

    for (int i = 0; i < total_pixels; i += step) {
        int offset = i * 2;
        uint16_t pixel = data[offset] | (data[offset + 1] << 8);
        r_sum += (pixel >> 11) & 0x1F;
        g_sum += (pixel >> 5) & 0x3F;
        b_sum += pixel & 0x1F;
        count++;
    }

    uint8_t r = (uint8_t)((r_sum / count) * 255 / 31);
    uint8_t g = (uint8_t)((g_sum / count) * 255 / 63);
    uint8_t b = (uint8_t)((b_sum / count) * 255 / 31);
    return lv_color_make(r, g, b);
}

static void start_pulse_animation(void)
{
    if (!pulse_ring) return;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, pulse_ring);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_set_opa);
    lv_anim_set_values(&a, LV_OPA_30, LV_OPA_80);
    lv_anim_set_time(&a, 1500);
    lv_anim_set_playback_time(&a, 1500);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void anim_set_opa(void *var, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)val, 0);
}

static void create_bg_row(lv_obj_t *parent, int index)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, MAX_ROW_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_row(row, 0, 0);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_set_style_pad_all(row, ROW_INNER_PAD, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_min_width(row, 0, 0);
    row_objects[index] = row;

    lv_obj_t *preview = lv_img_create(row);
    lv_img_set_src(preview, backgrounds[index].img);
    lv_obj_set_size(preview, 56, 56);
    lv_obj_set_style_radius(preview, 8, 0);
    row_previews[index] = preview;

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, backgrounds[index].name);
    lv_obj_set_flex_grow(label, 1);
    lv_obj_set_style_text_color(label, LV_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_80, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_pad_ver(row, 6, 0);
    row_labels[index] = label;
}

static void update_focus_styles(void)
{
    for (int i = 0; i < bg_count; i++) {
        lv_obj_t *row = row_objects[i];
        lv_obj_t *label = row_labels[i];
        if (!row || !label) continue;

        if (row == focused_row) {
            lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_pad_ver(row, 12, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_20, 0);
            lv_obj_set_style_bg_color(row, LV_COLOR_PRIMARY, 0);
            lv_obj_set_style_radius(row, 12, 0);

            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, label);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_set_opa);
            lv_anim_set_values(&a, LV_OPA_80, LV_OPA_COVER);
            lv_anim_set_time(&a, 200);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_start(&a);
        } else {
            lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
            lv_obj_set_style_pad_ver(row, 6, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);

            lv_anim_t a;
            lv_anim_init(&a);
            lv_anim_set_var(&a, label);
            lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_set_opa);
            lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_80);
            lv_anim_set_time(&a, 200);
            lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
            lv_anim_start(&a);
        }
    }
}

static void update_arrow_visibility(void)
{
    lv_coord_t st = lv_obj_get_scroll_top(bg_container);
    lv_coord_t sb = lv_obj_get_scroll_bottom(bg_container);
    if (st <= 0) lv_obj_add_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_clear_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);
    if (sb <= 0) lv_obj_add_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_clear_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);
}

static void apply_radial_scroll(void)
{
    if (!bg_container) return;

    lv_obj_update_layout(bg_container);

    bool any_hidden_top = false;
    bool any_hidden_bottom = false;

    for (int i = 0; i < bg_count; i++) {
        lv_obj_t *row = row_objects[i];
        if (!row) continue;

        lv_coord_t row_y = row->coords.y1;
        lv_coord_t row_h = lv_obj_get_height(row);
        lv_coord_t mid_y = row_y + row_h / 2;
        lv_coord_t dy = mid_y - DISPLAY_CY;
        lv_coord_t ady = dy < 0 ? -dy : dy;

        if (ady >= DISPLAY_R) {
            lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
            if (dy < 0) any_hidden_top = true;
            else        any_hidden_bottom = true;
            continue;
        }

        float fhalf = sqrtf((float)DISPLAY_R * DISPLAY_R - (float)ady * ady);
        lv_coord_t half_w = (lv_coord_t)fhalf;
        lv_coord_t avail_w = half_w * 2;
        if (avail_w > MAX_ROW_W) avail_w = MAX_ROW_W;

        if (avail_w < MIN_VISIBLE_W) {
            lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
            if (dy < 0) any_hidden_top = true;
            else        any_hidden_bottom = true;
            continue;
        }

        lv_obj_clear_flag(row, LV_OBJ_FLAG_HIDDEN);

        lv_coord_t new_x = (480 - avail_w) / 2;
        lv_obj_set_width(row, avail_w);
        lv_obj_set_x(row, new_x);

        lv_opa_t opa;
        if (ady < 60) {
            opa = LV_OPA_COVER;
        } else {
            float fade = 1.0f - (float)(ady - 60) / (float)(DISPLAY_R - 60);
            if (fade < 0.15f) fade = 0.15f;
            opa = (lv_opa_t)(fade * 255);
        }
        lv_obj_set_style_opa(row, opa, 0);
    }

    if (any_hidden_top) lv_obj_clear_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);
    if (any_hidden_bottom) lv_obj_clear_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);
}

static void radial_timer_cb(lv_timer_t *timer) { (void)timer; apply_radial_scroll(); }

static void initial_scroll_cb(lv_timer_t *timer)
{
    (void)timer;
    if (focused_row) {
        lv_obj_scroll_to_view(focused_row, LV_ANIM_OFF);
        apply_radial_scroll();
        update_focus_styles();
    }
}

static void scroll_cb(lv_event_t *e) { (void)e; update_arrow_visibility(); }

static void view_fade_in_ready(lv_anim_t *a)
{
    (void)a;
    start_pulse_animation();
}

static void enter_view_mode(void)
{
    if (selected_index < 0 || selected_index >= bg_count) return;
    viewing = true;

    lv_obj_add_flag(bg_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(lbl_title, LV_OBJ_FLAG_HIDDEN);

    lv_obj_clear_flag(fullscreen_img, LV_OBJ_FLAG_HIDDEN);
    lv_img_set_src(fullscreen_img, backgrounds[selected_index].img);
    lv_obj_set_style_opa(fullscreen_img, LV_OPA_TRANSP, 0);

    lv_color_t avg = compute_avg_color(backgrounds[selected_index].img);
    lv_obj_set_style_border_color(pulse_ring, avg, 0);
    lv_obj_clear_flag(pulse_ring, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(pulse_ring, LV_OPA_TRANSP, 0);

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, fullscreen_img);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_set_opa);
    lv_anim_set_values(&a, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a, 600);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
    lv_anim_set_ready_cb(&a, view_fade_in_ready);
    lv_anim_start(&a);

    lv_obj_clear_flag(view_hint, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Viewing background: %s", backgrounds[selected_index].name);
}

static void fade_out_ready_cb(lv_anim_t *a)
{
    (void)a;
    lv_obj_add_flag(fullscreen_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(view_hint, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pulse_ring, LV_OBJ_FLAG_HIDDEN);
    lv_anim_del(pulse_ring, NULL);

    lv_obj_clear_flag(bg_container, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(lbl_title, LV_OBJ_FLAG_HIDDEN);
    update_arrow_visibility();
    apply_radial_scroll();
}

static void exit_view_mode(void)
{
    viewing = false;

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, fullscreen_img);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)anim_set_opa);
    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_time(&a, 450);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
    lv_anim_set_ready_cb(&a, fade_out_ready_cb);
    lv_anim_start(&a);
}

lv_obj_t *screen_idle_background_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, LV_COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    lbl_title = lv_label_create(screen);
    lv_label_set_text(lbl_title, "Backgrounds");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 30);

    /* Fullscreen image (hidden by default) */
    fullscreen_img = lv_img_create(screen);
    lv_obj_set_size(fullscreen_img, 480, 480);
    lv_obj_align(fullscreen_img, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(fullscreen_img, LV_OBJ_FLAG_HIDDEN);

    /* Pulsing ring overlay (hidden by default) */
    pulse_ring = lv_obj_create(screen);
    lv_obj_set_size(pulse_ring, 480, 480);
    lv_obj_align(pulse_ring, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(pulse_ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(pulse_ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(pulse_ring, 12, 0);
    lv_obj_set_style_border_color(pulse_ring, LV_COLOR_PRIMARY, 0);
    lv_obj_set_style_border_opa(pulse_ring, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(pulse_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(pulse_ring, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(pulse_ring, LV_OBJ_FLAG_HIDDEN);

    view_hint = lv_label_create(screen);
    lv_label_set_text(view_hint, "Press to return");
    lv_obj_set_style_text_font(view_hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(view_hint, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(view_hint, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_add_flag(view_hint, LV_OBJ_FLAG_HIDDEN);

    /* Scrollable container for background rows */
    bg_container = lv_obj_create(screen);
    lv_obj_set_size(bg_container, 480, 480);
    lv_obj_align(bg_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(bg_container, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(bg_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bg_container, 0, 0);
    lv_obj_set_style_pad_all(bg_container, 0, 0);
    lv_obj_set_scroll_dir(bg_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(bg_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_y(bg_container, LV_SCROLL_SNAP_CENTER);
    lv_obj_add_event_cb(bg_container, scroll_cb, LV_EVENT_SCROLL, NULL);

    /* Arrow indicators */
    arrow_up_label = lv_label_create(screen);
    lv_label_set_text(arrow_up_label, LV_SYMBOL_UP);
    lv_obj_set_style_text_color(arrow_up_label, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(arrow_up_label, &lv_font_montserrat_16, 0);
    lv_obj_align(arrow_up_label, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_add_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);

    arrow_down_label = lv_label_create(screen);
    lv_label_set_text(arrow_down_label, LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(arrow_down_label, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_set_style_text_font(arrow_down_label, &lv_font_montserrat_16, 0);
    lv_obj_align(arrow_down_label, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_obj_add_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);

    /* Create background rows */
    for (int i = 0; i < bg_count; i++) {
        create_bg_row(bg_container, i);
        lv_obj_set_pos(row_objects[i], (480 - MAX_ROW_W) / 2, i * ROW_SPACING);
    }

    focused_row = row_objects[selected_index];
    update_focus_styles();
    update_arrow_visibility();

    apply_radial_scroll();
    radial_timer = lv_timer_create(radial_timer_cb, 50, NULL);

    lv_timer_t *init_timer = lv_timer_create(initial_scroll_cb, 50, NULL);
    init_timer->repeat_count = 1;

    ESP_LOGI(TAG, "Idle background screen created with %d backgrounds", bg_count);
    return screen;
}

void screen_idle_background_encoder_event(lv_indev_data_t *data)
{
    if (viewing) {
        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            exit_view_mode();
        }
        return;
    }

    if (!bg_container) return;

    if (data->enc_diff != 0) {
        int new_idx = selected_index + data->enc_diff;
        if (new_idx < 0) new_idx = bg_count - 1;
        if (new_idx >= bg_count) new_idx = 0;

        selected_index = new_idx;
        focused_row = row_objects[selected_index];
        update_focus_styles();
        lv_obj_scroll_to_view(focused_row, LV_ANIM_ON);
    }

    if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
        enter_view_mode();
    }
}
