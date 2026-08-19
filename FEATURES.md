# StudyBud — Feature Guide

StudyBud is a **screen-time companion device** built around a physical desk gadget with a
rotary encoder and a 480×480 round LCD, plus an optional web dashboard. It turns
real-world study habits into a **gamified "seeds" economy**: the better your daily habits
(water, focus, sleep, breathing, todos), the more seeds you earn, the faster your
tamagotchi-style plant grows.

This document walks through every feature of the device firmware and the web companion.

---

## 1. Hardware

| Component | Detail |
|---|---|
| MCU | ESP32-S3 (Wi-Fi 2.4 GHz, BLE), ESP-IDF 6.0.1 |
| Display | Waveshare ESP32-S3-LCD-2.8C, ST7701S driver, 480×480 round LCD |
| Input | Rotary encoder — rotate, short-press (mid press), long-press |
| Audio | On-board speaker (buzzer) |
| Storage | microSD card (holds `studybud.json` state + seed data) |

---

## 2. Architecture at a glance

```
                    ┌───────────────────────────────┐
                    │         ESP32-S3 firmware      │
  ┌──────────┐      │  ┌───────────────────────────┐│
  │ Encoder  │─────▶│  │  Display (LVGL 8, 480×480)││      ┌────────────┐
  │ + Speaker│      │  │  screen_menu / screen_*   ││      │  Web app   │
  └──────────┘      │  └───────────┬───────────────┘│      │  (Svelte)  │
                    │  ┌───────────▼───────────────┐│◀────▶│  ws://     │
                    │  │  app_state (shared state) ││  ws  │  /ws       │
                    │  ├───────────────────────────┤│      └────────────┘
                    │  │  points_store (seeds)     ││
                    │  │  sleep/timer/session stores││
                    │  ├───────────────────────────┤│
                    │  │  persistence → /sdcard/   ││
                    │  └───────────────────────────┘│
                    │  networking: wifi + httpd ws  │
                    └───────────────────────────────┘
```

Two connected halves:

- **Firmware** (`main/`) — the device itself. The ESP32 is the **source of truth**: it owns
  all state, persists it to the SD card, and broadcasts changes over WebSocket.
- **Web app** (`web-app/`) — a Svelte dashboard that mirrors the device state over a
  WebSocket connection. It is a *client*: any state it "writes" is sent to the device,
  which validates and broadcasts authoritative state back.

---

## 3. Device UI & interaction model

Everything runs on a 480×480 round display rendered with LVGL 8.

- **Rotate encoder** — move focus between buttons/rows on screen.
- **Mid press** — activate the focused item (open screen, toggle, increment, start…).
- **Long press** — back / return to previous screen or home.

Focus is drawn as an animated ring around the selected item; transitions between screens
use fade + ring animations. Screens are built once and **show/hide** widgets per internal
state (rather than rebuilding), so switching apps is instant.

Screens defined in `ui_manager.h`: `HOME, MENU, TIMER, TIMER_PRESETS, TIMER_EDIT, TODOS,
WATER, BREATHING, SEDENTARY, SETTINGS, BACKGROUNDS, NOTIFICATIONS, SLEEP, TAMAGOTCHI`.

The **menu** (reached from Home) lists: Home, Tamagotchi, Breathing, Water, Sleep, Timer,
Todos, Backgrounds, Settings.

---

## 4. Device apps, screen by screen

### 4.1 Home
`main/display/screens/screen_home.c`

Dashboard entry point. Shows the current time, today's seeds earned, level, and quick
status summaries (water glasses, next streak milestone, etc.). Press to open the menu.

### 4.2 Tamagotchi (the plant)
`main/display/screens/screen_tamagotchi.c`

The core gamification experience. Three sub-states, navigated with a focus ring:

1. **PET state** — your plant in a pot. It grows through stages (sapling → beanstalk) as
   your total seeds/level increase. A small chip shows `Level N · X seeds`. A large
   circular target (the plant) plus a **Goals** pill are the two main actions.
