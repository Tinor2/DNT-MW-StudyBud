#ifndef STUDYBUD_THEME_H
#define STUDYBUD_THEME_H

#include "lvgl.h"
#include "app_state.h"

/* === PRIMARY — Forest Green === */
#define LV_COLOR_PRIMARY        lv_color_hex(0x3B7D4B)
#define LV_COLOR_PRIMARY_LIGHT  lv_color_hex(0x74A77A)
#define LV_COLOR_PRIMARY_DARK   lv_color_hex(0x2B5A35)

/* === SECONDARY — Sage Green === */
#define LV_COLOR_SECONDARY      lv_color_hex(0x8FBF9A)
#define LV_COLOR_SECONDARY_LIGHT lv_color_hex(0xC8E0CE)
#define LV_COLOR_SECONDARY_DARK lv_color_hex(0x66916F)

/* === BACKGROUNDS === */
#define LV_COLOR_BG             lv_color_hex(0xEFF4EA)
#define LV_COLOR_BG_CARD        lv_color_hex(0xFFFFFF)
#define LV_COLOR_SURFACE        lv_color_hex(0xDDE5D6)
#define LV_COLOR_BORDER         lv_color_hex(0xBFC8B9)

/* === TEXT === */
#define LV_COLOR_TEXT            lv_color_hex(0x2A3B2E)
#define LV_COLOR_TEXT_ON_LIGHT   lv_color_hex(0x2A3B2E) /* new default text: deep ink-green */
#define LV_COLOR_TEXT_ON_DARK    lv_color_hex(0xFFFFFF) /* white, used on dark/colored bgs   */
#define LV_COLOR_TEXT_SECONDARY lv_color_hex(0x6B7094)
#define LV_COLOR_TEXT_MUTED     lv_color_hex(0x9B9FBA)

/* === STATUS === */
#define LV_COLOR_SUCCESS        lv_color_hex(0x5E9F72)
#define LV_COLOR_WARNING        lv_color_hex(0xE0A84C)
#define LV_COLOR_ERROR          lv_color_hex(0xC97A7A)
#define LV_COLOR_INFO           lv_color_hex(0x5FAF8B)

/* === FEATURE ACCENTS === */
#define LV_COLOR_TIMER          lv_color_hex(0xE0A84C)
#define LV_COLOR_TIMER_BREAK    lv_color_hex(0x5FAF8B)
#define LV_COLOR_WATER          lv_color_hex(0x5FAF8B)
#define LV_COLOR_BREATHING      lv_color_hex(0x3B7D4B)
#define LV_COLOR_BREATHING_LIGHT lv_color_hex(0x74A77A)
#define LV_COLOR_BREATHING_DARK  lv_color_hex(0x2B5A35)

/* === OPACITY === */
#define LV_OPACITY_BG           LV_OPA_10
#define LV_OPACITY_DISABLED     LV_OPA_40
#define LV_OPACITY_MUTED        LV_OPA_60

/* === FONTS (generated via lv_font_conv, registered in LVGL_Driver.c) === */
// extern const lv_font_t font_12;
// extern const lv_font_t font_16;
// extern const lv_font_t font_20_bold;
// extern const lv_font_t font_28;
// extern const lv_font_t font_36;

/* === SCREEN ACCENT COLORS (dynamically set by screen_menu) === */
/* Colour helper functions live in color_palette.h */

#endif /* STUDYBUD_THEME_H */
