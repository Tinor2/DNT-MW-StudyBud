# Tamagotchi — Points / Goals System (Backend Roadmap)

## Overview

The Tamagotchi app will have 2 components: (1) a growing plant and (2) a
goal/achievement section. This roadmap covers the **backend** of the
goal/achievement section — the point economy, streaks, daily goals, level
progression and WebSocket protocol — implemented first, before any LVGL or
Svelte front-end work.

Source of truth is the **ESP32** (state persisted to SD card), matching the
existing single-writer pattern in `app_state.c`.

## Locked design decisions

| Decision | Choice |
|---|---|
| Source of truth | ESP32 state + SD persistence |
| Points model | Daily total + lifetime total |
| Level curve | **Placeholder** for now — final curve deferred to plant work, isolated in one function |
| Daily goals | 3 qualitative goals/day, user-labelled, hybrid completion (manual tick OR linked to a metric), reset at midnight |
| Streaks | Per-activity: focus, water, breathing, daily goals, sleep |
| Streak bonus | Multiplier on that activity's points: `1.0 + 0.1×(streak−1)`, capped at `2.0` |
| Bedtime reward | Sleep started before a configurable time (default 23:30) |
| History feed | Last 50 events, persisted, newest-first |
| Achievements | None as a separate system — "achievements" = the daily goal cards + streaks + level |
| Admin controls | Yes (add/subtract/reset points from web app) |
| Time source | SNTP (device knows local time via `localtime_r`) |

## Point economy

| Event | Base points | Streak-multiplied by |
|---|---|---|
| Todo completed | +10 | — (todos are not a streak activity) |
| Water glass | +5 | water |
| Water glass removed (same day) | −5 | — (refunds the award) |
| Water goal met (once/day) | +20 | water |
| Water glass during an active session break | +10 extra | water |
| Breathing session | 10 + 1/cycle (scales with session size) | breathing |
| Breathing repeat (within 30 min) | +3 flat | — (anti-farming) |
| Focus / Pomodoro session | +25 | focus |
| Good bedtime (before threshold) | +30 | sleep |
| Any sleep tracked (awake pressed, duration > 0) | +5 | sleep |
| Sleep ≥ 3h / 6h / 7h / 8h | +10 / +15 / +25 / +50 | sleep |
| Daily goal completed | +20 | goals |
| All 3 daily goals completed | +30 bonus | goals |

- A todo awards **once per todo, ever** (`points_awarded` flag on the todo,
  persisted) to prevent farming via re-completing.
- Removing a water glass reverses the +5/glass award (clamped at 0, `water_today`
  decreases too). If the count drops back below the goal, the once-per-day +20
  water-goal bonus is also clawed back (`water_bonus_claimed` resets so it can be
  re-earned).
- Drinking a glass while the pomodoro timer is in an active short/long break
  (running, not awaiting press) awards an extra +10 per glass.
- Bedtime bonus requires a minimum sleep session (60 min) so it can't be
  farmed by starting/ending an instant session; the tiered "sleep tracked" award
  still applies for any logged session (duration > 0), rewarding consistency.
- Breathing cycles are clamped to `[1, 60]`; sessions repeated within
  `BREATHING_COOLDOWN_SEC = 1800` (30 min) award a flat `+3` and do not bump the
  breathing streak.
- Multiplier computed in integer basis points: `mult = 100 + 10*(streak-1)`,
  clamped to `[100, 200]`; `points = base * mult / 100`.

## Streaks

Consecutive days of activity per category. A streak increments when the
activity happens on a new day (previous day active → `streak+1`, else reset to
1). At day rollover, any streak whose last active day is older than yesterday
drops to 0. Multiplier is derived from the streak length as above.

## Daily goals

- 3 slots (`MAX_DAILY_GOALS = 3`), each `{label[64], metric, target, done}`.
- `metric` = NONE (manual tick-off) | WATER | FOCUS | BREATHING | TODOS.
- For a linked goal, `target` is the threshold (e.g. water ≥ 8 auto-completes).
- Goals reset at midnight (labels persist, `done` clears).
- Completing a goal awards points; completing all 3 awards the bonus.

## State model — `points_store` module