2. **GOALS state** — daily goals card on top (total seeds, today's seeds, level, progress
   bar), then up to 3 checklist rows (up to `MAX_DAILY_GOALS`). Toggling a goal **awards
   seeds immediately** (`points_store_toggle_goal`, +20 each, +30 bonus when all three are
   done; unchecking revokes them). Rows also show progress bars toward their metric
   target. A "Streaks" pill opens the streaks state; "Back" returns to the plant.
3. **STREAKS state** — 5 rows, one per streakable activity (Focus, Water, Breathing,
   Goals, Sleep). Each row shows the icon, current streak length, and a progress bar to
   the next milestone. Milestones at 3, 7, 14, 30 days — each unlocks a higher seed
   multiplier. Back returns to Goals.

### 4.3 Breathing
`main/display/screens/screen_breathing.c` + `session_store.c`

Guided breathing exercises. Up to `MAX_EXERCISES` (8) exercises, each defined by
inhale / hold / exhale / hold2 durations (e.g. **Box Breathing**, **4-7-8 Relaxation**).
Pick an exercise, run guided cycles with a pulsing animation; completing a session earns
seeds and updates the breathing streak. A cooldown (`BREATHING_COOLDOWN_SEC` = 30 min)
limits repeat-session spam.

### 4.4 Water
`main/display/screens/screen_water.c`

Water tracking with **− / +** buttons and an editable daily **goal**. A progress bar
fills toward the goal with a celebratory label animation on hitting it. Adding a glass
earns seeds; hitting the goal earns a bonus; a sedentary/water break reminder also awards
seeds.

### 4.5 Sleep
`main/display/screens/screen_sleep.c` + `sleep_store.c`

Sleep tracker with five states: **INTRO → START → ACTIVE → SUMMARY → WEEKLY**.

- Press to start a sleep session at night; press again in the morning to end it.
- Duration is tiered into seed rewards: 5 / 10 / 15 / 25 / 50 seeds for ≥1h / 3h / 6h /
  7h / 8h (`POINTS_SLEEP_TRACKED`, `_3H`, `_6H`, `_7H`, `_8H`).
- A **bedtime bonus** (+30) rewards starting before your configured bedtime.
- **Weekly review** shows last-7-days durations and the weekly average — a UI pattern
  later reused for the tamagotchi streaks screen.

### 4.6 Timer (Pomodoro)
`main/display/screens/screen_timer.c`, `screen_timer_presets.c`, `screen_timer_edit.c`
+ `timer_store.c`

Pomodoro / focus timer with up to `MAX_PRESETS` (16) presets, each with a name,
focus length, break length and a "pomodoro" flag. Cycles through **session → short break
→ long break** phases; a long break is offered every 4th session. Completing a focus
session earns `POINTS_FOCUS` (25) seeds and bumps the focus streak. Presets can be added,
edited, selected and deleted from dedicated screens.

### 4.7 Todos
`main/display/screens/screen_todos.c`

Simple task list (up to `MAX_TODOS` = 32). Items have text, a done flag, priority and
manual order. The list is a **scrollable container** with snap scrolling (the pattern the
tamagotchi lists are modeled on). Checking a todo off earns `POINTS_TODO` (10) seeds and
counts toward the todos-done-today stat and the Goals streak.

### 4.8 Backgrounds & idle
`main/display/screens/screen_idle_background.c`

A rotating set of background art / idle screensaver shown when the device is idle. The
idle timeout (seconds) is configurable in Settings.

### 4.9 Settings
`main/display/screens/screen_settings.c`

Adjust **brightness**, **volume** and **idle timeout**. Settings persist to the SD card
and broadcast to the web app (`settings_sync`).

### 4.10 Sedentary / notifications
`main/display/screens/screen_sedentary.c` (enum `SCREEN_SEDENTARY`)

Sits between apps to prompt a movement / water break — the "water break" seed award
(`POINTS_WATER_BREAK` = 10) is granted here.

---

## 5. The seeds (points) system

`main/display/utils/points_store.c` / `.h`

Seeds are the single gamification currency. State tracks `total_points`, `today_points`,
`day_key` (YYYY-MM-DD), and per-day counters (`water_today`, `focus_today`,
`breathing_today`, `todos_done_today`).

### Earning seeds

