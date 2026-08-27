#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_TODOS        32
#define MAX_PRESETS      16
#define MAX_EXERCISES    8
#define MAX_NAME_LEN     64
#define MAX_TODO_LEN     128
#define MAX_BROADCAST    32768
#define MAX_SCREENS      16

typedef struct {
    int id;
    char text[MAX_TODO_LEN];
    bool done;
    int priority;
    int order;
    bool points_awarded;
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

typedef enum {
    TIMER_PHASE_SESSION,
    TIMER_PHASE_SHORT_BREAK,
    TIMER_PHASE_LONG_BREAK
} timer_phase_t;

typedef struct {
    bool running;
    int remaining_ms;
    int phase;
    int preset_id;
    int64_t last_tick;
    uint32_t remaining_seconds;
    uint32_t total_seconds;
    bool is_running;
    bool phase_complete_awaiting_press;
    int64_t end_tick;
    uint8_t pomodoro_session_count;
} timer_state_t;

typedef struct {
    int brightness;
    int volume;
    int idle_timeout;
    int reading_light;
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

    uint32_t screen_accent[MAX_SCREENS];
    uint32_t screen_accent_light[MAX_SCREENS];
    uint32_t screen_accent_dark[MAX_SCREENS];
} app_state_t;

typedef void (*ws_broadcast_fn)(const char *msg);

void app_state_init(ws_broadcast_fn broadcaster);
void app_state_set_broadcaster(ws_broadcast_fn broadcaster);
app_state_t *app_state_get(void);
timer_state_t *app_state_get_timer(void);

void app_state_handle_message(const char *type, const char *json_msg, char *resp, size_t resp_len);
void app_state_send_full_sync(char *resp, size_t resp_len);

void app_state_broadcast_screen_change(int screen_id);
void app_state_broadcast_encoder_event(const char *direction, const char *press_state);
void app_state_broadcast_todo_toggled(int index, int id, const char *text, bool done);
void app_state_broadcast_todo_sync(void);
void app_state_broadcast_sleep_state(const char *state);
void app_state_broadcast_sleep_session(int duration_min);
void app_state_broadcast_breathing_sync(void);
void app_state_broadcast_breathing_complete(int cycles);
void app_state_broadcast_timer_sync(void);
void app_state_broadcast_timer_session_complete(int phase, int preset_id);
void app_state_broadcast_water_sync(void);
void app_state_broadcast_points_earned(int amount, int reason, int detail);
void app_state_broadcast_points_sync(void);
void app_state_broadcast_settings_sync(void);

#ifdef __cplusplus
}
#endif
