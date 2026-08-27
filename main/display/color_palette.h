#ifndef COLOR_PALETTE_H
#define COLOR_PALETTE_H

/*
 * color_palette.h
 *
 * Single source of truth for every colour helper in the firmware:
 *
 *   - Basic helpers         : lighten_color, darken_color, pastel_color, lv_color_pack
 *   - Per-screen accessors  : theme_accent, theme_accent_light, theme_accent_dark
 *   - Logo extraction       : rgb_to_hsv, hsv_to_lv_color, compute_dominant_color
 *   - Contrast helper       : darken_text_color
 *
 * The colour constants themselves (LV_COLOR_PRIMARY, LV_COLOR_TEXT, ...)
 * live in studybud_theme.h, which this file pulls in.
 */

#include "lvgl.h"
#include "studybud_theme.h"
#include "app_state.h"

#include <math.h>
#include <string.h>

/* ============================================================
 *  BASIC HELPERS
 * ============================================================ */

static inline uint8_t color_get_r8(lv_color_t c)
{
    return (uint8_t)(((uint32_t)LV_COLOR_GET_R(c) * 263 + 7) >> 5);
}

static inline uint8_t color_get_g8(lv_color_t c)
{
    return (uint8_t)(((uint32_t)LV_COLOR_GET_G(c) * 259 + 3) >> 6);
}

static inline uint8_t color_get_b8(lv_color_t c)
{
    return (uint8_t)(((uint32_t)LV_COLOR_GET_B(c) * 263 + 7) >> 5);
}

static inline lv_color_t lighten_color(lv_color_t c, float amount)
{
    uint32_t r = color_get_r8(c);
    uint32_t g = color_get_g8(c);
    uint32_t b = color_get_b8(c);
    r = r + (uint32_t)((255 - r) * amount);
    g = g + (uint32_t)((255 - g) * amount);
    b = b + (uint32_t)((255 - b) * amount);
    return lv_color_make(r, g, b);
}

static inline lv_color_t darken_color(lv_color_t c, float amount)
{
    uint32_t r = color_get_r8(c);
    uint32_t g = color_get_g8(c);
    uint32_t b = color_get_b8(c);
    r = (uint32_t)(r * (1.0f - amount));
    g = (uint32_t)(g * (1.0f - amount));
    b = (uint32_t)(b * (1.0f - amount));
    return lv_color_make(r, g, b);
}

/* Desaturate a color by blending it toward gray (amount 0..1).
 * Hue stays the same, so it mutes harsh/bright accents. */
