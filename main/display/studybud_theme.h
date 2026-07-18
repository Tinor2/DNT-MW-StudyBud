#ifndef STUDYBUD_THEME_H
#define STUDYBUD_THEME_H

#include "lvgl.h"

/* === PRIMARY — Periwinkle === */
#define LV_COLOR_PRIMARY        lv_color_hex(0x7E8EC9)
#define LV_COLOR_PRIMARY_LIGHT  lv_color_hex(0xA8B5DB)
#define LV_COLOR_PRIMARY_DARK   lv_color_hex(0x5A6AA8)

/* === SECONDARY — Eucalyptus === */
#define LV_COLOR_SECONDARY      lv_color_hex(0x7DBF9E)
#define LV_COLOR_SECONDARY_LIGHT lv_color_hex(0xA8D8BE)
#define LV_COLOR_SECONDARY_DARK lv_color_hex(0x5A9A78)

/* === BACKGROUNDS === */
#define LV_COLOR_BG             lv_color_hex(0xF2F3F8)
#define LV_COLOR_BG_CARD        lv_color_hex(0xFFFFFF)
#define LV_COLOR_SURFACE        lv_color_hex(0xE4E6ED)
#define LV_COLOR_BORDER         lv_color_hex(0xC8CCD8)

/* === TEXT === */
#define LV_COLOR_TEXT           lv_color_hex(0x2D3147)
#define LV_COLOR_TEXT_SECONDARY lv_color_hex(0x6B7094)
#define LV_COLOR_TEXT_MUTED     lv_color_hex(0x9B9FBA)

/* === STATUS === */
#define LV_COLOR_SUCCESS        lv_color_hex(0x7DBF9E)
#define LV_COLOR_WARNING        lv_color_hex(0xE0A84C)
#define LV_COLOR_ERROR          lv_color_hex(0xC97A7A)
#define LV_COLOR_INFO           lv_color_hex(0x6EAACC)

/* === FEATURE ACCENTS === */
#define LV_COLOR_TIMER          lv_color_hex(0xE0A84C)
#define LV_COLOR_TIMER_BREAK    lv_color_hex(0x6EAACC)
#define LV_COLOR_WATER          lv_color_hex(0x6EAACC)
#define LV_COLOR_BREATHING      lv_color_hex(0x7E8EC9)

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

#endif /* STUDYBUD_THEME_H */
