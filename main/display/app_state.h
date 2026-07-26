#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    TIMER_PHASE_SESSION,
    TIMER_PHASE_SHORT_BREAK,
    TIMER_PHASE_LONG_BREAK
} timer_phase_t;

typedef struct {
    uint32_t     remaining_seconds;
    uint32_t     total_seconds;
    timer_phase_t phase;
    int          preset_id;
    bool         is_running;
    bool         phase_complete_awaiting_press;
} timer_state_t;

typedef struct {
    timer_state_t timer;
} app_state_t;

app_state_t  *app_state_get(void);
timer_state_t *app_state_get_timer(void);
void app_state_init(void);
