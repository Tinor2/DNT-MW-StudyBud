# Demo Mode Implementation Roadmap

## 1. Purpose

Implement a **Demo** menu item in the StudyBud LVGL application that prepares a repeatable, presentation-ready set of data and guides the presenter through the strongest device screens.

The mode is intended for a live classroom/marking demonstration on the real ESP32 and rotary encoder. It should make the product easy to explain without waiting for habits, sleep history, timers or streaks to accumulate naturally.

### Core behaviour

- Runs on the real 480 × 480 ESP32 display.
- Starts from a dedicated Demo menu item.
- Uses a short press to advance and a long press to exit.
- Ignores rotation during the guided sequence so the presenter cannot accidentally change seeded values.
- Shows a persistent **DEMO · step X/Y** overlay above each existing screen.
- Seeds representative but clearly artificial showcase data.
- Restores the exact pre-demo state when the presenter exits.
- Never writes demo data to the SD-card state file.
- Can be exited safely at any step.

### Non-goals

- Demo mode is not a replacement for normal feature testing.
- It does not need to automate the web companion in its first version.
- It should not simulate features that the final product does not perform.
- It should not require a new copy of every existing screen.

**Status:** Architecture reviewed against the current repository; implementation not started.

---

## 2. Repository Findings

The original concept was sound, but the repository review identified several details that affect the implementation.

### 2.1 The data is split across several stores

Backing up only `app_state_t` is not sufficient.

| Data area | Current source | Demo implication |
|---|---|---|
| Todos, exercises, water, settings and timer runtime | `main/display/app_state.h:81` `app_state_t` | May be copied as part of an `app_state_t` snapshot |
| Timer preset display cache | `main/display/utils/timer_store.c` | Must be rebuilt after seeding and restoration via `timer_store_init()` (line 50) |
| Seeds, goals, streaks and point history | `points_store.h:95` `points_state_t` | Requires a separate snapshot via `points_store_get_state()` (line 124) |
| Sleep history | `sleep_store.c` | Must use `sleep_store_set_history_entries()` (line 153) |
| Breathing-session count | `session_store.c` | Must use `session_store_set_breath_count()` (line 20) |
| Stretch-break state | `sedentary_store.h:23` `sedentary_state_t` | Has its own state and background tick; query `pending_alert` field (line 46) |
| Persistent save state | `persistence.c` | Delayed saving must be suspended throughout the demo |

### 2.2 Existing screens should be reused

`ui_manager.c` already creates and refreshes the screens and routes rotary input through one central `ui_manager_encoder_event()` function (line 505). Demo mode should:

1. seed data;
2. switch between the existing screen IDs;
3. intercept encoder input centrally; and
4. draw its progress indicator on `lv_layer_top()` (follows the pattern used at lines 128, 149, 193, 254).

This avoids maintaining duplicate "demo" versions of all screens.

### 2.3 Persistence is the highest-risk area

`persistence_mark_dirty()` (line 663) sets `s_dirty = true`. The `save_timer_cb()` (line 650) fires every 5 seconds and writes to SD when dirty. If demo values are allowed to reach that save, a restart could load the artificial data.

The safe sequence should be:

1. finish any pending normal save (`persistence_save()`, line 482);
2. capture the runtime snapshot;
3. suspend persistence (new API);
4. seed the demo state;
5. run the walkthrough;
6. restore the snapshot;
7. refresh/broadcast the restored state; and
8. resume persistence.

### 2.4 Preconditions reduce unnecessary risk

Version 1 should refuse to start if:

- a sleep log is active (`sleep_store_is_active()`, sleep_store.c line 33);
- a timer is actively running (`app_state_get()->timer.is_running`); or
- a stretch-break alert is currently being handled (`sedentary_store_get_state()->pending_alert`).

The landing screen should explain which activity must be ended first. This is safer than trying to snapshot live countdown timestamps and resume them perfectly after the demonstration.

### 2.5 Screen numbering should not be hard-coded