static inline lv_color_t desaturate_color(lv_color_t c, float amount)
{
    uint32_t r = color_get_r8(c);
    uint32_t g = color_get_g8(c);
    uint32_t b = color_get_b8(c);
    uint32_t lum = (uint32_t)(0.299f * r + 0.587f * g + 0.114f * b);
    r = r + (uint32_t)((lum - r) * amount);
    g = g + (uint32_t)((lum - g) * amount);
    b = b + (uint32_t)((lum - b) * amount);
    return lv_color_make((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

static inline lv_color_t pastel_color(lv_color_t c)
{
    float r = color_get_r8(c);
    float g = color_get_g8(c);
    float b = color_get_b8(c);
    r = r + (255.0f - r) * 0.88f;
    g = g + (255.0f - g) * 0.88f;
    b = b + (255.0f - b) * 0.88f;
    return lv_color_make((uint8_t)r, (uint8_t)g, (uint8_t)b);
}

static inline uint32_t lv_color_pack(lv_color_t c)
{
    return ((uint32_t)color_get_r8(c) << 16) | ((uint32_t)color_get_g8(c) << 8) | color_get_b8(c);
}

/* ============================================================
 *  PER-SCREEN ACCENT ACCESSORS
 *
 *  The accent colors are computed from the menu logos by
 *  screen_menu and stored in app_state; these return them,
 *  falling back to the static primary palette.
 * ============================================================ */

static inline lv_color_t theme_accent(int screen_id)
{
    if (screen_id < 0 || screen_id >= MAX_SCREENS) return LV_COLOR_PRIMARY;
    uint32_t c = app_state_get()->screen_accent[screen_id];
    if (c == 0) return LV_COLOR_PRIMARY;
    return lv_color_make((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

static inline lv_color_t theme_accent_light(int screen_id)
{
    if (screen_id < 0 || screen_id >= MAX_SCREENS) return LV_COLOR_PRIMARY_LIGHT;
    uint32_t c = app_state_get()->screen_accent_light[screen_id];
    if (c == 0) return LV_COLOR_PRIMARY_LIGHT;
    return lv_color_make((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

static inline lv_color_t theme_accent_dark(int screen_id)
{
    if (screen_id < 0 || screen_id >= MAX_SCREENS) return LV_COLOR_PRIMARY_DARK;
    uint32_t c = app_state_get()->screen_accent_dark[screen_id];
    if (c == 0) return LV_COLOR_PRIMARY_DARK;
    return lv_color_make((c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF);
}

/* ============================================================
 *  LOGO COLOUR EXTRACTION
 *
 *  Scans a pixel-art logo and derives a bright, saturated
 *  accent colour from its dominant hue.
 * ============================================================ */

/* Helper function to convert RGB to HSV */
static inline void rgb_to_hsv(uint8_t r, uint8_t g, uint8_t b, float *h, float *s, float *v)
{
    float rf = r / 255.0f;
    float gf = g / 255.0f;
    float bf = b / 255.0f;

    float max = (rf > gf) ? ((rf > bf) ? rf : bf) : ((gf > bf) ? gf : bf);
    float min = (rf < gf) ? ((rf < bf) ? rf : bf) : ((gf < bf) ? gf : bf);
    float delta = max - min;

    *v = max; // Brightness / Value

    if (max < 0.00001f) {
        *s = 0;
        *h = 0; // Undefined hue
        return;
    }

    *s = delta / max; // Saturation

    if (delta < 0.00001f) {
        *h = 0; // Gray/achromatic
        return;
    }

    if (rf >= max) {
        *h = (gf - bf) / delta;
    } else if (gf >= max) {
        *h = 2.0f + (bf - rf) / delta;
    } else {
        *h = 4.0f + (rf - gf) / delta;
    }

    *h *= 60.0f;
    if (*h < 0.0f) {
        *h += 360.0f;
    }
}

/* Helper function to convert HSV back to LVGL Color with boosted S & V */
static inline lv_color_t hsv_to_lv_color(float h, float s, float v)
{
    // Force high saturation and maximum brightness for a bright accent color
    if (s < 0.70f) s = 0.70f;
    v = 1.0f;

    float c = v * s;
    float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float rf = 0, gf = 0, bf = 0;

    if (h < 60.0f)       { rf = c; gf = x; bf = 0; }
    else if (h < 120.0f) { rf = x; gf = c; bf = 0; }
    else if (h < 180.0f) { rf = 0; gf = c; bf = x; }
    else if (h < 240.0f) { rf = 0; gf = x; bf = c; }
    else if (h < 300.0f) { rf = x; gf = 0; bf = c; }
    else                 { rf = c; gf = 0; bf = x; }

    uint8_t r = (uint8_t)((rf + m) * 255.0f);
    uint8_t g = (uint8_t)((gf + m) * 255.0f);
    uint8_t b = (uint8_t)((bf + m) * 255.0f);

    return lv_color_make(r, g, b);
}

static inline lv_color_t compute_dominant_color(const lv_img_dsc_t *dsc)
{
    if (!dsc || !dsc->data) {
        return LV_COLOR_PRIMARY;
    }

    const uint8_t *data = dsc->data;
    int total_pixels = dsc->header.w * dsc->header.h;
    const int BYTES_PER_PIXEL = 3;

    #define MAX_COLORS 128

    typedef struct {
        float hue;
        uint32_t count;
    } hue_bin_t;

    hue_bin_t hue_bins[18]; // Divide 360 degrees into 18 bins (20 deg each)
    memset(hue_bins, 0, sizeof(hue_bins));

    for (int i = 0; i < total_pixels; i++) {
        const uint8_t *pixel = &data[i * BYTES_PER_PIXEL];

        uint16_t rgb565 = ((uint16_t)pixel[1] << 8) | pixel[0];
        uint8_t alpha = pixel[2];

        // Ignore transparent pixels
        if (alpha < 128) continue;

        // Extract RGB565 and expand to 8-bit
        uint8_t r = ((rgb565 >> 11) & 0x1F) * 255 / 31;
        uint8_t g = ((rgb565 >> 5) & 0x3F) * 255 / 63;
        uint8_t b = (rgb565 & 0x1F) * 255 / 31;

        float h, s, v;
        rgb_to_hsv(r, g, b, &h, &s, &v);

        /*
         * Filter out dark colors (Value < 30%) and
         * washed-out grays/whites (Saturation < 20%)
         */
        if (v < 0.30f || s < 0.20f) {
            continue;
        }

        // Group into hue bins (0-17)
        int bin = (int)(h / 20.0f) % 18;
        hue_bins[bin].hue += h; // Accumulate hue to average later
        hue_bins[bin].count++;
    }

    // Find the most frequent Hue group
    int max_bin = -1;
    uint32_t max_count = 0;

    for (int i = 0; i < 18; i++) {
        if (hue_bins[i].count > max_count) {
            max_count = hue_bins[i].count;
            max_bin = i;
        }
    }

    // Fallback if no colorful pixel was found
    if (max_bin == -1 || max_count == 0) {
        return LV_COLOR_PRIMARY;
    }

    // Average hue of the winning bin
    float dominant_hue = hue_bins[max_bin].hue / hue_bins[max_bin].count;

    // Convert Hue back to RGB with 100% Value and high Saturation
    return hsv_to_lv_color(dominant_hue, 0.85f, 1.0f);

    #undef MAX_COLORS
}

/* ============================================================
 *  CONTRAST HELPERS
 * ============================================================ */

static inline lv_color_t darken_text_color(lv_color_t c, float factor)
{
    uint32_t r = color_get_r8(c);
    uint32_t g = color_get_g8(c);
    uint32_t b = color_get_b8(c);

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

/* ============================================================
 *  CONTRAST TEXT — picks a readable text color for any bg
 * ============================================================ */

static inline float color_luminance(lv_color_t c)
{
    float r = color_get_r8(c) / 255.0f;
    float g = color_get_g8(c) / 255.0f;
    float b = color_get_b8(c) / 255.0f;

    r = (r <= 0.03928f) ? (r / 12.92f) : powf((r + 0.055f) / 1.055f, 2.4f);
    g = (g <= 0.03928f) ? (g / 12.92f) : powf((g + 0.055f) / 1.055f, 2.4f);
    b = (b <= 0.03928f) ? (b / 12.92f) : powf((b + 0.055f) / 1.055f, 2.4f);

    return 0.2126f * r + 0.7152f * g + 0.0722f * b;
}

static inline float color_contrast(lv_color_t a, lv_color_t b)
{
    float la = color_luminance(a);
    float lb = color_luminance(b);
    if (la < lb) {
        float t = la;
        la = lb;
        lb = t;
    }
    return (la + 0.05f) / (lb + 0.05f);
}

/* Returns white text on dark/colored bgs, dark text on light bgs */
static inline lv_color_t contrast_text_color(lv_color_t bg)
{
    lv_color_t light_text = LV_COLOR_TEXT_ON_DARK;
    lv_color_t dark_text = LV_COLOR_TEXT_ON_LIGHT;
    if (color_contrast(light_text, bg) >= color_contrast(dark_text, bg)) {
        return light_text;
    }
    return dark_text;
}

#endif /* COLOR_PALETTE_H */
