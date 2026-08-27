#include "screen_timer_presets.h"
#include "ui_manager.h"
#include "color_palette.h"
#include "../app_state.h"
#include "../utils/timer_store.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "Screen_TimerPresets";

#define SCREEN_W       480
#define SCREEN_H       480
#define SCREEN_CY      (SCREEN_H / 2)
#define FOCUSED_SIZE   340
#define SMALL_SIZE     90
#define CARD_SPACING   380
#define ANIM_SPEED     3.5
#define ACTION_BTN_SIZE 40
#define ACTION_BORDER_W 3

typedef enum {
    ACTION_EDIT,
    ACTION_PLAY,
    ACTION_PRIMARY_COUNT
} action_focus_t;

typedef struct {
    lv_obj_t *card;
    lv_obj_t *lbl_icon;
    lv_obj_t *lbl_name;
    lv_obj_t *lbl_time;
    lv_obj_t *lbl_sub;
    bool      has_actions;
    lv_obj_t *action_btns[ACTION_PRIMARY_COUNT];
    lv_obj_t *action_lbls[ACTION_PRIMARY_COUNT];
    lv_obj_t *back_btn;
    lv_obj_t *back_lbl;
    int  target_x;
    int  target_size;
    lv_opa_t target_opa;
    int  cur_x;
    int  cur_size;
    lv_opa_t cur_opa;
} carousel_card_t;

static lv_obj_t *screen;
static lv_obj_t *lbl_title;

static carousel_card_t cards[TIMER_STORE_MAX_PRESETS + 1];
static int preset_count = 0;
static int total_cards  = 0;

static int  focused_index   = 0;
static bool action_mode     = false;
static action_focus_t action_idx = ACTION_PLAY;

static lv_timer_t *anim_timer = NULL;

static void update_focus(bool immediate);
static void update_card_border(void);
static void update_action_highlight(void);