`SCREEN_TAMAGOTCHI` is currently the last real screen (enum value 13), followed by `SCREEN_COUNT` (value 14) at `ui_manager.h:23-24`. Add `SCREEN_DEMO` immediately before `SCREEN_COUNT`, but do not assign a literal value. Allow the enum to determine its value so future screens do not create collisions.

### 2.6 Menu capacity is sufficient

The menu item array in `screen_menu.c:43-54` currently has 10 items. Parallel widget arrays are hard-coded to 16 slots (lines 57-60). Adding the 11th Demo item is safe; adding more beyond that would overflow.

### 2.7 Dual `app_state.c` implementations exist

There are two `app_state.c` files with `app_state_get()`:
- `main/display/app_state.c:29` — display stub (used by simulator)
- `main/networking/app_state.c:115` — real implementation (used on hardware)

Demo mode should work with both. The snapshot operates on the pointer returned by `app_state_get()` regardless of which implementation is linked.

---

## 3. Recommended Demonstration Sequence

The first version should use **nine guided steps**. Menu navigation itself is the launch mechanism and does not need to be counted as a separate step.

| Step | Screen | What the presenter should point out | Seed/state required |
|---:|---|---|---|
| 1 | Home (`SCREEN_HOME`) | Clock, level/seeds and daily summaries | Non-zero points and daily counts |
| 2 | Tamagotchi (`SCREEN_TAMAGOTCHI`) | Developed plant, seed total and level progression | Points total high enough for a later plant stage (≥1200 for level 5 / plant_3) |
| 3 | Breathing (`SCREEN_BREATHING`) | Exercise selection and guided-wellbeing purpose | Two exercises with recognisable timing patterns |
| 4 | Water (`SCREEN_WATER`) | Near-complete daily target and direct dial input model | 6 of 8 glasses |
| 5 | Stretch Break (`SCREEN_SEDENTARY`) | Optional reminder and movement-break structure | Enabled, sensible interval, no active popup |
| 6 | Sleep (`SCREEN_SLEEP`) | Manually recorded sleep and seven-night comparison | Seven varied sleep records with start times |
| 7 | Timer Presets (`SCREEN_TIMER_PRESETS`) | Pomodoro and alternative focus periods | Three valid presets |
| 8 | Todos (`SCREEN_TODOS`) | Priorities, completed/pending states and progress | Four varied tasks, one complete |
| 9 | Settings (`SCREEN_SETTINGS`) | Brightness, idle timeout and reading-light controls | Safe showcase values |

At the final step, a short press should open a completion card with **Restart demo** and **Exit demo** choices, or simply exit if the minimal implementation is preferred. A long press must always restore and exit immediately.

---

## 4. Proposed Architecture

### 4.1 New files

Create:

- `main/display/demo_mode.h`
- `main/display/demo_mode.c`
- `main/display/screens/screen_demo.h`
- `main/display/screens/screen_demo.c`

`screen_demo.c` is only the launch/status screen (shows landing, precondition failures, or completion card). The walkthrough itself reuses the normal screens.

### 4.2 Public API

`demo_mode.h` should expose a small interface:

```c
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "ui_manager.h"

bool demo_mode_can_start(char *reason, size_t reason_len);
bool demo_mode_start(void);
void demo_mode_stop(bool restore_state);
bool demo_mode_is_active(void);
void demo_mode_advance(void);
uint8_t demo_mode_current_step(void);
uint8_t demo_mode_total_steps(void);
screen_id_t demo_mode_current_screen(void);
```

`demo_mode_start()` should return `false` if a precondition fails or a snapshot cannot be created.

### 4.3 Snapshot structure

The snapshot must cover all state modified by the seed operation:

```c
typedef struct {
    bool valid;
    app_state_t app;                          // main display state
    points_state_t points;                    // gamification store
    uint16_t sleep_minutes[7];                // sleep durations
    int16_t sleep_start_minutes[7];           // sleep bedtimes (-1 = unset)
    int sleep_count;                          // number of sleep entries
    int breathing_session_count;              // session_store count
    sedentary_state_t sedentary;              // stretch-break state
    screen_id_t previous_screen;              // screen before demo started
} demo_backup_t;
```

