#include "timer_store.h"
#include "../app_state.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "Timer_Store";

static timer_preset_t s_cache[TIMER_STORE_MAX_PRESETS];
static int s_count = 0;

static void preset_to_timer(const preset_t *src, timer_preset_t *dst)
{
    dst->id = src->id;
    strncpy(dst->name, src->name, TIMER_STORE_NAME_LEN - 1);
    dst->name[TIMER_STORE_NAME_LEN - 1] = '\0';
    dst->type = src->is_pomodoro ? TIMER_TYPE_POMODORO : TIMER_TYPE_STANDARD;
    dst->session_sec = src->focus_ms / 1000;
    dst->short_break_sec = src->break_ms / 1000;
    dst->long_break_sec = src->is_pomodoro ? (src->break_ms * 3 / 1000) : (src->break_ms / 1000);
    dst->duration_sec = src->focus_ms / 1000;
    dst->is_default_pomodoro = false;
}

static void timer_to_preset(const timer_preset_t *src, preset_t *dst)
{
    dst->id = src->id;
    strncpy(dst->name, src->name, MAX_NAME_LEN - 1);
    dst->name[MAX_NAME_LEN - 1] = '\0';
    dst->is_pomodoro = (src->type == TIMER_TYPE_POMODORO);
    dst->focus_ms = src->session_sec * 1000;
    dst->break_ms = src->short_break_sec * 1000;
}

static void rebuild_cache(void)
{
    app_state_t *state = app_state_get();
    s_count = state->preset_count;
    if (s_count > TIMER_STORE_MAX_PRESETS) s_count = TIMER_STORE_MAX_PRESETS;
    for (int i = 0; i < s_count; i++) {
        preset_to_timer(&state->presets[i], &s_cache[i]);
    }
}

void timer_store_init(void)
{
    rebuild_cache();
    ESP_LOGI(TAG, "Timer store synced from app_state (%d presets)", s_count);
}

int timer_store_count(void)
{
    return s_count;
}

timer_preset_t *timer_store_get(int index)
{
    if (index < 0 || index >= s_count) return NULL;
    return &s_cache[index];
}

timer_preset_t *timer_store_get_by_id(int id)
{
    for (int i = 0; i < s_count; i++) {
        if (s_cache[i].id == id) return &s_cache[i];
    }
    return NULL;
}

int timer_store_add(const timer_preset_t *preset)
{
    app_state_t *state = app_state_get();
    if (state->preset_count >= MAX_PRESETS) return -1;
    preset_t *p = &state->presets[state->preset_count];
    timer_to_preset(preset, p);
    p->id = state->next_preset_id++;
    state->preset_count++;
    preset_to_timer(p, &s_cache[state->preset_count - 1]);
    s_count = state->preset_count;
    ESP_LOGI(TAG, "Added preset '%s' (id=%d), total=%d", p->name, p->id, s_count);
    return p->id;
}

void timer_store_update(int id, const timer_preset_t *preset)
{
    app_state_t *state = app_state_get();
    for (int i = 0; i < state->preset_count; i++) {
        if (state->presets[i].id == id) {
            int saved_id = state->presets[i].id;
            timer_to_preset(preset, &state->presets[i]);
            state->presets[i].id = saved_id;
            preset_to_timer(&state->presets[i], &s_cache[i]);
            ESP_LOGI(TAG, "Updated preset id=%d", id);
            return;
        }
    }
}

void timer_store_delete(int id)
{
    app_state_t *state = app_state_get();
    for (int i = 0; i < state->preset_count; i++) {
        if (state->presets[i].id == id) {
            for (int j = i; j < state->preset_count - 1; j++) {
                state->presets[j] = state->presets[j + 1];
                s_cache[j] = s_cache[j + 1];
            }
            state->preset_count--;
            s_count = state->preset_count;
            ESP_LOGI(TAG, "Deleted preset id=%d, remaining=%d", id, s_count);
            return;
        }
    }
}

int timer_store_next_id(void)
{
    return app_state_get()->next_preset_id;
}
