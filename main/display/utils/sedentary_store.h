#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SEDENTARY_MAX_EXERCISES     4
#define SEDENTARY_EXERCISE_LEN      64
#define SEDENTARY_DEFAULT_INTERVAL_MIN 60
#define SEDENTARY_SNOOZE_MIN        10
#define SEDENTARY_DAILY_CAP         5

typedef enum {
    SEDENTARY_UI_IDLE = 0,
    SEDENTARY_UI_RUNNING,
    SEDENTARY_UI_BREAK,
    SEDENTARY_UI_SUMMARY
} sedentary_ui_state_t;

typedef struct {
    bool enabled;
    int  interval_min;
    int  quiet_start_min;
    int  quiet_end_min;
    int  exercise_count;
    char exercises[SEDENTARY_MAX_EXERCISES][SEDENTARY_EXERCISE_LEN];

    int  ui_state;
    bool running;
    bool manually_paused;
    int  remaining_sec;
    int  total_sec;
    int64_t end_epoch;
    int64_t snooze_until;
    bool snoozing;
    bool in_quiet;
    bool sleep_paused;

    char day_key[16];
    int  breaks_today;
    int  rewarded_today;
    int  last_exercise_idx;
    bool pending_alert;
} sedentary_state_t;

void sedentary_store_init(void);
sedentary_state_t *sedentary_store_get_state(void);

void sedentary_store_set_enabled(bool enabled);
void sedentary_store_set_interval(int minutes);
void sedentary_store_set_quiet_hours(int start_min, int end_min);
void sedentary_store_set_exercises(const char *names[SEDENTARY_MAX_EXERCISES], int count);
bool sedentary_store_rename_exercise(int index, const char *name);
bool sedentary_store_add_exercise(const char *name);
bool sedentary_store_remove_exercise(int index);

void sedentary_store_start(void);
void sedentary_store_toggle_pause(void);
void sedentary_store_snooze(void);
void sedentary_store_enter_break(void);
int  sedentary_store_confirm_break(int exercise_idx);
void sedentary_store_acknowledge_summary(void);

void sedentary_store_tick(void);
bool sedentary_store_consume_alert(void);

int  sedentary_store_get_remaining_sec(void);
int  sedentary_store_get_total_sec(void);
bool sedentary_store_should_refresh(void);

#ifdef __cplusplus
}
#endif
