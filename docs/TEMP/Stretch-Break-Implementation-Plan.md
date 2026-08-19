# Stretch Break — Implementation Plan

Scope: a new **Stretch Break** app (device + web) that reminds the user to move at a
configurable interval, lets them pick which exercise they did on the device, and rewards
**+10 seeds** (max 5 rewarded breaks/day) plus a 6th "Move" streak.

Reference mockup: `docs/TEMP/stretch_break_mockup.html` (4 device states + alert popup + web tab).

## Confirmed spec

- Name: **Stretch Break**. Manual countdown only — fully independent of the pomodoro timer.
- Default interval **60 min**, configurable on the web app only.
- Alert: full-screen popup on `lv_layer_top` + single 3-tone chime. Rotate + press picks
  **Snooze (10m)** or **Start Break**. Starting the break auto-opens the Move app in BREAK state.
- Break end: user selects which exercise they did (list of up to **4** free-form named
  exercises, edited on web). No break-duration timer.
- Seeds: **+10/break**, new reason `POINT_REASON_MOVE`. **Cap 5/day** (beyond cap: break is
  still logged, streak counts, no seeds awarded).
- 6th streak **"Move"**: +1 per day with ≥1 completed break. "Move" also becomes a
  daily-goal metric option (metric index 5).
- Screens (mirror `screen_sleep.c` state machine):
  - **IDLE** — breaks today `n/5`, Move streak, master toggle, big Start.
  - **RUNNING** — `mm:ss` + `lv_arc` ring (270°, 135°→405° like `screen_timer.c`), Pause.
  - **BREAK** — up to 4 exercise rows, pick = confirm.
  - **SUMMARY** — "+10 seeds" + n/5 + streak; press → auto-restart countdown.
- Countdown pauses during **quiet hours** (auto-resumes after) and during **active sleep
  sessions**; **master off** resets a running countdown; finished-while-off countdown fires
  the alert once on boot.
- Web: new "Stretch Break" tab — mirrored countdown only (device authoritative), exercise
  editor (add/rename/delete, max 4), interval input, quiet-hours times, master toggle;
  toasts via existing notification store when interval fires / break confirmed.
- Persistence: all settings + session state to `/sdcard/studybud.json`.

## Phase 1 — Store + points (device core)

New `main/display/utils/sedentary_store.h/.c`:

```
sedentary_state_t {
  bool   enabled;                 // master toggle
  int    interval_min;            // default 60
  int    quiet_start_min;         // minutes since midnight, default 23*60
  int    quiet_end_min;           // default 7*60
  int    exercise_count;          // 0..4
  char   exercises[4][MAX_NAME_LEN];

  // UI / session
  int    ui_state;                // IDLE / RUNNING / BREAK / SUMMARY
  bool   running;
  int    remaining_sec;           // live remainder
  int    total_sec;               // fixed at interval start
  int64_t end_epoch;              // absolute target (persist across reboot)
  bool   paused;                  // sleep/quiet pause (not manual)
  int64_t snooze_until;           // epoch when snoozed
  bool   in_quiet;
  bool   sleep_paused;

  // day tracking
  char   day_key[16];
  int    breaks_today;            // completed breaks (always counts)
  int    rewarded_today;          // capped at 5
  int    last_exercise_idx;
  bool   pending_alert;           // raise once on boot if finished while off
}
```

API: `sedentary_store_init`, `sedentary_store_tick(now)`, `start`, `pause`,
`snooze`, `confirm_break(idx)`, `acknowledge_summary`, `set_config`, `set_enabled`,
`exercise` getters/setters, `sedentary_store_is_sleep_active` bridge, persistence hooks
`sedentary_store_save_fields` / `sedentary_store_load`. Tick handles rollover (day_key via
points day key), quiet-hours pause/resume, sleep pause, snooze expiry → pending_alert.

