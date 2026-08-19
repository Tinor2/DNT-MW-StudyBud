# Demo Seed Data Plan

This document outlines everything needed to seed the StudyBud project so a marker can
see all features without manually interacting with timers, streaks, or waiting for
real-time events.

---

## Current State

Two independent seed systems exist:

| Seed | File | What it covers | Limitations |
|------|------|----------------|-------------|
| Device SD card | `seed_studybud.py` → `studybud.json` | Points, streaks, todos, presets, water, breathing exercises, sleep history, settings | Web goals not synced from device; history timestamps are hardcoded to a fixed epoch |
| Web app localStorage | `App.svelte` (`seedDemo*` functions) | Breathing sessions, sleep/water/breathing daily logs, tamagotchi points/streaks/goals, todos, presets, water/breathing state | Only seeds once (flag keys), stale data on repeat visits, no admin/seed reset mechanism |

**Problem:** The two seed systems are not coordinated. If the marker opens the web app
before connecting to the device, they see web-local seeds. If the device connects, it
overwrites the web state via `full_sync`. The web seeds also use `localStorage` guards
that prevent re-seeding, so the demo looks different on first vs. subsequent visits.

---

## What Needs to Be Demo'd

### A. Device screens (ESP32, via rotary encoder)

| Screen | Key features to show | How to trigger manually | Seed data needed |
|--------|---------------------|------------------------|------------------|
| **Home** | Clock, today's seeds, level, quick stats | Press encoder from boot | Points state with `today > 0`, water/breathing/todos counts |
| **Menu** | List navigation, focus ring animation | Rotate + press from Home | Nothing extra |
| **Tamagotchi — Pet** | Plant growth stage, level chip, seeds display | Navigate from Menu | Total seeds high enough for visible growth (≥300 for beanstalk stage) |
| **Tamagotchi — Goals** | Daily goals checklist, seed awards on toggle, progress bars, bonus on all 3 done | Press Goals pill on Pet screen | 3 goals with labels/metrics, `done` flags mixed (1 done, 2 pending) |
| **Tamagotchi — Streaks** | 5 streak rows, milestone progress bars, multiplier display | Press Streaks pill on Goals screen | Streaks: focus=7, water=12, breathing=5, goals=3, sleep=9 (all ≥3 for milestone visibility) |
| **Breathing** | Exercise list, guided pulsing animation, session completion seeds, cooldown | Select exercise + press to start | 2 exercises with different timing patterns |
| **Water** | +/- glasses, progress bar, goal celebration, sedentary break seeds | Navigate from Menu | `glasses: 6, goal: 8` (near completion for visual impact) |
| **Sleep** | Intro → start → active → summary → weekly review flow | Press to start at "night" | `sleep_history: [420,450,480,360,470,440,465]` (7 days of varied data) |
| **Timer** | Preset list, session/break/long-break phases, completion seeds | Select preset + press to start | 3 presets with different durations |
| **Todos** | Scrollable list, check/uncheck, priority colors, seed awards | Navigate from Menu | 4 todos: 1 done, 3 pending, varied priorities |
| **Backgrounds** | Idle screensaver rotation | Set short idle timeout + wait | Settings with `idle_timeout: 10` |
| **Settings** | Brightness, volume, idle timeout sliders | Navigate from Menu | `brightness: 80, volume: 40, idle_timeout: 60` |
| **Sedentary** | Movement/water break prompt | Triggered by idle timer | Needs idle timeout to fire |

### B. Web app features (Svelte dashboard)