Important rules:

- Do not snapshot LVGL object pointers (they are recreated on screen switch).
- Do not restore by replacing screen objects; use `ui_manager_switch_screen()`.
- Do not copy an active sleep session in version 1; block demo start while `sleep_store_is_active()`.
- Re-run `timer_store_init()` after copying preset data into or out of `app_state_t`.
- Refresh the currently displayed screen after restoration.
- Memory cost: approximately `sizeof(app_state_t) + sizeof(points_state_t) + sizeof(sedentary_state_t) + ~30 bytes` ≈ **4-5 KB** stack or heap.

### 4.4 Demo state machine

```text
INACTIVE
   ↓ select Demo menu item
LANDING SCREEN (precondition check)
   ↓ precondition fails → show reason → short press → MENU
   ↓ precondition passes → short press
BACKUP → FORCE SAVE → SUSPEND PERSISTENCE → SEED DATA → STEP 1
   ↓ short press
STEP 2 … STEP 9
   ↓ short press at step 9
COMPLETION CARD (Restart / Exit)
   ↓ short press (Exit) or long press (any step)
RESTORE → REFRESH SCREENS → BROADCAST → RESUME PERSISTENCE → MENU
```

The ordered step list should be a constant array of `screen_id_t`:

```c
static const screen_id_t demo_steps[] = {
    SCREEN_HOME,
    SCREEN_TAMAGOTCHI,
    SCREEN_BREATHING,
    SCREEN_WATER,
    SCREEN_SEDENTARY,
    SCREEN_SLEEP,
    SCREEN_TIMER_PRESETS,
    SCREEN_TODOS,
    SCREEN_SETTINGS,
};
#define DEMO_STEP_COUNT (sizeof(demo_steps) / sizeof(demo_steps[0]))
```

### 4.5 Progress overlay

Create the indicator on `lv_layer_top()` so it remains visible across screen changes. This follows the existing pattern used for `glow_overlay` (line 128), `nav_bubble` (line 149), `reading_overlay` (line 193), and `alert_overlay` (line 254) in `ui_manager.c`.

```text
DEMO  3/9   ·   press: next   ·   hold: exit
```

Requirements:

- Compact enough not to hide important circular-display content.
- High contrast against every screen theme (use a dark semi-transparent pill with white text).
- Recreated or brought to the foreground after screen transitions.
- Deleted before normal interaction resumes.
- No automatic advancement.

### 4.6 Persistence suspension mechanism

The new `persistence_set_suspended()` must gate two things:

1. `persistence_mark_dirty()` — do not set `s_dirty` while suspended.
2. `save_timer_cb()` — early return if suspended.

Implementation in `persistence.c`:

```c
static volatile bool s_suspended = false;

void persistence_set_suspended(bool suspended) {
    s_suspended = suspended;
}

bool persistence_is_suspended(void) {
    return s_suspended;
}

// Modified persistence_mark_dirty():
void persistence_mark_dirty(void) {
    if (!s_suspended) s_dirty = true;
}

// Modified save_timer_cb():
static void save_timer_cb(void *arg) {
    if (s_suspended) return;       // <-- add this guard
    if (s_dirty) {
        if (persistence_save()) {
            s_dirty = false;
            ESP_LOGI(TAG, "State saved");
        } else {
            ESP_LOGW(TAG, "Save failed, will retry");
        }
    }
}
```

---

## 5. Implementation Phases

### Phase 0 — Safety preparation

- [ ] Add `static volatile bool s_suspended = false;` to `persistence.c`.
- [ ] Add `persistence_set_suspended(bool suspended)` to `persistence.h` and `persistence.c`.
- [ ] Add `persistence_is_suspended(void)` to `persistence.h` and `persistence.c`.
- [ ] Guard `persistence_mark_dirty()` with `if (!s_suspended)`.
- [ ] Guard `save_timer_cb()` with `if (s_suspended) return;`.
- [ ] Call `persistence_save()` before taking the snapshot to flush any pending dirty state.
- [ ] Add precondition check helper in `demo_mode.c`:
  ```c
  bool demo_mode_can_start(char *reason, size_t len) {
      if (sleep_store_is_active()) { snprintf(reason, len, "Stop sleep tracking"); return false; }
      if (app_state_get()->timer.is_running) { snprintf(reason, len, "Stop the timer"); return false; }
      if (sedentary_store_get_state()->pending_alert) { snprintf(reason, len, "Dismiss stretch break"); return false; }
      return true;
  }
  ```

