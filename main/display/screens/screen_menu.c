#include "screen_menu.h"
#include "studybud_theme.h"
#include "ui_manager.h"
#include "app_logo.h"
#include "todo_logo.h"
#include "timer_logo.h"
#include "esp_log.h"
#include <math.h>
#include <string.h>

static const char *TAG = "Screen_Menu";

#define DISPLAY_R    240
#define DISPLAY_CY   240
#define ROW_SPACING  64
#define MAX_ROW_W    420
#define MIN_VISIBLE_W 120
#define ROW_INNER_PAD 12

static lv_obj_t *screen = NULL;
static lv_obj_t *menu_container = NULL;
static lv_obj_t *title_label = NULL;
static lv_obj_t *arrow_up_label = NULL;
static lv_obj_t *arrow_down_label = NULL;
static lv_obj_t *count_label = NULL;
static lv_timer_t *radial_timer = NULL;

static int selected_index = 0;

typedef struct {
    const char *icon;
    const char *name;
    screen_id_t target;
    const lv_img_dsc_t *img;
} menu_item_t;

static const menu_item_t menu_items[] = {
    { LV_SYMBOL_HOME,      "Home",          SCREEN_HOME,       NULL },
    { LV_SYMBOL_PLAY,      "Timer",         SCREEN_TIMER_PRESETS, &timer_logo },
    { LV_SYMBOL_LIST,      "Todos",         SCREEN_TODOS,      &todo_logo },
    { LV_SYMBOL_BELL,      "Water",         SCREEN_WATER,      &app_logo },
    { LV_SYMBOL_REFRESH,   "Breathing",     SCREEN_BREATHING,  NULL },
    { LV_SYMBOL_IMAGE,     "Backgrounds",   SCREEN_BACKGROUNDS, NULL },
    { LV_SYMBOL_EYE_OPEN,  "Sleep",         SCREEN_SLEEP,      NULL },
    { LV_SYMBOL_SETTINGS,  "Settings",      SCREEN_SETTINGS,   NULL },
};
static const int menu_count = sizeof(menu_items) / sizeof(menu_items[0]);

static lv_obj_t *row_icons[8];
static bool row_icon_is_image[8];
static lv_obj_t *row_labels[8];
static lv_obj_t *row_objects[8];
static lv_obj_t *focused_row = NULL;

static void update_focus_styles(void);
static void update_arrow_visibility(void);
static void update_menu_count(void);
static void apply_radial_scroll(void);
static void radial_timer_cb(lv_timer_t *timer);
static void initial_scroll_cb(lv_timer_t *timer);

static void anim_set_opa(void *var, int32_t val)
{
    lv_obj_set_style_opa((lv_obj_t *)var, (lv_opa_t)val, 0);
}

static lv_color_t accent_colors[8];
static lv_color_t accent_light[8];
static lv_color_t accent_dark[8];