`points_store.h/.c` changes:
- `#define POINTS_MOVE 10`, `#define POINTS_MOVE_DAILY_CAP 5`.
- `POINT_REASON_MOVE = 12` after `POINT_REASON_WATER_BREAK = 11`.
- `GOAL_METRIC_MOVE` after `GOAL_METRIC_TODOS` (= 5).
- `STREAK_MOVE` after `STREAK_SLEEP` (STREAK_COUNT becomes 6).
- `points_store_award_move(void)` → checks `moves_today` cap, returns seeds (0 if capped),
  bumps streak via existing streak logic, writes history with reason `"move"` detail = exercise idx.
- Add `int moves_today;` to `points_state_t`; reset in rollover.
- reason string "move" in `points_store.c` reason table (~line 167).

`tests/points_store_test.c`: add `test_move_award` — first 5 award 10 each, 6th awards 0,
streak increments once per day, rollover resets count.

## Phase 2 — Persistence

- `persistence.c`: bump version → 3. In save, emit `"sedentary":{...}` via
  `sedentary_store_save_fields`. In load, call `sedentary_store_load` (~line 266 settings block).

## Phase 3 — Device UI

- New `main/display/screens/screen_sedentary.c/.h`, one screen object with the 4 states
  (mirror `screen_sleep.c`): IDLE → RUNNING → BREAK → SUMMARY.
- `screen_timer.c` pattern: `lv_arc` (ARC_SIZE 300, start 135, end 405, animated) + `format_time`.
- BREAK: up to 4 rows; selecting a row calls `confirm_break(idx)` then transitions to SUMMARY.
- SUMMARY: press → `acknowledge_summary` → auto-restart (`start`) back to RUNNING.
- **Alert popup**: `lv_layer_top` overlay built in `ui_manager.c` — rotate + press two pills
  (Snooze / Start Break). Start Break → switch to SCREEN_SEDENTARY in BREAK state.
  Encoder interception in `ui_manager_encoder_event` before current screen handler.
- `ui_manager.c`: register screen in registry (~line 196), add 1s background tick calling
  `sedentary_store_tick` (guarded by time), refresh on switch.
- `screen_menu.c` `menu_items[]` (~line 42): add "Stretch Break" row with a symbol.
- **Chime**: new `main/display/utils/buzzer.c/.h`. TCA9554PWR EXIO toggle (`Set_EXIO`/
  `Set_Toggle`) or LEDC PWM — **pending board schematic check** (active vs passive buzzer,
  pin). Respect `settings.volume` (0 = silent); play 3-tone chime on alert.
- `sleep_store.c`: add `sleep_store_is_active(void)` for pause-during-sleep.

## Phase 4 — Networking (device)

- `app_state.c`: `screen_names[]` already contains `"sedentary"` (index 8). Add `"move"` to
  streak names (~line 189).
- WS handlers: `get_sedentary` (reply full state), `sedentary_update` (apply config from web).
- Broadcasts: `sedentary_sync` (state), `sedentary_alert` (interval fired), and reuse
  `points_earned` on confirm (type move).
- `app_state_send_full_sync`: append `sedentary` block.

## Phase 5 — Web app

- `App.svelte`: add tab `{ id:'move', label:'Stretch Break', icon:'🧘' }` (tabs ~line 769);
  WS message cases (`get_sedentary` reply, `sedentary_sync`, `sedentary_alert`) (~lines 539–602);
  send `get_sedentary` in sync timer (~line 180); toast mapping for reason MOVE.
- New `web-app/src/lib/components/MovePage.svelte` (mockup in `docs/TEMP/stretch_break_mockup.html`):
  mirrored countdown card, exercise editor (add/rename/delete ≤4), interval input,
  quiet-hours times, master toggle; sends `sedentary_update`.
- `TamagotchiPage.svelte` `STREAK_META` (~line 185): add `move` entry.
- Goals editor: add metric option "Move" (index 5).

## Phase 6 — Build & test

- Add new sources to `main/CMakeLists.txt` and `simulator/CMakeLists.txt`.
- Build ESP-IDF target; build + run simulator (`cmake --build`); run
  `tests/points_store_test.c` (standalone).

## Open items

- Buzzer pin + active/passive type — needs `ESP32-S3-LCD-2.8C` schematic.
- `sleep_store_is_active()` must be added.
- Water-break bonus (`app_state.c:405`) left unchanged.