**Exit criterion:** Suspend persistence, call `persistence_mark_dirty()`, wait >5 seconds, confirm the SD-card file was not changed. Then resume and confirm normal saves work again.

### Phase 1 — Core demo controller

- [ ] Create `demo_mode.h` with the public API (section 4.2).
- [ ] Create `demo_mode.c` with:
  - Static `demo_backup_t s_backup` and `bool s_active`, `uint8_t s_step`.
  - The `demo_steps[]` constant array (section 4.4).
  - `demo_mode_can_start()` with precondition checks.
  - `demo_mode_start()` implementing the safe sequence: force save → suspend → backup → seed → switch to step 1.
  - `demo_mode_stop(bool restore)` implementing: restore → refresh → broadcast → resume → switch to menu.
  - `demo_mode_advance()` incrementing step and calling `ui_manager_switch_screen()`.
  - `demo_mode_current_screen()` returning `demo_steps[s_step]`.
  - Idempotency guards: reject `start()` if already active, make `stop()` safe to call multiple times.

**Exit criterion:** A simulator test can call `demo_mode_start()`, verify backup was taken, and call `demo_mode_stop(true)` to restore without crashes.

### Phase 2 — Seed representative data

#### App state (via `app_state_get()`)

- [ ] Water: set `water.glasses = 6`, `water.goal = 8`.
- [ ] Todos: four items with `todo_store_add()` or direct struct writes; set `todo_count = 4`; one marked done, three pending with varied priorities (high/medium/low).
- [ ] Breathing exercises: two entries in `exercises[]` — "Calm Box" (4s×4) and "4-7-8" (4-7-8s); set `exercise_count = 2`.
- [ ] Timer presets: three entries — "Pomodoro" (25/5), "Quick Sprint" (10/0), "Deep Work" (45/10); set `preset_count = 3`.
- [ ] Settings: retain current brightness; use safe values for idle timeout (60s) and reading light (off).

#### Points and gamification (via `points_store_get_state()`)

- [ ] Set `total_points = 1200`, `today_points = 85` directly (avoid calling award functions that have side effects).
- [ ] Verify `points_store_get_level()` returns 5 and the plant sprite is `plant_3_sprite` using the real level calculation.
- [ ] Seed three daily goals: "Drink water" (done), "Study 30 min" (pending), "Exercise" (pending).
- [ ] Seed six streaks: focus=7, water=12, breathing=5, goals=3, sleep=9, move=4.
- [ ] Seed point history: 5-8 entries with varied `point_reason_t` values and `esp_timer_get_time()` timestamps.
- [ ] Keep bonus flags consistent: `water_bonus_claimed = true` (matches 6 glasses), `bedtime_bonus_claimed = false`, `all_goals_bonus_claimed = false`.
- [ ] Set `day_key` to today's date via `get_day_key()` or equivalent.

#### Other stores

- [ ] Sleep: call `sleep_store_set_history_entries()` with 7 varied nights:
  ```c
  uint16_t mins[] = {420, 450, 480, 360, 470, 440, 465};
  int16_t starts[] = {1320, 1300, 1290, 1380, 1310, 1340, 1300}; // minutes from midnight
  sleep_store_set_history_entries(mins, starts, 7);
  ```
- [ ] Breathing count: `session_store_set_breath_count(12)`.
- [ ] Stretch break: `sedentary_store_set_enabled(true)`, `sedentary_store_set_interval(45)`, ensure `running = false` and `pending_alert = false`.
- [ ] Rebuild timer cache: `timer_store_init()` after seeding presets.

**Exit criterion:** Every selected screen displays useful, internally consistent information in the simulator.

