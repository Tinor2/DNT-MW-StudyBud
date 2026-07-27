#include "app_state.h"
#include <string.h>

static app_state_t s_state;

void app_state_init(ws_broadcast_fn broadcaster)
{
    (void)broadcaster;
    memset(&s_state, 0, sizeof(s_state));
    s_state.todo_count = 0;
    s_state.next_todo_id = 1;
    s_state.preset_count = 0;
    s_state.next_preset_id = 1;
    s_state.active_preset_id = -1;
    s_state.timer.running = false;
    s_state.timer.remaining_ms = 0;
    s_state.timer.phase = TIMER_PHASE_SESSION;
    s_state.timer.preset_id = -1;
    s_state.timer.last_tick = 0;
}

app_state_t *app_state_get(void)
{
    return &s_state;
}

timer_state_t *app_state_get_timer(void)
{
    return &s_state.timer;
}

void app_state_handle_message(const char *type, const char *json_msg, char *resp, size_t resp_len)
{
    (void)type; (void)json_msg; (void)resp; (void)resp_len;
}

void app_state_send_full_sync(char *resp, size_t resp_len)
{
    (void)resp; (void)resp_len;
}

void app_state_broadcast_screen_change(int screen_id)
{
    (void)screen_id;
}

void app_state_broadcast_encoder_event(const char *direction, const char *press_state)
{
    (void)direction; (void)press_state;
}

void app_state_broadcast_todo_toggled(int index, const char *text, bool done)
{
    (void)index; (void)text; (void)done;
}
