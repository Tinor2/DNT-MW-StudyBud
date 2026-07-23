#include "screen_todos.h"
#include "ui_manager.h"
#include "studybud_theme.h"
#include "networking/app_state.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *TAG = "Screen_Todos";

#define DISPLAY_R   240
#define DISPLAY_CY  240
#define ROW_SPACING 64
#define MAX_ROW_W   420
#define MIN_VISIBLE_W 120
#define ROW_INNER_PAD 12
#define ROW_COL_GAP   10

static lv_color_t get_priority_color(int priority)
{
    switch (priority) {
        case 0:  return lv_color_make(0xC9, 0x7A, 0x7A);
        case 1:  return lv_color_make(0xE0, 0xA8, 0x4C);
        case 2:  return lv_color_make(0x7D, 0xBF, 0x9E);
        default: return lv_color_make(0x9B, 0x9F, 0xBA);
    }
}

typedef struct {
    const char *text;
    int priority;
    bool completed;
} todo_item_t;

static todo_item_t todo_items[] = {
    {"Finish project report", 0, false},
    {"Reply to emails",       1, false},
    {"Buy groceries",         2, false},
    {"Schedule dentist",      1, false},
    {"Read 20 pages",         2, false},
    {"Clean desk and also do a bunch more things blah blah blah",            0, false},
};
#define TODO_COUNT (sizeof(todo_items) / sizeof(todo_items[0]))

static lv_obj_t *screen;
static lv_obj_t *todo_container;
static lv_obj_t *arrow_up_label;
static lv_obj_t *arrow_down_label;
static lv_obj_t *count_label;

static lv_obj_t *task_rows[TODO_COUNT];
static lv_obj_t *task_indicators[TODO_COUNT];
static lv_obj_t *task_labels[TODO_COUNT];
static lv_obj_t *task_dots[TODO_COUNT];

static lv_obj_t *focused_row;
static lv_timer_t *radial_timer;

typedef struct { lv_obj_t *row; } shuffle_data_t;

static void update_focus_styles(void);
static void update_arrow_visibility(void);
static void update_task_count(void);
static void complete_task(lv_obj_t *row);
static void uncomplete_task(lv_obj_t *row);
static void shuffle_timer_cb(lv_timer_t *timer);
static void initial_scroll_cb(lv_timer_t *timer);

static bool is_row_completed(lv_obj_t *row)
{
    for (int i = 0; i < (int)TODO_COUNT; i++)
        if (task_rows[i] == row) return todo_items[i].completed;
    return false;
}

static lv_obj_t *find_first_uncompleted(void)
{
    uint32_t cnt = lv_obj_get_child_cnt(todo_container);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t *child = lv_obj_get_child(todo_container, i);
        if (!is_row_completed(child)) return child;
    }
    return NULL;
}

static void update_indicator(int index)
{
    lv_obj_t *ind = task_indicators[index];
    if (!ind) return;
    if (todo_items[index].completed) {
        lv_obj_set_style_bg_color(ind, LV_COLOR_PRIMARY, 0);
        lv_obj_set_style_border_color(ind, LV_COLOR_PRIMARY, 0);
        lv_label_set_text(ind, LV_SYMBOL_OK);
        lv_obj_set_style_text_color(ind, LV_COLOR_BG_CARD, 0);
        lv_obj_set_style_text_font(ind, &lv_font_montserrat_14, 0);
    } else {
        lv_obj_set_style_bg_color(ind, LV_COLOR_BG_CARD, 0);
        lv_obj_set_style_border_color(ind, LV_COLOR_BORDER, 0);
        lv_label_set_text(ind, "");
    }
}

static void update_task_count(void)
{
    if (!count_label) return;
    int done = 0;
    for (int i = 0; i < (int)TODO_COUNT; i++)
        if (todo_items[i].completed) done++;
    lv_label_set_text_fmt(count_label, "%d/%d done", done, (int)TODO_COUNT);
}