New pure-C module `main/display/utils/points_store.{c,h}` (no LVGL/WS deps),
built in both firmware and simulator, mirroring `sleep_store`/`session_store`.

```c
typedef struct {
    char label[MAX_GOAL_LEN];
    int  metric;          // goal_metric_t
    int  target;
    bool done;
} daily_goal_t;

typedef struct {
    int  streak;          // consecutive days
    char last_active[16]; // "YYYY-MM-DD"
} streak_t;

typedef struct {
    int   amount;
    int   reason;         // point_reason_t
    int   detail;
    char  day_key[16];
    int64_t timestamp;
} point_event_t;

typedef struct {
    int total_points;
    int today_points;
    char day_key[16];

    daily_goal_t goals[MAX_DAILY_GOALS];

    streak_t streaks[STREAK_COUNT];      // focus, water, breathing, goals, sleep

    int water_today;
    int focus_today;
    int breathing_today;
    int todos_done_today;

    bool water_bonus_claimed;
    bool bedtime_bonus_claimed;
    bool all_goals_bonus_claimed;

    int bedtime_hour;                    // default 23
    int bedtime_min;                     // default 30

    point_event_t history[POINT_HISTORY_LEN];  // newest-first ring (shifted)
    int history_count;
} points_state_t;
```

Level is derived (not stored): `points_store_get_level()`,
`get_level_progress()`, `get_level_threshold()` use a placeholder linear curve
(`LEVEL_BASE_XP = 100`, `LEVEL_XP_INCREMENT = 150`) — one function to change
when the plant component is built.

## WebSocket protocol additions

Web → ESP32:
- `get_points` → responds `points_sync`
- `goal_set {index, label?, metric?, target?}`
- `goal_toggle {index, done}`
- `points_setting {hour?, min?}` (bedtime threshold)
- `points_admin {action: "add"|"sub"|"reset", amount?, reason?}`

ESP32 → Web (broadcast on change, also nested under `"points"` in `full_sync`):
- `points_sync` — totals, day, level+progress, goals, streaks+multipliers,
  water/bedtime claimed flags, bedtime setting, last 50 history events
- `points_earned {amount, reason, detail, total, today}` — fired per user
  action (the feed's granular breakdown comes from the `points_sync` history)

Reason codes: TODO=1, WATER=2, WATER_GOAL=3, BREATHING=4, FOCUS=5, BEDTIME=6,
DAILY_GOAL=7, ALL_GOALS=8, ADMIN=9, SLEEP=10, WATER_BREAK=11.

## Event hooks (firmware)

Device and web events funnel through `networking/app_state.c`:

| Event | Hook point |
|---|---|
| Todo completed (web) | `handle_todo_update` (done transition false→true) |
| Todo completed (device) | `app_state_broadcast_todo_toggled` |
| Water add (web + device) | `app_state_broadcast_water_sync` (glasses delta detection, add-only) |
| Breathing session | `app_state_broadcast_breathing_complete` |
| Focus session | `app_state_broadcast_timer_session_complete` (phase = session) |
| Sleep session | `app_state_broadcast_sleep_session` (+ `sleep_store` last-start getter) |

## Files

- `main/display/utils/points_store.h` / `.c` — new core module
- `main/display/utils/sleep_store.h` / `.c` — add last-start hour/min getter
- `main/display/app_state.h` — `todo_item_t.points_awarded`, `MAX_BROADCAST`
  4096 → 8192, new broadcast prototypes
- `main/networking/app_state.c` — hooks, WS handlers, `points_sync`/`points_earned`
  broadcasts, `full_sync` points block
- `main/display/utils/persistence.c` — schema v2, save/load points + todo flag,
  buffer 16KB → 32KB
- `main/main.cpp` — `points_store_init()`
- `simulator/CMakeLists.txt` — add `points_store.c`
- `tests/points_store_test.c` (host, gcc) — unit test for rollover/streaks/bonuses

## Deferred (not backend milestone)

- LVGL `screen_tamagotchi.c` (plant + goals on-device)
- Plant component & growth-stage → level mapping
- Final level curve
- Svelte "Tamagotchi" tab UI (points total, level bar, daily-goal cards grid,
  streak chips, history feed, admin panel)