| Action | Seeds | Reason |
|---|---|---|
| Todo checked | +10 | `POINT_REASON_TODO` |
| Glass of water | +5 | `POINT_REASON_WATER` |
| Water goal reached | +20 | `POINT_REASON_WATER_GOAL` |
| Water/sedentary break | +10 | `POINT_REASON_WATER_BREAK` |
| Breathing session | +10 + 1/cycle (repeat runs give only +3) | `POINT_REASON_BREATHING` |
| Focus timer session complete | +25 | `POINT_REASON_FOCUS` |
| Sleep tracked ≥1h | +5 | `POINT_REASON_SLEEP` |
| Sleep ≥3h / 6h / 7h / 8h | +10 / +15 / +25 / +50 | `POINT_REASON_SLEEP` |
| Bedtime bonus | +30 | `POINT_REASON_BEDTIME` |
| Daily goal toggled done | +20 | `POINT_REASON_DAILY_GOAL` |
| All 3 daily goals done | +30 | `POINT_REASON_ALL_GOALS` |
| Admin add/subtract/reset | ±n | `POINT_REASON_ADMIN` |

Un-checking a goal or removing water **revokes** seeds (history records negative amounts).

### Levels
- XP curve: level 1 needs 100 XP, each subsequent level costs +150 more
  (`LEVEL_BASE_XP = 100`, `LEVEL_XP_INCREMENT = 150`), i.e. cumulative staircase
  (`points_store_get_level`).
- Level drives the plant's growth stage and the chip shown on the tamagotchi screen.

### Streaks & multipliers
- Five streakable activities: **Focus, Water, Breathing, Goals, Sleep**
  (`STREAK_FOCUS=0 … STREAK_SLEEP=4`).
- A streak increments if the activity's `last_active` day is yesterday; it resets to 1 if
  a day is skipped, and isn't double-counted within the same day.
- Multiplier: `100 + 10 * (streak − 1)`, clamped to 100–200% — each consecutive day adds
  +10%, up to **2×** at 11+ days.
- Display milestones at **3, 7, 14, 30** days (web shows "Next: N d · X.X× seeds").

### Daily goals
- Up to 3 goals per day (`MAX_DAILY_GOALS`), each with a label and an optional metric
  target (water / focus / breathing / todos). They roll over daily along with the
  "done" flags and bonus-claim flags.

### History
- Last `POINT_HISTORY_LEN` (50) events are kept: amount, reason, detail, day key,
  timestamp — shown in the web event log and usable for insights.

---

## 6. Persistence

`main/display/utils/persistence.c` / `sd_card.c`

- All authoritative state lives on the **SD card** at `/sdcard/studybud.json`
  (JSON, version 2 format).
- Loaded once at boot; the file is written when state is marked dirty
  (`persistence_mark_dirty()`). The device is therefore the source of truth — a web
  client that connects later gets whatever the device currently has, not the other way
  around.
- Persisted: todos, presets, active timer, water, breathing exercises, settings, the
  full points/streaks/goals state, sleep history (last 7 days + start times), and the
  breathing session counter.

---

## 7. Networking

`main/networking/`

- **Wi-Fi station** (`wifi_manager.c`) connects to your network.
- **HTTP server** (`web_server.c`) on ESP-IDF `httpd`:
  - `GET /ws` — WebSocket endpoint (up to 4 clients).
  - `GET /api/status` — JSON status (uptime, free heap, ws client count).
  - Heartbeat broadcast every 5 s: `{"type":"heartbeat","uptime_ms",...,"free_heap",...,"clients":n}`.
- On WS connect, the device immediately sends a **`full_sync`** snapshot of everything.
- Every incoming WS message is dispatched by its `type` field to
  `app_state_handle_message`, which mutates device state and replies with a JSON response;
  state changes are then **broadcast** to all clients.

### Broadcast / message types
Requests: `get_*` sync queries (todos, breathing, sleep, water, timer, points, settings,
presets) and mutators (add/update/delete/select preset, toggle todo, water inc/dec,
timer start/stop/skip, settings apply, admin points, goal toggle, breathing start…).