| Tab / Section | Key features to show | Seed data needed |
|---------------|---------------------|------------------|
| **Home (Dashboard)** | Summary cards (todos, water, breathing, sleep, device status) | All device state synced via WebSocket |
| **Tamagotchi** | Seeds overview, daily goals list, streaks with milestones & multipliers, event log | Points state with streaks, goals, history entries |
| **Breathing** | Exercise selection, breathing analysis (favourite exercise, avg time, weekly sessions), cycle bar chart (3D/Week/Month), evaluation badge | `breathSessions` log (8 entries over 7 days), `breathingLog` (5 days of daily cycles) |
| **Water** | +/- controls, goal stepper, progress bar, bar chart (3D/Week/Month), water analysis (avg, goal hit rate, evaluation) | `waterLog` (5 days), `waterState: {glasses: 6, goal: 8}` |
| **Sleep** | Sleep status, nights bar chart, sleep window visualization, weekly avg, sleep evaluation | `sleepLog` (5 days with start times), `sleep_history` from device |
| **Todos** | Add/toggle/edit/delete, priority colors, completed section | 4 todos from device sync |
| **Timer** | Preset grid, live timer status, Pomodoro analytics (sessions today, focus minutes, pomodoros this week, breaks) | Presets from device, `focusLog` entries |
| **Stretch Break** | Sedentary reminder info | Device screen (no web control) |
| **Settings** | Brightness/volume/idle timeout sliders, state export | `settingsState` from device |
| **Event Log** | Scrollable WS message history panel | WebSocket messages accumulate during demo |
| **Notifications** | Toast popups for seed awards, habit events | Generated by user actions during demo |
| **Theme Toggle** | Light/dark mode switch | localStorage (client-side only) |
| **Insights** | InfoIcon tooltips with health/wellness context on each tab | Derived from live data |

### C. Cross-cutting features

| Feature | What to demonstrate | Seed requirement |
|---------|--------------------|--------------------|
| **Seeds economy** | Points earned for every action type, visible in history | History array with diverse reason codes (todo, water, breathing, focus, sleep, bedtime, daily goal, all goals) |
| **Levels & XP** | Level-up progression, XP bar on tamagotchi | `total: 1230` → Level ~3 (base 100 + 150 + 250 + ... staircase) |
| **Streak multipliers** | 100%–200% multiplier display, milestone badges at 3/7/14/30 days | Streaks at varied lengths (3, 5, 7, 9, 12) |
| **Persistence** | State survives reboot (SD card) | `studybud.json` on SD card |
| **WebSocket sync** | Device ↔ web real-time mirroring | Device online + web connected |
| **Daily rollover** | Day key resets today_points, streaks decay if day missed | `day` and `last` fields must equal today's date |

---

## Seed Data Requirements

### 1. Device seed (`seed_studybud.py`)

The existing script is close but needs these adjustments:

- [ ] **History timestamps** — use `time.time()` offsets instead of fixed epoch so the
      event log shows realistic "2h ago", "5h ago" timestamps
- [ ] **Streak `last` fields** — must equal today's date (already done, but re-run
      before every demo)
- [ ] **Sleep history** — keep 7 days of varied data (already present)
- [ ] **Add bedtime bonus seed to history** — currently missing a `reason: 6` entry
- [ ] **Goals** — the 3 goals should have 1 done + 2 pending so the marker can toggle
      them and see seed awards
- [ ] **Water** — `glasses: 6, goal: 8` so the marker can add 2 more to hit the goal
      and see the celebration + bonus

### 2. Web app localStorage seeds

The `seedDemo*` functions in `App.svelte` handle web-side data. To make the demo
repeatable:

- [ ] **Add a demo reset button** — a hidden URL param (e.g. `?reset=1`) or a button
      in Settings that clears all `studybud_seeded_*` localStorage keys and reloads,
      so the marker always sees fresh demo data
- [ ] **Coordinate with device seed** — when the device connects and sends `full_sync`,
      the web should accept it as authoritative and not fight with localStorage seeds
- [ ] **Seed `focusLog`** — currently not seeded by `seedDemoSessions()`. Add ~5 focus
      session entries so the Timer tab's Pomodoro Analytics shows data
