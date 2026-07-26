#include "app_state.h"
#include <string.h>

static app_state_t state;

void app_state_init(void)
{
    memset(&state, 0, sizeof(state));
    state.timer.remaining_seconds = 0;
    state.timer.total_seconds = 0;
    state.timer.phase = TIMER_PHASE_SESSION;
    state.timer.preset_id = -1;
    state.timer.is_running = false;
    state.timer.phase_complete_awaiting_press = false;
}

app_state_t *app_state_get(void)
{
    return &state;
}

timer_state_t *app_state_get_timer(void)
{
    return &state.timer;
}