static lv_color_t compute_dominant_color(const lv_img_dsc_t *dsc)
{
    if (!dsc || !dsc->data) {
        return LV_COLOR_PRIMARY;
    }

    /*
     * This function is specifically designed for:
     *
     * LV_IMG_CF_TRUE_COLOR_ALPHA
     *
     * On a standard LVGL RGB565 + Alpha image:
     *
     *   2 bytes = RGB565
     *   1 byte  = Alpha
     *
     * So each pixel is 3 bytes.
     */

    const uint8_t *data = dsc->data;

    int w = dsc->header.w;
    int h = dsc->header.h;

    int total_pixels = w * h;

    const int BYTES_PER_PIXEL = 3;

    /*
     * Number of possible colour entries.
     *
     * Your pixel-art images are very small, so
     * 128 unique colours is plenty.
     */
    #define MAX_COLORS 128

    typedef struct {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint32_t count;
    } color_entry_t;

    color_entry_t colors[MAX_COLORS];

    int color_count = 0;

    memset(colors, 0, sizeof(colors));

    /*
     * ----------------------------------------------------
     * Scan image
     * ----------------------------------------------------
     */
    for (int i = 0; i < total_pixels; i++) {

        const uint8_t *pixel =
            &data[i * BYTES_PER_PIXEL];

        /*
         * LVGL RGB565 is stored as two bytes.
         *
         * On little-endian ESP32:
         *
         * pixel[0] = low byte
         * pixel[1] = high byte
         */
        uint16_t rgb565 =
            ((uint16_t)pixel[1] << 8) |
            pixel[0];

        /*
         * Alpha is the third byte.
         */
        uint8_t alpha = pixel[2];

        /*
         * Ignore transparent pixels.
         */
        if (alpha < 128) {
            continue;
        }

        /*
         * Extract RGB565 channels.
         */
        uint8_t r5 =
            (rgb565 >> 11) & 0x1F;

        uint8_t g6 =
            (rgb565 >> 5) & 0x3F;

        uint8_t b5 =
            rgb565 & 0x1F;

        /*
         * Convert RGB565 to 8-bit RGB.
         *
         * Multiplication gives a better approximation
         * than simply shifting.
         */
        uint8_t r =
            (r5 * 255) / 31;

        uint8_t g =
            (g6 * 255) / 63;

        uint8_t b =
            (b5 * 255) / 31;

        /*
         * ------------------------------------------------
         * Ignore WHITE
         *
         * This removes white backgrounds and highlights.
         * ------------------------------------------------
         */
        if (r > 220 &&
            g > 220 &&
            b > 220) {

            continue;
        }

        /*
         * ------------------------------------------------
         * Ignore BLACK
         *
         * This removes black outlines.
         * ------------------------------------------------
         */
        if (r < 35 &&
            g < 35 &&
            b < 35) {

            continue;
        }

        /*
         * ------------------------------------------------
         * Ignore GREYS
         *
         * A colour is considered grey if the RGB
         * channels are all relatively close together.
         *
         * This prevents neutral grey colours from
         * becoming the dominant accent.
         * ------------------------------------------------
         */
        int max_channel = r;

        if (g > max_channel)
            max_channel = g;

        if (b > max_channel)
            max_channel = b;

        int min_channel = r;

        if (g < min_channel)
            min_channel = g;

        if (b < min_channel)
            min_channel = b;

        /*
         * Saturation difference.
         */
        int saturation =
            max_channel - min_channel;

        /*
         * Ignore low-saturation colours.
         *
         * Increase this to 40 if you want only
         * strongly coloured pixels.
         */
        if (saturation < 25) {
            continue;
        }

        /*
         * ------------------------------------------------
         * Find exact matching colour.
         * ------------------------------------------------
         */
        bool found = false;

        for (int j = 0;
             j < color_count;
             j++) {

            if (colors[j].r == r &&
                colors[j].g == g &&
                colors[j].b == b) {

                colors[j].count++;

                found = true;

                break;
            }
        }

        /*
         * ------------------------------------------------
         * Add new colour.
         * ------------------------------------------------
         */
        if (!found &&
            color_count < MAX_COLORS) {

            colors[color_count].r = r;
            colors[color_count].g = g;
            colors[color_count].b = b;

            colors[color_count].count = 1;

            color_count++;
        }
    }

    /*
     * ----------------------------------------------------
     * No valid colour found.
     * ----------------------------------------------------
     */
    if (color_count == 0) {
        return LV_COLOR_PRIMARY;
    }

    /*
     * ----------------------------------------------------
     * Find the most common coloured pixel.
     * ----------------------------------------------------
     */
    int dominant_index = 0;

    for (int i = 1;
         i < color_count;
         i++) {

        if (colors[i].count >
            colors[dominant_index].count) {

            dominant_index = i;
        }
    }

    /*
     * Return the exact dominant colour.
     */
    return lv_color_make(
        colors[dominant_index].r,
        colors[dominant_index].g,
        colors[dominant_index].b
    );

    #undef MAX_COLORS
}

static lv_color_t lighten_color(lv_color_t c, float amount)
{
    uint32_t r = LV_COLOR_GET_R(c);
    uint32_t g = LV_COLOR_GET_G(c);
    uint32_t b = LV_COLOR_GET_B(c);
    r = r + (uint32_t)((255 - r) * amount);
    g = g + (uint32_t)((255 - g) * amount);
    b = b + (uint32_t)((255 - b) * amount);
    return lv_color_make(r, g, b);
}