- [ ] **Seed `sleepLog` with start times** — the current seed has `start_hour_min` but
      the sleep window visualization needs these to render properly
- [ ] **Seed event log** — pre-populate `studybud_event_log` with ~10 entries of
      different types so the Event Log panel looks populated on first load

---

## Recommended Demo Flow

This is the sequence a marker should follow to see everything in ~5 minutes:

### Phase 1: Boot & overview (30s)
1. Power on the device → **Home screen** shows time, seeds, level
2. Open web app → click **Connect** → device syncs full state
3. Show the **Dashboard** cards mirroring device state

### Phase 2: Tamagotchi & gamification (60s)
4. Navigate to **Tamagotchi** on device → show plant, level chip, seeds
5. Open **Goals** → toggle the 2 pending goals → seeds awarded, bonus on 3rd
6. Open **Streaks** → show all 5 streaks, milestone progress, multipliers
7. Switch to web **Tamagotchi** tab → show same data mirrored, event log

### Phase 3: Study tools (90s)
8. **Timer**: select "Deep Work" preset → start → show running state on both
   device and web. Skip to completion → 25 seeds earned
9. **Todos**: check off "30 minutes of deep reading" → 10 seeds, streak bump
10. **Breathing**: select "Calm Box" → run 1 cycle → show pulsing animation,
    session complete → seeds awarded, breathing streak increments

### Phase 4: Wellness tracking (60s)
11. **Water**: add 2 glasses → hit goal → celebration animation + bonus seeds
12. **Sleep**: show weekly review with 7 days of data, average calculation
13. Web **Sleep** tab → show sleep window visualization + evaluation badge
14. Web **Water** tab → show bar chart, analysis, goal hit rate

### Phase 5: Settings & extras (30s)
15. **Settings**: adjust brightness/volume → show device response + web sync
16. **Backgrounds**: set idle timeout to 10s → wait → show screensaver
17. **Theme toggle** on web → light/dark mode
18. **Event Log** panel → scroll through WS message history

### Phase 6: Persistence & offline (30s)
19. Reboot device → state restored from SD card
20. Disconnect web → reconnect → full_sync restores everything

---

## Implementation Checklist

### High priority
- [ ] Update `seed_studybud.py` to use dynamic timestamps (`time.time()` offsets)
- [ ] Add `reason: 6` (bedtime bonus) entry to history
- [ ] Add demo reset mechanism to web app (clear localStorage seeds + reload)
- [ ] Seed `focusLog` in `seedDemoSessions()` with ~5 completed session entries

### Medium priority
- [ ] Pre-populate web event log with ~10 diverse message types
- [ ] Ensure web `seedDemoDeviceState()` todos match device seed todos
- [ ] Add a `?demo` URL param that auto-connects and disables idle timeout

### Low priority
- [ ] Add a "Demo Mode" banner/indicator in the web UI when seeded data is active
- [ ] Document the seed reset procedure in `DEMO_INSTRUCTIONS.md`
- [ ] Consider a one-shot `?fresh=1` param that clears all localStorage and re-seeds

---

## File Reference

| File | Role |
|------|------|
| `seed_studybud.py` | Generates `studybud.json` for SD card |
| `web-app/src/App.svelte` | Contains `seedDemoSessions()`, `seedDemoLogs()`, `seedDemoTamagotchi()`, `seedDemoDeviceState()` |
| `web-app/src/lib/stores/tamagotchi.js` | `setPointsState()`, `applyPointsEvent()` |
| `web-app/src/lib/stores/goals.js` | `seedDemoGoals()` |
| `web-app/src/lib/stores/notifications.js` | Toast notification system |
| `web-app/src/lib/insights.js` | Evaluation/analysis logic for each tab |
| `main/display/utils/points_store.c` | Points, streaks, levels, goals (device) |
| `main/display/utils/persistence.c` | SD card load/save |
| `main/networking/app_state.c` | WebSocket message dispatch |
