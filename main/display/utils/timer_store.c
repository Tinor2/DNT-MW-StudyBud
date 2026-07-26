#include "timer_store.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "Timer_Store";

static timer_preset_t presets[TIMER_STORE_MAX_PRESETS];
static int preset_count = 0;
static int next_id = 1;

void timer_store_init(void)
{
    preset_count = 0;
    next_id = 1;
    memset(presets, 0, sizeof(presets));

    timer_preset_t default_pomo;
    memset(&default_pomo, 0, sizeof(default_pomo));
    default_pomo.id = next_id++;
    strncpy(default_pomo.name, "Pomodoro", TIMER_STORE_NAME_LEN - 1);
    default_pomo.type = TIMER_TYPE_POMODORO;
    default_pomo.session_sec = 25 * 60;
    default_pomo.short_break_sec = 5 * 60;
    default_pomo.long_break_sec = 15 * 60;
    default_pomo.is_default_pomodoro = true;
    presets[preset_count++] = default_pomo;

    ESP_LOGI(TAG, "Timer store initialized with %d default preset(s)", preset_count);
}

int timer_store_count(void)
{
    return preset_count;
}

timer_preset_t *timer_store_get(int index)
{
    if (index < 0 || index >= preset_count) return NULL;
    return &presets[index];
}

timer_preset_t *timer_store_get_by_id(int id)
{
    for (int i = 0; i < preset_count; i++) {
        if (presets[i].id == id) return &presets[i];
    }
    return NULL;
}

int timer_store_add(const timer_preset_t *preset)
{
    if (preset_count >= TIMER_STORE_MAX_PRESETS) return -1;
    timer_preset_t copy = *preset;
    copy.id = next_id++;
    presets[preset_count++] = copy;
    ESP_LOGI(TAG, "Added preset '%s' (id=%d), total=%d", copy.name, copy.id, preset_count);
    return copy.id;
}

void timer_store_update(int id, const timer_preset_t *preset)
{
    for (int i = 0; i < preset_count; i++) {
        if (presets[i].id == id) {
            int saved_id = presets[i].id;
            presets[i] = *preset;
            presets[i].id = saved_id;
            ESP_LOGI(TAG, "Updated preset id=%d", id);
            return;
        }
    }
}

void timer_store_delete(int id)
{
    for (int i = 0; i < preset_count; i++) {
        if (presets[i].id == id) {
            for (int j = i; j < preset_count - 1; j++) {
                presets[j] = presets[j + 1];
            }
            preset_count--;
            ESP_LOGI(TAG, "Deleted preset id=%d, remaining=%d", id, preset_count);
            return;
        }
    }
}

int timer_store_next_id(void)
{
    return next_id;
}
