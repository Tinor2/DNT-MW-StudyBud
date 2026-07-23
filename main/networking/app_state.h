#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TODOS        32
#define MAX_PRESETS      16
#define MAX_EXERCISES    8
#define MAX_NAME_LEN     64
#define MAX_TODO_LEN     128
#define MAX_BROADCAST    1024

typedef struct {
    int id;
    char text[MAX_TODO_LEN];
    bool done;
    int priority;
    int order;
} todo_item_t;

typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    int focus_ms;
    int break_ms;
    bool is_pomodoro;
} preset_t;

typedef struct {
    int id;
    char name[MAX_NAME_LEN];
    int inhale_ms;
    int hold_ms;
    int exhale_ms;
    int hold2_ms;
} exercise_t;

typedef struct {
    int glasses;
    int goal;
} water_t;

typedef struct {
    char action[16];
} timer_cmd_t;

typedef struct {
    bool running;
    int remaining_ms;
    int phase;
    int preset_id;
    int64_t last_tick;
} timer_state_t;

typedef struct {
    int brightness;
    int volume;
    int idle_timeout;
} settings_t;

typedef struct {
    todo_item_t todos[MAX_TODOS];
    int todo_count;
    int next_todo_id;

    preset_t presets[MAX_PRESETS];
    int preset_count;
    int next_preset_id;
    int active_preset_id;

    timer_state_t timer;

    water_t water;

    exercise_t exercises[MAX_EXERCISES];
    int exercise_count;
    bool breathing_active;
    int breathing_exercise_id;

    settings_t settings;

    int current_screen;
} app_state_t;

typedef void (*ws_broadcast_fn)(const char *msg);

void app_state_init(ws_broadcast_fn broadcaster);
app_state_t *app_state_get(void);

void app_state_handle_message(const char *type, const char *json_msg, char *resp, size_t resp_len);
void app_state_send_full_sync(char *resp, size_t resp_len);

void app_state_broadcast_screen_change(int screen_id);
void app_state_broadcast_encoder_event(const char *direction, const char *press_state);
void app_state_broadcast_todo_toggled(int index, const char *text, bool done);

#ifdef __cplusplus
}
#endif