### Phase 3 — UI integration

- [ ] Add `SCREEN_DEMO` immediately before `SCREEN_COUNT` in `ui_manager.h` (do not assign a literal value).
- [ ] Create `screen_demo.h` with `lv_obj_t *screen_demo_create(void);`.
- [ ] Create `screen_demo.c` implementing a landing screen that:
  - Shows the demo title and a "Press to start" prompt.
  - On start, checks `demo_mode_can_start()` and either begins the demo or shows the failure reason.
  - Can also serve as the completion card (step 9 → show "Restart / Exit").
- [ ] Register `screen_demo_create` in `ui_manager.c` `ui_manager_init()` and add `SCREEN_DEMO` case in `ui_manager_switch_screen()`.
- [ ] Add `screen_event_handlers[SCREEN_DEMO] = screen_demo_encoder_event;` (or NULL if demo handles its own input).
- [ ] Add Demo as the last item in `screen_menu.c`:
  ```c
  { LV_SYMBOL_PLAY, "Demo", SCREEN_DEMO, NULL }
  ```
  Current array is 10 items (line 43-54); parallel widget arrays have 16 slots (lines 57-60), so 11 items is safe.
- [ ] Add `demo_mode.c` and `screen_demo.c` to `main/CMakeLists.txt`.
- [ ] Create the top-layer progress overlay in `demo_mode.c` using `lv_layer_top()`.

**Exit criterion:** Demo appears in the menu, the landing screen loads, and the progress overlay renders.

### Phase 4 — Input interception and exit

Input interception belongs near the beginning of `ui_manager_encoder_event()` (line 505), after the existing `alert_is_showing()` check (line 508) which must remain the highest-priority safety case.

The interception block should be inserted at approximately line 530 (after the alert block, before the rotation-forward block):

```c
/* Demo mode intercepts all encoder input */
if (demo_mode_is_active()) {
    /* Swallow rotation */
    if (data->enc_diff != 0) return;

    /* Press handling — replicate the existing short/long press logic */
    if (data->state == LV_INDEV_STATE_PR && !waiting_for_release) {
        press_start_tick = lv_tick_get();
        waiting_for_release = true;
        long_press_fired = false;
        return;
    }
    if (waiting_for_release && data->state == LV_INDEV_STATE_PR) {
        if (!long_press_fired && lv_tick_elaps(press_start_tick) >= LONG_PRESS_MS) {
            long_press_fired = true;
            demo_mode_stop(true);  /* restore and exit */
        }
        return;
    }
    if (data->state == LV_INDEV_STATE_REL && waiting_for_release) {
        bool was_long = long_press_fired;
        waiting_for_release = false;
        press_start_tick = 0;
        long_press_fired = false;
        if (!was_long) {
            demo_mode_advance();  /* next step */
        }
        return;
    }
    return;
}
```

Additional requirements:

- [ ] Preserve the existing `alert_is_showing()` check as the first guard (alerts override demo).
- [ ] Do not forward demo presses into the underlying feature screen handler.
- [ ] Remove the overlay on every exit path (both advance past step 9 and long-press exit).
- [ ] Prevent a held button from causing both advance and exit (the `was_long` guard handles this).

**Exit criterion:** The presenter cannot modify seeded data accidentally while stepping through the walkthrough.

### Phase 5 — Restore, refresh and synchronise

- [ ] Restore `app_state_t` by memcpy from `s_backup.app` back to `app_state_get()`.
- [ ] Restore `points_state_t` by memcpy from `s_backup.points` back to `points_store_get_state()`.
- [ ] Restore sleep history via `sleep_store_set_history_entries(s_backup.sleep_minutes, s_backup.sleep_start_minutes, s_backup.sleep_count)`.
- [ ] Restore breathing count via `session_store_set_breath_count(s_backup.breathing_session_count)`.
- [ ] Restore sedentary state by memcpy from `s_backup.sedentary` back to `sedentary_store_get_state()`.
- [ ] Rebuild timer cache: `timer_store_init()`.
- [ ] Refresh the relevant LVGL screens by calling `ui_manager_switch_screen(s_backup.previous_screen)`.
- [ ] Broadcast restored state to the web companion: call `app_state_broadcast_full_sync()` or equivalent `app_state_broadcast_*` functions declared in `app_state.h:119-132`.
- [ ] Resume persistence: `persistence_set_suspended(false)`.
- [ ] Clear the backup: `memset(&s_backup, 0, sizeof(s_backup)); s_backup.valid = false;`.
- [ ] Return to Menu.

