#pragma once

#include <stdint.h>
#include <stdbool.h>

#define TIMER_STORE_MAX_PRESETS 10
#define TIMER_STORE_NAME_LEN   24

typedef enum {
    TIMER_TYPE_STANDARD,
    TIMER_TYPE_POMODORO
} timer_type_t;

typedef struct {
    int           id;
    char          name[TIMER_STORE_NAME_LEN];
    timer_type_t  type;
    uint32_t      duration_sec;
    uint32_t      session_sec;
    uint32_t      short_break_sec;
    uint32_t      long_break_sec;
    bool          is_default_pomodoro;
} timer_preset_t;

void             timer_store_init(void);
int              timer_store_count(void);
timer_preset_t  *timer_store_get(int index);
timer_preset_t  *timer_store_get_by_id(int id);
int              timer_store_add(const timer_preset_t *preset);
void             timer_store_update(int id, const timer_preset_t *preset);
void             timer_store_delete(int id);
int              timer_store_next_id(void);