static lv_color_t darken_color(lv_color_t c, float amount)
{
    uint32_t r = LV_COLOR_GET_R(c);
    uint32_t g = LV_COLOR_GET_G(c);
    uint32_t b = LV_COLOR_GET_B(c);
    r = (uint32_t)(r * (1.0f - amount));
    g = (uint32_t)(g * (1.0f - amount));
    b = (uint32_t)(b * (1.0f - amount));
    return lv_color_make(r, g, b);
}

static lv_color_t pastel_color(lv_color_t c)
{
    float r = LV_COLOR_GET_R(c);
    float g = LV_COLOR_GET_G(c);
    float b = LV_COLOR_GET_B(c);

    /*
     * Create a very subtle tint.
     *
     * Lower this number for a stronger colour.
     * 0.88 = very subtle
     * 0.80 = noticeable
     * 0.70 = strong
     */
    r = r + (255.0f - r) * 0.88f;
    g = g + (255.0f - g) * 0.88f;
    b = b + (255.0f - b) * 0.88f;

    return lv_color_make(
        (uint8_t)r,
        (uint8_t)g,
        (uint8_t)b
    );
}

static void create_menu_row(lv_obj_t *parent, int index)
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

    lv_obj_t *icon;
    if (menu_items[index].img) {
        icon = lv_img_create(row);
        lv_img_set_src(icon, menu_items[index].img);
        lv_obj_set_style_img_recolor(icon, accent_colors[index], 0);
        lv_obj_set_style_img_opa(icon, LV_OPA_80, 0);
        lv_obj_set_style_bg_opa(icon, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(icon, 0, 0);
        row_icon_is_image[index] = true;
    } else {
        icon = lv_label_create(row);
        lv_label_set_text(icon, menu_items[index].icon);
        lv_obj_set_style_text_font(icon, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(icon, accent_colors[index], 0);
        lv_obj_set_style_text_opa(icon, LV_OPA_80, 0);
        row_icon_is_image[index] = false;
    }
    row_icons[index] = icon;

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, menu_items[index].name);
    lv_obj_set_flex_grow(label, 1);
    lv_obj_set_style_text_color(label, LV_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_opa(label, LV_OPA_80, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_pad_ver(row, 6, 0);
    row_labels[index] = label;
}
static lv_color_t darken_text_color(lv_color_t c,float factor)
{
    uint32_t r = LV_COLOR_GET_R(c);
    uint32_t g = LV_COLOR_GET_G(c);
    uint32_t b = LV_COLOR_GET_B(c);

    /*
     * Darken the text by 20%.
     *
     * Increase 0.20f to 0.25f or 0.30f
     * if you want stronger contrast.
     */
    r = (uint32_t)(r * factor);
    g = (uint32_t)(g * factor);
    b = (uint32_t)(b * factor);

    return lv_color_make(r, g, b);
}
static void update_focus_styles(void)
{
    int fi = selected_index;
    lv_color_t fc = accent_colors[fi];
    lv_color_t fl = accent_light[fi];
    lv_color_t text_color = darken_text_color(fc, 1.0f);
    if (screen) lv_obj_set_style_bg_color(screen, pastel_color(fc), 0);
    if (title_label) lv_obj_set_style_text_color(title_label, text_color, 0);
    if (arrow_up_label) lv_obj_set_style_text_color(arrow_up_label, fl, 0);
    if (arrow_down_label) lv_obj_set_style_text_color(arrow_down_label, fl, 0);
    if (count_label) lv_obj_set_style_text_color(count_label, fl, 0);

    for (int i = 0; i < menu_count; i++) {
        lv_obj_t *row = row_objects[i];
        lv_obj_t *label = row_labels[i];
        lv_obj_t *icon = row_icons[i];
        if (!row || !label || !icon) continue;

        if (row == focused_row) {
            lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_color(label, text_color, 0);
            if (row_icon_is_image[i]) {
                lv_obj_set_style_img_opa(icon, LV_OPA_COVER, 0);
            } else {
                lv_obj_set_style_text_opa(icon, LV_OPA_COVER, 0);
            }
            lv_obj_set_style_pad_ver(row, 12, 0);
            lv_obj_set_style_bg_opa(row, LV_OPA_20, 0);
            lv_obj_set_style_bg_color(row, fc, 0);
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
            lv_obj_set_style_text_color(label, LV_COLOR_TEXT, 0);
            if (row_icon_is_image[i]) {
                lv_obj_set_style_img_opa(icon, LV_OPA_80, 0);
            } else {
                lv_obj_set_style_text_opa(icon, LV_OPA_80, 0);
            }
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
    lv_coord_t st = lv_obj_get_scroll_top(menu_container);
    lv_coord_t sb = lv_obj_get_scroll_bottom(menu_container);
    if (st <= 0) lv_obj_add_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_clear_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);
    if (sb <= 0) lv_obj_add_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_clear_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);
}

static void update_menu_count(void)
{
    if (!count_label) return;
    lv_label_set_text_fmt(count_label, "%d apps", menu_count);
}

static void apply_radial_scroll(void)
{
    if (!menu_container) return;

    lv_obj_update_layout(menu_container);

    bool any_hidden_top = false;
    bool any_hidden_bottom = false;

    for (int i = 0; i < menu_count; i++) {
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

lv_obj_t *screen_menu_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, LV_COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    /* Title */
    title_label = lv_label_create(screen);
    lv_label_set_text(title_label, "Menu");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title_label, LV_COLOR_TEXT, 0);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 30);

    /* Scrollable container for menu rows */
    menu_container = lv_obj_create(screen);
    lv_obj_set_size(menu_container, 480, 480);
    lv_obj_align(menu_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(menu_container, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(menu_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(menu_container, 0, 0);
    lv_obj_set_style_pad_all(menu_container, 0, 0);
    lv_obj_set_scroll_dir(menu_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(menu_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_y(menu_container, LV_SCROLL_SNAP_CENTER);
    lv_obj_add_event_cb(menu_container, scroll_cb, LV_EVENT_SCROLL, NULL);

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

    /* Bottom count label */
    count_label = lv_label_create(screen);
    lv_label_set_text(count_label, "");
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(count_label, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, -60);

    /* Compute per-item accent colors from icon images */
    for (int i = 0; i < menu_count; i++) {
        if (menu_items[i].img) {
            accent_colors[i] =
                compute_dominant_color(
                    menu_items[i].img
                );
        } 
        else {
          accent_colors[i] =
                LV_COLOR_PRIMARY;
        }
        accent_light[i] =
            lighten_color(
                accent_colors[i],
                0.40f
            );
        accent_dark[i] =
            darken_color(
                accent_colors[i],
                0.15f
            );
    }

    /* Create menu rows */
    for (int i = 0; i < menu_count; i++) {
        create_menu_row(menu_container, i);
        lv_obj_set_pos(row_objects[i], (480 - MAX_ROW_W) / 2, i * ROW_SPACING);
    }

    focused_row = row_objects[selected_index];
    update_focus_styles();
    update_menu_count();
    update_arrow_visibility();

    apply_radial_scroll();
    radial_timer = lv_timer_create(radial_timer_cb, 50, NULL);

    lv_timer_t *init_timer = lv_timer_create(initial_scroll_cb, 50, NULL);
    init_timer->repeat_count = 1;

    ESP_LOGI(TAG, "Menu screen created with %d items", menu_count);
    return screen;
}

void screen_menu_encoder_event(lv_indev_data_t *data)
{
    if (!menu_container) return;

    if (data->enc_diff != 0) {
        int new_idx = selected_index + data->enc_diff;
        if (new_idx < 0) new_idx = menu_count - 1;
        if (new_idx >= menu_count) new_idx = 0;

        selected_index = new_idx;
        focused_row = row_objects[selected_index];
        update_focus_styles();
        lv_obj_scroll_to_view(focused_row, LV_ANIM_ON);
    }

    if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
        screen_id_t target = screen_menu_get_selection();
        if (target != SCREEN_COUNT) {
            ui_manager_switch_screen(target);
        }
    }
}

screen_id_t screen_menu_get_selection(void)
{
    if (selected_index >= 0 && selected_index < menu_count) {
        return menu_items[selected_index].target;
    }
    return SCREEN_COUNT;
}