static lv_obj_t *create_task_row(lv_obj_t *parent, int index)
{
    todo_item_t *item = &todo_items[index];

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, MAX_ROW_W, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_row(row, 0, 0);
    lv_obj_set_style_pad_column(row, ROW_COL_GAP, 0);
    lv_obj_set_style_pad_all(row, ROW_INNER_PAD, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_min_width(row, 0, 0);
    task_rows[index] = row;

    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, item->text);
    lv_obj_set_flex_grow(label, 1);
    lv_obj_set_style_text_color(label, LV_COLOR_TEXT, 0);
    task_labels[index] = label;
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_opa(label, LV_OPA_80, 0);
    lv_obj_set_style_pad_ver(row, 6, 0);

    lv_obj_t *ind = lv_label_create(row);
    lv_obj_set_size(ind, 24, 24);
    lv_obj_set_style_radius(ind, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ind, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(ind, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_border_color(ind, LV_COLOR_BORDER, 0);
    lv_obj_set_style_border_width(ind, 2, 0);
    lv_obj_set_style_pad_all(ind, 0, 0);
    lv_obj_set_style_text_align(ind, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ind, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_text_font(ind, &lv_font_montserrat_14, 0);
    lv_label_set_text(ind, "");
    task_indicators[index] = ind;

    lv_obj_t *dot = lv_obj_create(row);
    lv_obj_set_size(dot, 10, 10);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, get_priority_color(item->priority), 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    task_dots[index] = dot;

    return row;
}

static void update_focus_styles(void)
{
    for (int i = 0; i < (int)TODO_COUNT; i++) {
        lv_obj_t *row = task_rows[i];
        lv_obj_t *label = task_labels[i];
        if (!row || !label) continue;

        if (row == focused_row) {
            lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
            lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
            lv_obj_set_style_pad_ver(row, 12, 0);
            lv_obj_set_style_text_color(label,
                todo_items[i].completed ? lv_color_make(0x80, 0x80, 0x80) : LV_COLOR_TEXT, 0);
        } else {
            lv_obj_set_style_text_font(label, &lv_font_montserrat_16, 0);
            lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
            lv_obj_set_style_pad_ver(row, 6, 0);
            lv_obj_set_style_text_opa(label,
                todo_items[i].completed ? LV_OPA_60 : LV_OPA_80, 0);
            lv_obj_set_style_text_color(label,
                todo_items[i].completed ? lv_color_make(0x80, 0x80, 0x80) : LV_COLOR_TEXT, 0);
        }
    }
}

static void update_arrow_visibility(void)
{
    lv_coord_t st = lv_obj_get_scroll_top(todo_container);
    lv_coord_t sb = lv_obj_get_scroll_bottom(todo_container);
    if (st <= 0) lv_obj_add_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_clear_flag(arrow_up_label, LV_OBJ_FLAG_HIDDEN);
    if (sb <= 0) lv_obj_add_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_clear_flag(arrow_down_label, LV_OBJ_FLAG_HIDDEN);
}

static void apply_radial_scroll(void)
{
    if (!todo_container) return;

    lv_obj_update_layout(todo_container);

    bool any_hidden_top = false;
    bool any_hidden_bottom = false;

    for (int i = 0; i < (int)TODO_COUNT; i++) {
        lv_obj_t *row = task_rows[i];
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

static void update_row_positions(void)
{
    int uncompleted_y = 0;
    int completed_y = 0;
    int uncompleted_count = 0;
    int completed_count = 0;

    for (int i = 0; i < (int)TODO_COUNT; i++) {
        if (todo_items[i].completed) completed_count++;
        else uncompleted_count++;
    }

    int uncompleted_start = 0;
    int completed_start = uncompleted_count * ROW_SPACING;

    for (int i = 0; i < (int)TODO_COUNT; i++) {
        lv_obj_t *row = task_rows[i];
        if (!row) continue;

        lv_coord_t x = (480 - MAX_ROW_W) / 2;
        if (todo_items[i].completed) {
            lv_obj_set_pos(row, x, completed_start + completed_y * ROW_SPACING);
            completed_y++;
        } else {
            lv_obj_set_pos(row, x, uncompleted_start + uncompleted_y * ROW_SPACING);
            uncompleted_y++;
        }
    }
}

static void shuffle_timer_cb(lv_timer_t *timer)
{
    shuffle_data_t *data = (shuffle_data_t *)timer->user_data;
    lv_obj_t *row = data->row;

    lv_obj_set_style_bg_opa(row, LV_OPA_40, 0);

    update_row_positions();

    lv_obj_t *next = find_first_uncompleted();
    focused_row = next ? next : row;

    update_focus_styles();
    update_task_count();
    if (focused_row) lv_obj_scroll_to_view(focused_row, LV_ANIM_ON);
    update_arrow_visibility();
    free(data);
    lv_timer_del(timer);
}

static void complete_task(lv_obj_t *row)
{
    int idx = -1;
    for (int i = 0; i < (int)TODO_COUNT; i++)
        if (task_rows[i] == row) { idx = i; break; }
    if (idx < 0 || todo_items[idx].completed) return;

    todo_items[idx].completed = true;
    update_indicator(idx);
    lv_obj_set_style_text_decor(task_labels[idx], LV_TEXT_DECOR_STRIKETHROUGH, 0);
    lv_obj_set_style_text_color(task_labels[idx], lv_color_make(0x80, 0x80, 0x80), 0);
    lv_obj_set_style_bg_opa(task_dots[idx], LV_OPA_40, 0);

    {
        app_state_broadcast_todo_toggled(idx, todo_items[idx].text, true);
    }

    shuffle_data_t *data = malloc(sizeof(shuffle_data_t));
    data->row = row;
    lv_timer_t *t = lv_timer_create(shuffle_timer_cb, 500, data);
    t->repeat_count = 1;
    ESP_LOGI(TAG, "Task %d completed: %s", idx, todo_items[idx].text);
}

static void uncomplete_task(lv_obj_t *row)
{
    int idx = -1;
    for (int i = 0; i < (int)TODO_COUNT; i++)
        if (task_rows[i] == row) { idx = i; break; }
    if (idx < 0 || !todo_items[idx].completed) return;

    todo_items[idx].completed = false;
    update_indicator(idx);
    lv_obj_set_style_text_decor(task_labels[idx], LV_TEXT_DECOR_NONE, 0);
    lv_obj_set_style_text_color(task_labels[idx], LV_COLOR_TEXT, 0);
    lv_obj_set_style_bg_opa(task_dots[idx], LV_OPA_COVER, 0);

    {
        app_state_broadcast_todo_toggled(idx, todo_items[idx].text, false);
    }

    update_row_positions();
    update_focus_styles();
    focused_row = row;
    lv_obj_scroll_to_view(focused_row, LV_ANIM_ON);
    update_arrow_visibility();
    ESP_LOGI(TAG, "Task %d un-completed: %s", idx, todo_items[idx].text);
}

lv_obj_t *screen_todos_create(void)
{
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, LV_COLOR_BG, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    todo_container = lv_obj_create(screen);
    lv_obj_set_size(todo_container, 480, 480);
    lv_obj_align(todo_container, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(todo_container, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(todo_container, LV_COLOR_BG_CARD, 0);
    lv_obj_set_style_bg_opa(todo_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(todo_container, 0, 0);
    lv_obj_set_style_pad_all(todo_container, 0, 0);
    lv_obj_set_scroll_dir(todo_container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(todo_container, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_scroll_snap_y(todo_container, LV_SCROLL_SNAP_CENTER);
    lv_obj_add_event_cb(todo_container, scroll_cb, LV_EVENT_SCROLL, NULL);

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

    count_label = lv_label_create(screen);
    lv_label_set_text(count_label, "0/6 done");
    lv_obj_set_style_text_font(count_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(count_label, LV_COLOR_TEXT_MUTED, 0);
    lv_obj_align(count_label, LV_ALIGN_BOTTOM_MID, 0, -60);

    for (int i = 0; i < (int)TODO_COUNT; i++) {
        create_task_row(todo_container, i);
        lv_obj_set_pos(task_rows[i], (480 - MAX_ROW_W) / 2, i * ROW_SPACING);
    }

    focused_row = task_rows[0];
    update_focus_styles();
    update_task_count();
    update_arrow_visibility();

    apply_radial_scroll();
    radial_timer = lv_timer_create(radial_timer_cb, 50, NULL);

    lv_timer_t *init_timer = lv_timer_create(initial_scroll_cb, 50, NULL);
    init_timer->repeat_count = 1;

    ESP_LOGI(TAG, "Todos screen created with %d items", (int)TODO_COUNT);
    return screen;
}

void screen_todos_encoder_event(lv_indev_data_t *data)
{
    if (!todo_container) return;

    if (data->enc_diff != 0) {
        int cur_idx = -1;
        for (int i = 0; i < (int)TODO_COUNT; i++)
            if (task_rows[i] == focused_row) { cur_idx = i; break; }
        if (cur_idx < 0) return;

        int new_idx = cur_idx + data->enc_diff;
        if (new_idx < 0) new_idx = (int)TODO_COUNT - 1;
        if (new_idx >= (int)TODO_COUNT) new_idx = 0;

        focused_row = task_rows[new_idx];
        update_focus_styles();
        lv_obj_scroll_to_view(focused_row, LV_ANIM_ON);
    }

    if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
        if (focused_row) {
            if (is_row_completed(focused_row))
                uncomplete_task(focused_row);
            else
                complete_task(focused_row);
        }
    }
}