Broadcasts: `full_sync`, `todo_sync`, `presets_sync`, `timer_sync`,
`timer_session_complete`, `water_sync`, `points_sync`, `points_earned`,
`breathing_sync`, `breathing_complete`, `sleep_state`, `sleep_session`,
`settings_sync`, `screen_change`, `encoder_event`, `heartbeat`.

The web app mirrors this set in `web-app/src/lib/stores/websocket.js`.

---

## 8. Web companion

`web-app/` — Svelte 4 + Vite 5 dashboard. Run with `npm run dev` and open in a browser;
in dev it proxies `/ws` to `ws://192.168.0.200` (override with the `ESP32_IP` env var).

Tabs (bottom nav): **Home, Tamagotchi 🌱, Breathing, Water, Sleep, Todos, Timer,
Settings**.

- **Connection**: connects to `ws://<host>/ws`. On connect it sends `full_sync`,
  `get_breathing`, `get_sleep`, `get_water`, `get_timer`, `get_points`; it re-syncs a
  subset every 10 s. Reconnects with exponential backoff (1 s → 30 s). Keeps the last
  200 messages + a sequence number for rendering.
- **Tamagotchi page** (`TamagotchiPage.svelte`): seeds overview (total, today, level,
  progress bar), Today's goals, and streaks with milestone logic
  (`streakMilestone/Pct/Label`, milestone multipliers), styled per-activity
  (`STREAK_META`: focus/water/breathing/goals/sleep with distinct colors).
- **Breathing / Water / Sleep / Timer / Todos**: dashboards that edit device state via WS
  (e.g. add water glass, start timer, toggle todo, run breathing) and show live updates
  from broadcasts.
- **Settings** (`Settings.svelte`): push brightness/volume/idle-timeout to the device,
  and **export** the current state as JSON.
- **Local, browser-only state** (localStorage): demo seed sessions, sleep/breathing/water/
  focus logs, goals, and event log — used for demos and insights; the authoritative state
  still lives on the device.
- **Notifications & event log**: toasts for seed awards and habit events, plus a scrollable
  `EventLog.svelte` fed by WS messages. Theme toggle (light/dark) is client-side.
- **Insights** (`web-app/src/lib/insights.js`): derived stats/aggregations from the logs
  (e.g. weekly patterns).

---

## 9. Desktop simulator

`simulator/` — a native build of the same firmware logic (LVGL + ST7701S + persistence
stubbed via `persistence_sim.c`) so screens can be developed and exercised without
hardware. Build outputs `studybud_sim`.

---

## 10. Building & running

Firmware (macOS, ESP-IDF 6.0.1 toolchain):

```sh
source /Users/ronitbhandari/.espressif/tools/activate_idf_v6.0.1.sh
"$IDF_PYTHON_ENV_PATH/bin/python" "$IDF_PATH/tools/idf.py" build
```

`IDF_PYTHON_ENV_PATH=/Users/ronitbhandari/.espressif/python_env/idf6.0_py3.14_env`
(avoids the venv-mismatch abort). Output: `build/studybud.bin`. Flashing an ESP32-S3
that has recovered from a bad state: hold BOOT, connect USB, erase flash, then flash
normally.

Web app:

```sh
cd web-app && npm install && npm run dev   # http://localhost:5173
```

---

## 11. Directory map

```
main/                      ESP-IDF firmware
  networking/              wifi_manager, web_server (HTTP+WS), app_state (message handling)
  display/
    screens/               one .c/.h per device app (home, menu, tamagotchi, breathing,
                           water, sleep, timer, timer_presets, timer_edit, todos, settings,
                           idle_background)
    utils/                 points_store, persistence, sd_card, sleep_store, timer_store,
                           session_store
  assets/                  logo assets (e.g. tamagotchi_logo)
web-app/                   Svelte dashboard
  src/lib/stores/          websocket, tamagotchi, goals, theme, notifications
  src/lib/components/      TamagotchiPage, Settings, EventLog, NotificationToast, InfoIcon
  src/lib/insights.js      derived analytics
simulator/                 native SDL/LVGL harness (persistence_sim, ST7701S stub)
docs/TEMP/                 scratch/mockup assets (tamagotchi_mockup.html, seed json)
```