**Exit criterion:** A before/after comparison of all backed-up data is identical. Run `demo_mode_start()` → `demo_mode_stop(true)` → compare all stores.

### Phase 6 — Verification

#### Simulator

- [ ] Start and complete the full nine-step sequence (use headless script: navigate to menu → press Demo → advance 9 times).
- [ ] Exit early from every individual step (long press at each step).
- [ ] Start/exit the mode three times consecutively.
- [ ] Attempt to start during an active timer — confirm rejection message.
- [ ] Attempt to start during an active sleep log — confirm rejection message.
- [ ] Confirm rotation has no effect during demo.
- [ ] Confirm the overlay remains above every screen.

#### Persistence

- [ ] Record the content of `studybud.json` before demo.
- [ ] Run Demo longer than the 5-second save interval.
- [ ] Exit and confirm the file was never replaced with demo data.
- [ ] Reboot and confirm the original data loads.

#### Hardware

- [ ] Flash to the ESP32 and test with the final rotary encoder.
- [ ] Confirm short press (<800 ms) and long press (≥800 ms) are distinct.
- [ ] Check text remains legible within the circular display.
- [ ] Watch free heap before, during and after the walkthrough (`esp_get_free_heap_size()`).
- [ ] Confirm no overlay or timer remains after exit.
- [ ] Test with the web companion disconnected and connected.

---

## 6. Risks and Controls

| Risk | Impact | Control |
|---|---|---|
| Demo state is saved to SD card | Critical | Suspend persistence before seeding; restore before resuming |
| Snapshot omits a secondary store | High | Use the repository-derived snapshot checklist (section 4.3) |
| Timer cache disagrees with restored `app_state` | High | Call `timer_store_init()` after seed and restore |
| Background timer mutates data during demo | High | Block active timer/sleep; neutralise sedentary runtime |
| Demo input leaks into a feature screen | High | Intercept before normal screen handlers; test every step |
| Web companion retains demo data | Medium | Broadcast restored full sync on exit; coordinate web demo in v2 |
| Overlay hides important content | Medium | Use a narrow pill; inspect every screen on hardware |
| Screen enum/index arrays fall out of sync | Medium | Insert before `SCREEN_COUNT`; initialise all entries |
| Repeated starts overwrite the original backup | High | Reject `start()` while already active |
| Snapshot uses too much stack space | Medium | Allocate `demo_backup_t` statically (file-scope) or on heap with `malloc` |
| `sedentary_tick_cb` fires during demo | Low | Sedentary state is neutralised (not running, no pending alert); tick is harmless |
| Timer background callback fires during demo | Low | Timer is not running; callback checks `is_running` flag |

---

## 7. Definition of Done

- [ ] Demo is selectable from the final device menu.
- [ ] It displays nine purposeful steps using real application screens.
- [ ] Short press advances and long press exits.
- [ ] Rotation cannot change data during the sequence.
- [ ] The progress indicator is visible and accurate.
- [ ] Seed data is internally consistent across screens.
- [ ] Original runtime state is restored after completion and early exit.
- [ ] Demo data is never persisted to `studybud.json`.
- [ ] Connected web clients receive restored state after exit.
- [ ] Simulator and real-hardware tests pass.
- [ ] No warnings, crashes, memory leaks or orphaned LVGL objects are observed.

Binary growth should be measured. The earlier fixed "under 10 KB" limit should be treated as a target until a release-build comparison establishes what is realistic.

---

## 8. Suggested Folio Evidence