static void format_time(char *buf, size_t len, uint32_t sec)
{
    uint32_t h = sec / 3600;
    uint32_t m = (sec % 3600) / 60;
    uint32_t s = sec % 60;
    if (h > 0)
        snprintf(buf, len, "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
    else
        snprintf(buf, len, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
}

static void refresh_card_content(carousel_card_t *c, int index)
{
    if (index < preset_count) {
        const timer_preset_t *p = timer_store_get(index);
        if (!p) return;

        lv_label_set_text(c->lbl_name, p->name);

        char time_buf[16];
        uint32_t sec = (p->type == TIMER_TYPE_POMODORO) ? p->session_sec : p->duration_sec;
        format_time(time_buf, sizeof(time_buf), sec);
        lv_label_set_text(c->lbl_time, time_buf);

        if (p->type == TIMER_TYPE_POMODORO) {
            char sub_buf[48];
            snprintf(sub_buf, sizeof(sub_buf), "Short %lum   Long %lum",
                     (unsigned long)(p->short_break_sec / 60),
                     (unsigned long)(p->long_break_sec / 60));
            lv_label_set_text(c->lbl_sub, sub_buf);
            lv_obj_clear_flag(c->lbl_sub, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_label_set_text(c->lbl_sub, "Standard");
            lv_obj_clear_flag(c->lbl_sub, LV_OBJ_FLAG_HIDDEN);
        }

        lv_label_set_text(c->lbl_icon, LV_SYMBOL_PLAY);
    } else {
        lv_label_set_text(c->lbl_icon, LV_SYMBOL_PLUS);
        lv_label_set_text(c->lbl_name, "New\nTimer");
        lv_label_set_text(c->lbl_time, "");
        lv_label_set_text(c->lbl_sub, "");
    }
}

static void create_card(carousel_card_t *c, int index)
{
    bool is_add_card = (index >= preset_count);
    c->has_actions = !is_add_card;

    lv_obj_t *card = lv_obj_create(screen);
    lv_obj_set_size(card, FOCUSED_SIZE, FOCUSED_SIZE);
    lv_obj_set_style_radius(card, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(card, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);
    lv_obj_set_style_border_color(card, theme_accent(SCREEN_TIMER_PRESETS), 0);
    lv_obj_set_style_shadow_width(card, 10, 0);
    lv_obj_set_style_shadow_opa(card, LV_OPA_20, 0);
    lv_obj_set_style_pad_all(card, 0, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    c->card = card;

    lv_obj_t *icon = lv_label_create(card);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(icon, theme_accent(SCREEN_TIMER_PRESETS), 0);
    lv_obj_align(icon, LV_ALIGN_CENTER, 0, -60);
    c->lbl_icon = icon;

    lv_obj_t *name = lv_label_create(card);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(name, LV_COLOR_TEXT, 0);
    lv_label_set_long_mode(name, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(name, FOCUSED_SIZE - 60);
    lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(name, LV_ALIGN_CENTER, 0, -20);
    c->lbl_name = name;

    lv_obj_t *time = lv_label_create(card);
    lv_obj_set_style_text_font(time, &lv_font_montserrat_36, 0);
    lv_obj_set_style_text_color(time, LV_COLOR_TEXT, 0);
    lv_obj_align(time, LV_ALIGN_CENTER, 0, 20);
    c->lbl_time = time;

    lv_obj_t *sub = lv_label_create(card);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sub, LV_COLOR_TEXT_SECONDARY, 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, 65);
    c->lbl_sub = sub;

    if (c->has_actions) {
        const char *a_icons[] = { LV_SYMBOL_SETTINGS, LV_SYMBOL_PLAY };
        for (int a = 0; a < ACTION_PRIMARY_COUNT; a++) {
            lv_obj_t *btn = lv_btn_create(card);
            lv_obj_set_size(btn, ACTION_BTN_SIZE, ACTION_BTN_SIZE);
            lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, (a == 0 ? -32 : 32), -45);
            lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_bg_color(btn, theme_accent_dark(SCREEN_TIMER_PRESETS), 0);
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
            lv_obj_set_style_shadow_width(btn, 0, 0);
            lv_obj_set_style_shadow_opa(btn, LV_OPA_TRANSP, 0);
            lv_obj_set_style_border_width(btn, 0, 0);
            lv_obj_set_style_border_color(btn, theme_accent_light(SCREEN_TIMER_PRESETS), 0);
            lv_obj_set_style_pad_all(btn, 0, 0);
            c->action_btns[a] = btn;

            lv_obj_t *lbl = lv_label_create(btn);
            lv_label_set_text(lbl, a_icons[a]);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
            lv_obj_set_style_text_color(lbl, contrast_text_color(theme_accent_dark(SCREEN_TIMER_PRESETS)), 0);
            lv_obj_center(lbl);
            c->action_lbls[a] = lbl;
        }

        lv_obj_t *back = lv_btn_create(card);
        lv_obj_set_size(back, 90, 28);
        lv_obj_align(back, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_style_radius(back, 14, 0);
        lv_obj_set_style_bg_color(back, theme_accent_dark(SCREEN_TIMER_PRESETS), 0);
        lv_obj_set_style_bg_opa(back, LV_OPA_COVER, 0);
        lv_obj_set_style_shadow_width(back, 0, 0);
        lv_obj_set_style_shadow_opa(back, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(back, 0, 0);
        lv_obj_set_style_border_color(back, theme_accent_light(SCREEN_TIMER_PRESETS), 0);
        lv_obj_set_style_pad_all(back, 0, 0);
        lv_obj_add_flag(back, LV_OBJ_FLAG_HIDDEN);
        c->back_btn = back;

        lv_obj_t *back_l = lv_label_create(back);
        lv_label_set_text(back_l, "BACK");
        lv_obj_set_style_text_font(back_l, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(back_l, contrast_text_color(theme_accent_dark(SCREEN_TIMER_PRESETS)), 0);
        lv_obj_center(back_l);
        c->back_lbl = back_l;
    } else {
        for (int a = 0; a < ACTION_PRIMARY_COUNT; a++) {
            c->action_btns[a] = NULL;
            c->action_lbls[a] = NULL;
        }
        c->back_btn = NULL;
        c->back_lbl = NULL;
    }

    refresh_card_content(c, index);
}

static void anim_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    bool any_moving = false;

    for (int i = 0; i < total_cards; i++) {
        carousel_card_t *c = &cards[i];

        bool moving_x    = (c->cur_x    != c->target_x);
        bool moving_size = (c->cur_size != c->target_size);
        bool moving_opa  = (c->cur_opa  != c->target_opa);

        if (!moving_x && !moving_size && !moving_opa) continue;
        any_moving = true;

        /* Smooth Exponential Interpolation with tight convergence threshold */
        if (moving_x) {
            int diff = c->target_x - c->cur_x;
            if (abs(diff) <= 2) c->cur_x = c->target_x;
            else c->cur_x += (diff > 0) ? (diff + 1) / ANIM_SPEED : (diff - 1) / ANIM_SPEED;
        }

        if (moving_size) {
            int diff = c->target_size - c->cur_size;
            if (abs(diff) <= 2) c->cur_size = c->target_size;
            else c->cur_size += (diff > 0) ? (diff + 1) / ANIM_SPEED : (diff - 1) / ANIM_SPEED;
        }

        if (moving_opa) {
            int diff = (int)c->target_opa - (int)c->cur_opa;
            if (abs(diff) <= 2) c->cur_opa = c->target_opa;
            else c->cur_opa += (diff > 0) ? (diff + 1) / ANIM_SPEED : (diff - 1) / ANIM_SPEED;
        }

        /* Update Position */
        lv_obj_set_x(c->card, c->cur_x);
        lv_obj_set_y(c->card, SCREEN_CY - c->cur_size / 2);
        lv_obj_set_style_opa(c->card, c->cur_opa, 0);

        /* * ONLY update widget dimensions if size actually changed.
         * Preventing unnecessary calls to lv_obj_set_size saves massive CPU draw cycles.
         */
        if (moving_size) {
            lv_obj_set_size(c->card, c->cur_size, c->cur_size);
        }

        /* Visibility checks */
        bool on_screen = (c->cur_x > -FOCUSED_SIZE && c->cur_x < SCREEN_W + FOCUSED_SIZE);
        bool show_content = on_screen && (c->cur_size > SMALL_SIZE + 40);

        if (show_content && i < preset_count) {
            if (lv_obj_has_flag(c->lbl_name, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_clear_flag(c->lbl_name, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(c->lbl_time, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(c->lbl_sub, LV_OBJ_FLAG_HIDDEN);
                if (c->has_actions) {
                    for (int a = 0; a < ACTION_PRIMARY_COUNT; a++)
                        lv_obj_clear_flag(c->action_btns[a], LV_OBJ_FLAG_HIDDEN);
                }
            }
        } else {
            if (!lv_obj_has_flag(c->lbl_name, LV_OBJ_FLAG_HIDDEN)) {
                lv_obj_add_flag(c->lbl_name, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(c->lbl_time, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(c->lbl_sub, LV_OBJ_FLAG_HIDDEN);
                if (c->has_actions) {
                    for (int a = 0; a < ACTION_PRIMARY_COUNT; a++)
                        lv_obj_add_flag(c->action_btns[a], LV_OBJ_FLAG_HIDDEN);
                    if (c->back_btn)
                        lv_obj_add_flag(c->back_btn, LV_OBJ_FLAG_HIDDEN);
                }
            }
        }
    }

    if (!any_moving && anim_timer) {
        lv_timer_pause(anim_timer);
    }
}
static void update_card_border(void)
{
    for (int i = 0; i < total_cards; i++) {
        carousel_card_t *c = &cards[i];
        bool selected = action_mode && (i == focused_index);
        lv_obj_set_style_border_width(c->card, selected ? ACTION_BORDER_W : 0, 0);
        lv_obj_set_style_border_color(c->card, theme_accent_light(SCREEN_TIMER_PRESETS), 0);
    }
}

static void update_action_highlight(void)
{
    carousel_card_t *c = &cards[focused_index];
    if (!c->has_actions) return;

    for (int a = 0; a < ACTION_PRIMARY_COUNT; a++) {
        if (!c->action_btns[a]) continue;
        bool focused = action_mode && (a == (int)action_idx);
        lv_color_t bg = focused ? theme_accent(SCREEN_TIMER_PRESETS) : theme_accent_dark(SCREEN_TIMER_PRESETS);
        lv_obj_set_style_bg_color(c->action_btns[a], bg, 0);
        lv_obj_set_style_text_color(c->action_lbls[a], contrast_text_color(bg), 0);
        lv_obj_set_style_border_width(c->action_btns[a],
            focused ? ACTION_BORDER_W : 0, 0);
        lv_obj_set_style_border_color(c->action_btns[a], theme_accent_light(SCREEN_TIMER_PRESETS), 0);
    }

    if (c->back_btn) {
        bool show_back = action_mode;
        bool back_focused = action_mode && (action_idx == ACTION_PLAY + 1);
        if (show_back)
            lv_obj_clear_flag(c->back_btn, LV_OBJ_FLAG_HIDDEN);
        else
            lv_obj_add_flag(c->back_btn, LV_OBJ_FLAG_HIDDEN);
        lv_color_t back_bg = back_focused ? theme_accent(SCREEN_TIMER_PRESETS) : theme_accent_dark(SCREEN_TIMER_PRESETS);
        lv_obj_set_style_bg_color(c->back_btn, back_bg, 0);
        lv_obj_set_style_text_color(c->back_lbl, contrast_text_color(back_bg), 0);
        lv_obj_set_style_border_width(c->back_btn,
            back_focused ? ACTION_BORDER_W : 0, 0);
        lv_obj_set_style_border_color(c->back_btn, theme_accent_light(SCREEN_TIMER_PRESETS), 0);
    }

    update_card_border();
}

static void update_focus(bool immediate)
{
    for (int i = 0; i < total_cards; i++) {
        int diff = i - focused_index;
        int target_x;
        int target_size;
        lv_opa_t target_opa;

        if (diff == 0) {
            target_x    = (SCREEN_W - FOCUSED_SIZE) / 2;
            target_size = FOCUSED_SIZE;
            target_opa  = LV_OPA_COVER;
        } else if (diff == -1) {
            target_x    = (SCREEN_W - FOCUSED_SIZE) / 2 - CARD_SPACING;
            target_size = SMALL_SIZE;
            target_opa  = LV_OPA_40;
        } else if (diff == 1) {
            target_x    = (SCREEN_W - FOCUSED_SIZE) / 2 + CARD_SPACING;
            target_size = SMALL_SIZE;
            target_opa  = LV_OPA_40;
        } else {
            target_x    = (diff < 0) ? -FOCUSED_SIZE - 80 : SCREEN_W + 80;
            target_size = SMALL_SIZE;
            target_opa  = LV_OPA_TRANSP;
        }

        cards[i].target_x    = target_x;
        cards[i].target_size = target_size;
        cards[i].target_opa  = target_opa;

        if (immediate) {
            cards[i].cur_x    = target_x;
            cards[i].cur_size = target_size;
            cards[i].cur_opa  = target_opa;

            lv_obj_set_size(cards[i].card, target_size, target_size);
            lv_obj_set_x(cards[i].card, target_x);
            lv_obj_set_y(cards[i].card, SCREEN_CY - target_size / 2);
            lv_obj_set_style_opa(cards[i].card, target_opa, 0);

            bool show = (diff == 0);
            if (show) {
                lv_obj_clear_flag(cards[i].lbl_name, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(cards[i].lbl_time, LV_OBJ_FLAG_HIDDEN);
                lv_obj_clear_flag(cards[i].lbl_sub, LV_OBJ_FLAG_HIDDEN);
                if (cards[i].has_actions) {
                    for (int a = 0; a < ACTION_PRIMARY_COUNT; a++)
                        lv_obj_clear_flag(cards[i].action_btns[a], LV_OBJ_FLAG_HIDDEN);
                }
                refresh_card_content(&cards[i], i);
            } else {
                lv_obj_add_flag(cards[i].lbl_name, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(cards[i].lbl_time, LV_OBJ_FLAG_HIDDEN);
                lv_obj_add_flag(cards[i].lbl_sub, LV_OBJ_FLAG_HIDDEN);
                if (cards[i].has_actions) {
                    for (int a = 0; a < ACTION_PRIMARY_COUNT; a++)
                        lv_obj_add_flag(cards[i].action_btns[a], LV_OBJ_FLAG_HIDDEN);
                    if (cards[i].back_btn)
                        lv_obj_add_flag(cards[i].back_btn, LV_OBJ_FLAG_HIDDEN);
                }
                if (i < preset_count)
                    lv_label_set_text(cards[i].lbl_icon, LV_SYMBOL_PLAY);
                else
                    lv_label_set_text(cards[i].lbl_icon, LV_SYMBOL_PLUS);
            }
        }
    }

    update_card_border();
    update_action_highlight();

    if (!immediate && anim_timer) {
        lv_timer_resume(anim_timer);
    }
}
void screen_timer_presets_encoder_event(lv_indev_data_t *data)
{
    if (total_cards == 0) return;

    if (action_mode) {
        carousel_card_t *fc = &cards[focused_index];

        if (data->enc_diff != 0 && fc->has_actions) {
            int new_action = (int)action_idx + data->enc_diff;
            if (new_action < 0) new_action = ACTION_PLAY + 1;
            if (new_action > ACTION_PLAY + 1) new_action = 0;
            action_idx = (action_focus_t)new_action;
            update_action_highlight();
        }

        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            if (!fc->has_actions) {
                action_mode = false;
                update_card_border();
                return;
            }

            if (action_idx == ACTION_PLAY + 1) {
                action_mode = false;
                update_action_highlight();
            } else if (action_idx == ACTION_PLAY) {
                timer_preset_t *p = timer_store_get(focused_index);
                if (p) {
                    timer_state_t *ts = app_state_get_timer();
                    ts->preset_id = p->id;
                    ts->is_running = false;
                    ts->phase_complete_awaiting_press = false;
                    if (p->type == TIMER_TYPE_POMODORO) {
                        ts->phase = TIMER_PHASE_SESSION;
                        ts->total_seconds = p->session_sec;
                        ts->remaining_seconds = p->session_sec;
                    } else {
                        ts->phase = TIMER_PHASE_SESSION;
                        ts->total_seconds = p->duration_sec;
                        ts->remaining_seconds = p->duration_sec;
                    }
                    ui_manager_switch_screen(SCREEN_TIMER);
                }
            } else if (action_idx == ACTION_EDIT) {
                timer_preset_t *p = timer_store_get(focused_index);
                if (p) app_state_get()->active_preset_id = p->id;
                ui_manager_switch_screen(SCREEN_TIMER_EDIT);
            }
        }
    } else {
        if (data->enc_diff != 0) {
            int new_idx = focused_index + data->enc_diff;
            if (new_idx < 0) new_idx = 0;
            if (new_idx >= total_cards) new_idx = total_cards - 1;
            if (new_idx != focused_index) {
                focused_index = new_idx;
                update_focus(false);
            }
        }

        if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
            if (focused_index < preset_count) {
                action_mode = true;
                action_idx = ACTION_PLAY;
                update_action_highlight();
            } else {
                app_state_get()->active_preset_id = -1;
                ui_manager_switch_screen(SCREEN_TIMER_EDIT);
            }
        }
    }
}

lv_obj_t *screen_timer_presets_create(void)
{
    if (anim_timer) {
        lv_timer_del(anim_timer);
        anim_timer = NULL;
    }
    if (screen) {
        lv_obj_del(screen);
    }

    timer_store_init();
    preset_count = timer_store_count();
    total_cards  = preset_count + 1;

    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, pastel_color(theme_accent(SCREEN_TIMER_PRESETS)), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    lbl_title = lv_label_create(screen);
    lv_label_set_text(lbl_title, "Timer");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl_title, LV_COLOR_TEXT, 0);
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 12);

    for (int i = 0; i < total_cards; i++) {
        create_card(&cards[i], i);
    }

    anim_timer = lv_timer_create(anim_timer_cb, 16, NULL);
    lv_timer_pause(anim_timer);

    focused_index = 0;
    action_mode   = false;
    update_focus(true);

    ESP_LOGI(TAG, "Timer presets screen created (%d presets + add)", preset_count);
    return screen;
}
