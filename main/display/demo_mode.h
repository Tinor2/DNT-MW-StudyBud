#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ui_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DEMO_TASK_NONE,
    DEMO_TASK_WAIT_FOR_ACTION,
    DEMO_TASK_AUTO_ADVANCE,
} demo_task_type_t;

bool demo_mode_can_start(char *reason, size_t reason_len);
bool demo_mode_start(void);
void demo_mode_stop(bool restore_state);
bool demo_mode_is_active(void);
bool demo_mode_is_task_active(void);
void demo_mode_advance(void);
void demo_mode_check_task_complete(void);
uint8_t demo_mode_current_step(void);
uint8_t demo_mode_total_steps(void);
screen_id_t demo_mode_current_screen(void);

#ifdef __cplusplus
}
#endif