- Demo landing screen and the presentation problem it addresses.
- Snapshot → seed → walkthrough → restore state-machine diagram.
- Simulator captures of three representative steps.
- Persistence test showing the original file unchanged.
- Hardware photo showing the progress overlay.
- Compact test/result/fault/applied-correction table.

The folio should describe Demo mode as a presentation and validation aid, not as a normal end-user wellbeing feature.

---

## 9. Later Improvements

1. Coordinated web-companion demo banner and localStorage reset.
2. Optional automatic advancement.
3. Presenter-selectable screen sequences.
4. Screenshot-capture automation.
5. Developer-only "keep demo data" option.
6. Custom scripts loaded from the SD card.

---

## 10. Relevant Files

| File | Relevance | Key lines |
|---|---|---|
| `main/display/ui_manager.h` | Screen enum (`screen_id_t`), public API | Lines 9-25 (enum), 27-31 (API) |
| `main/display/ui_manager.c` | Screen switching, encoder event routing, overlays | Lines 455-503 (switch), 505-601 (encoder), 128/149/193/254 (lv_layer_top usage) |
| `main/display/screens/screen_menu.c` | Menu item array, selection handler | Lines 43-54 (items), 57-60 (widget arrays, 16 slots), 392 (encoder), 415 (get_selection) |
| `main/display/app_state.h` | `app_state_t` struct, broadcast API | Lines 81-107 (struct), 109 (ws_broadcast_fn), 113 (app_state_get), 119-132 (broadcast functions) |
| `main/display/app_state.c` | Display stub implementation | Lines 29-32 (app_state_get) |
| `main/networking/app_state.c` | Real implementation, WebSocket dispatch | Lines 115-118 (app_state_get), 167-174 (broadcast_state) |
| `main/display/utils/points_store.h` | `points_state_t`, level/goal/streak types | Lines 95-121 (struct), 124 (get_state) |
| `main/display/utils/points_store.c` | Points operations, level calculation | Line 77 (get_state) |
| `main/display/utils/sleep_store.h` | Sleep history API, active-session check | Lines 11-12 (seed_demo, is_active), 21 (set_history_entries) |
| `main/display/utils/sleep_store.c` | Sleep implementation | Lines 33-36 (is_active), 38-46 (seed_demo), 153-161 (set_history_entries) |
| `main/display/utils/session_store.h` | Breathing session count | Line 10 (set_breath_count) |
| `main/display/utils/session_store.c` | Session count implementation | Lines 20-23 |
| `main/display/utils/timer_store.h` | Timer preset cache, init | Line 25 (init) |
| `main/display/utils/timer_store.c` | Timer cache rebuild | Lines 40-48 (rebuild_cache), 50-54 (init) |
| `main/display/utils/sedentary_store.h` | `sedentary_state_t`, control API | Lines 23-47 (struct), 49-72 (API) |
| `main/display/utils/sedentary_store.c` | Sedentary implementation | Line 89 (get_state) |
| `main/display/utils/persistence.h` | SD-card save API | Lines 9-10 (save, mark_dirty) |
| `main/display/utils/persistence.c` | Save mechanism, timer callback | Lines 18 (SAVE_INTERVAL), 21 (s_dirty), 482 (save), 650-661 (timer_cb), 663-666 (mark_dirty), 668-693 (init) |
| `main/CMakeLists.txt` | Build system — add new .c files | N/A |
| `DEMO_SEED_PLAN.md` | Broader device/web seed-data requirements | Reference document |
| `main/display/screens/screen_tamagotchi.c` | Plant sprite logic | Line 392 (srand(42) for deterministic sprite) |

---

## Version History

| Version | Date | Changes |
|---|---|---|
| 1.0 | 21 Aug 2026 | Initial roadmap |
| 1.1 | 21 Aug 2026 | Expanded after repository-wide architecture review; corrected state ownership, persistence safety, screen integration, preconditions and test criteria |
| 2.0 | 21 Aug 2026 | Finalised with verified API signatures, line-number references, concrete implementation code for persistence suspension and encoder interception, memory cost estimates, dual app_state warning, and menu capacity check |

**Next review:** After Phase 0 persistence-suspension test.
