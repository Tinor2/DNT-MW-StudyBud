#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_DAILY_GOALS      3
#define MAX_GOAL_LEN         64
#define POINT_HISTORY_LEN    50

#define POINTS_TODO          10
#define POINTS_WATER         5
#define POINTS_WATER_GOAL    20
#define POINTS_WATER_BREAK   10
#define POINTS_BREATHING_BASE    10
#define POINTS_BREATHING_PER_CYCLE 1
#define POINTS_BREATHING_REPEAT  3
#define POINTS_FOCUS         25
#define POINTS_BEDTIME       30
#define POINTS_SLEEP_TRACKED 5
#define POINTS_SLEEP_3H      10
#define POINTS_SLEEP_6H      15
#define POINTS_SLEEP_7H      25
#define POINTS_SLEEP_8H      50
#define POINTS_GOAL          20
#define POINTS_ALL_GOALS     30
#define POINTS_MOVE          10
#define POINTS_MOVE_DAILY_CAP 5

#define SLEEP_TIER_3H_MIN    180
#define SLEEP_TIER_6H_MIN    360
#define SLEEP_TIER_7H_MIN    420
#define SLEEP_TIER_8H_MIN    480

#define BREATHING_COOLDOWN_SEC   1800
#define MIN_SLEEP_MINUTES    60

typedef enum {
    POINT_REASON_TODO = 1,
    POINT_REASON_WATER,
    POINT_REASON_WATER_GOAL,
    POINT_REASON_BREATHING,
    POINT_REASON_FOCUS,
    POINT_REASON_BEDTIME,
    POINT_REASON_DAILY_GOAL,
    POINT_REASON_ALL_GOALS,
    POINT_REASON_ADMIN,
    POINT_REASON_SLEEP = 10,
    POINT_REASON_WATER_BREAK = 11,
    POINT_REASON_MOVE = 12
} point_reason_t;

typedef enum {
    GOAL_METRIC_NONE = 0,
    GOAL_METRIC_WATER,
    GOAL_METRIC_FOCUS,
    GOAL_METRIC_BREATHING,
    GOAL_METRIC_TODOS,
    GOAL_METRIC_MOVE
} goal_metric_t;

typedef enum {
    STREAK_FOCUS = 0,
    STREAK_WATER,
    STREAK_BREATHING,
    STREAK_GOALS,
    STREAK_SLEEP,
    STREAK_MOVE,
    STREAK_COUNT
} streak_activity_t;

typedef struct {
    char label[MAX_GOAL_LEN];
    int  metric;
    int  target;
    bool done;
} daily_goal_t;

typedef struct {
    int  streak;
    char last_active[16];
} streak_t;

typedef struct {
    int      amount;
    int      reason;
    int      detail;
    char     day_key[16];
    int64_t  timestamp;
} point_event_t;

typedef struct {
    int total_points;
    int today_points;
    char day_key[16];

    daily_goal_t goals[MAX_DAILY_GOALS];

    streak_t streaks[STREAK_COUNT];

    int water_today;
    int focus_today;
    int breathing_today;
    int todos_done_today;
    int moves_today;

    bool water_bonus_claimed;
    bool bedtime_bonus_claimed;
    bool all_goals_bonus_claimed;

    int bedtime_hour;
    int bedtime_min;

    int64_t last_breathing_ts;

    point_event_t history[POINT_HISTORY_LEN];
    int history_count;
} points_state_t;

void points_store_init(void);
points_state_t *points_store_get_state(void);
void points_store_rollover_if_new_day(void);

int points_store_get_level(void);
int points_store_get_level_progress(void);
int points_store_get_level_threshold(void);

void points_store_get_bedtime(int *hour, int *min);
void points_store_set_bedtime(int hour, int min);

bool points_store_set_goal(int index, const char *label, int metric, int target);
int  points_store_toggle_goal(int index, bool done);

int points_store_award_todo(void);
int points_store_award_water(int water_goal);
int points_store_revoke_water(int count, int water_goal);
int points_store_award_water_break(void);
int points_store_award_move(void);
int points_store_award_breathing(int cycles);
int points_store_award_focus(void);
int points_store_award_sleep_tracked(int duration_min);
int points_store_award_bedtime(int start_hour_min);

int points_store_admin_add(int amount);
int points_store_admin_sub(int amount);
void points_store_admin_reset(void);

#ifdef __cplusplus
}
#endif
