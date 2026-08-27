# Web App Design Specification & Roadmap

## 1. Overview

The StudyBud web app supplements the ESP32 LVGL display by providing:
- Detailed data history and analytics the 480×480 screen cannot show
- Convenient input for presets, todos, breathing exercises, and settings
- Persistent data cache (localStorage) that survives page reloads
- Export capabilities for data analysis

The web app is **not** a replica of the LVGL display. It handles only what the ESP32 cannot do well: rich text input, large data visualization, long-term history, and customization management.

---

## 2. Theme System (Light / Dark toggle)

Toggle icon in the header bar, persistent via localStorage.

### Light Mode (matches LVGL display exactly)
| Token | Hex | Usage |
|---|---|---|
| `--color-bg` | `#EFF4EA` | Page background |
| `--color-bg-card` | `#FFFFFF` | Card backgrounds |
| `--color-surface` | `#DDE5D6` | Secondary surfaces |
| `--color-border` | `#BFC8B9` | Borders |
| `--color-text` | `#2D3147` | Primary text |
| `--color-text-secondary` | `#6B7094` | Secondary text |
| `--color-text-muted` | `#9B9FBA` | Muted text |

### Dark Mode (inverted, brighter accents)
| Token | Hex | Usage |
|---|---|---|
| `--color-bg` | `#1a1f2e` | Page background |
| `--color-bg-card` | `#252a3a` | Card backgrounds |
| `--color-surface` | `#2d3348` | Secondary surfaces |
| `--color-border` | `#3d4358` | Borders |
| `--color-text` | `#e8eaf0` | Primary text |
| `--color-text-secondary` | `#a8accc` | Secondary text |
| `--color-text-muted` | `#6b7094` | Muted text |

### Shared Feature Accents (identical in both modes)
| Token | Hex | Feature |
|---|---|---|
| `--color-primary` | `#3B7D4B` | Buttons, headers |
| `--color-primary-light` | `#74A77A` | Focus indicators |
| `--color-primary-dark` | `#2B5A35` | Secondary buttons |
| `--color-secondary` | `#8FBF9A` | Alternate accent |
| `--color-success` | `#5E9F72` | Positive indicators |
| `--color-warning` | `#E0A84C` | Warnings, timer |
| `--color-error` | `#C97A7A` | Errors, high priority |
| `--color-info` | `#5FAF8B` | Info, water, timer-break |

*Dark mode uses **brighter/slightly more saturated** versions of these accents for better contrast on dark backgrounds.*

### Todo Priority Tag Colors (8 colors)
| Label | Hex |
|---|---|
| Red (High) | `#C97A7A` |
| Amber (Medium) | `#E0A84C` |
| Gray (Default) | `#9B9FBA` |
| Green (Low) | `#7DBF9E` |
| Blue (Info) | `#5FAF8B` |
| Purple (Optional) | `#9B7FBF` |
| Teal (Custom) | `#5FAF8B` |
| Pink (Custom) | `#BF7FA0` |

---

## 3. Navigation

### Desktop: Top Tab Bar
- Text + icon tabs: `Home | Timer | Todos | Water | Breathing | Sleep | Settings`
- Active tab: background highlight with `--color-primary` + white text
- Inactive tabs: muted text, hover = subtle background

### Mobile: Bottom Tab Bar
- Icon-only tabs (text hidden)
- Same active/hover behavior

### Header Bar
- Left: App title "StudyBud"
- Right: Connection status dot + Theme toggle icon + Event Log button

### Event Log
- Separate page/modal accessible from header button
- Timestamped list of all WebSocket messages (in/out)
- Filterable by direction and type

---

## 4. Screen Specifications

### 4A. Home Screen — Dashboard

**Layout:** Draggable uniform grid cards (native HTML5 drag-and-drop).
- Desktop: 2-3 column grid
- Mobile: Single column
- Cards can be reordered by user, order saved to localStorage

**Cards:**

| Card | Content |
|---|---|
| **Todo** | Summary: X/Y done, overdue count, priority breakdown (bar of active priorities). Empty: "No todos — add one!" + CTA |
| **Water** | Current glasses/goal, progress bar. Empty: "No water logged today" + CTA |
| **Breathing** | Total sessions today, total time today. Empty: "No breathing sessions today" + CTA |
| **Sleep** | Last night duration, weekly average, goal met indicator. Empty: "No sleep data yet — start a session!" + CTA |
| **Device** | ESP32 uptime, WiFi signal strength, connection status |

**Card behavior:**
- Hover: shadow increase + slight scale (1.02)
- Border-radius: 8px
- Click: navigates to that feature's tab

---

### 4B. Timer / Presets Screen

**Layout:** Card grid (2-3 columns desktop, 1 column mobile).

**Each preset card shows:**
- Preset name
- Type badge (text: "Standard" / "Pomodoro")
- Duration (focus time / session time)
- Today's session count
- Usage streak (consecutive days)
- Last used timestamp (relative: "2h ago", "yesterday")
- Three-dot menu (⋮) with Edit / Delete options

**Add Preset:** Modal popup with fields:
- Name (text input)
- Type selector (Standard / Pomodoro dropdown)
- Focus duration (number, minutes)
- Short break (number, minutes — Pomodoro only)
- Long break (number, minutes — Pomodoro only)
- Sort order (managed by drag reorder in list view, not in modal)

**Preset List View:** Separate view showing all presets in a vertical list for drag reorder.

**Delete Preset:** Confirmation modal ("Are you sure?") with Cancel/Delete buttons.

**Preset type badges:** Text labels only ("Standard" / "Pomodoro").

---

### 4C. Todos Screen

**Layout:** Single vertical list.

**Add Todo:** Inline at top of list — text input + optional tag color selector (8 colors). Default: no tag (optional). Press Enter or click + to add.

**List items:**
- Checkbox to toggle completion
- Text
- Color dot for priority/tag (if assigned)
- Delete button (×) on hover
- Completed items: muted color + strikethrough, moved to bottom of list
- No timestamps shown

**Features:** No search/filter. Simple list only.

**Empty state:** "No todos yet!" + "Add one" CTA.

---

### 4D. Water Screen

**Layout:** Centered widget.

**Controls:**
- Large progress bar (glasses/goal) with filled/empty segments
- Pill-shaped buttons: "+ Add Glass" and "− Remove Glass"
- Goal number input (click to edit, e.g., "8 glasses")

**Chart:**
- 7-day bar chart showing glasses per day
- Dotted goal line in `--color-primary` green
- Linear trend line
- Hover tooltip: exact value ("5 glasses, goal: 8")

---

### 4E. Breathing Screen

**Layout:** Dashboard.

**Pre-made Presets (included):**
| Name | Pattern |
|---|---|
| 4-7-8 Relaxing | Inhale 4s, Hold 7s, Exhale 8s |
| Box Breathing | Inhale 4s, Hold 4s, Exhale 4s, Hold 4s |
| Energizing | Inhale 2s, Exhale 2s (fast) |
| Sleep Prep | Inhale 4s, Hold 7s, Exhale 8s (extended) |

**Custom Exercises:**
- Create/Edit modal: Name, Inhale, Hold1, Exhale, Hold2 (all in seconds)
- Edit/Delete via three-dot menu

**Each exercise card shows:** Name, timing breakdown, total cycle duration, usage count, actions menu.

**Stats summary:**
- All-time session count
- Total time spent breathing (hours/minutes)
- Average session length
- Favorite exercise (most used)
- Current streak (consecutive days with 1+ session)
- Best streak (all-time)

**Weekly chart:** Bar chart of sessions per day (past 7 days).

**Session history:** List of past breathing sessions with exercise name and duration.

---

### 4F. Sleep Screen

**Layout:** Dashboard.

**Nightly Goal:** User sets target hours per night (e.g., 8.0). Stored in localStorage.

**Chart:**
- 7-day bar chart of sleep duration per night
- Traffic light bar colors: green (goal met), amber (close), red (far off)
- Dotted goal line
- Hover tooltip: exact duration + goal status

**Stats:**
- Weekly average duration
- Best night's sleep
- Worst night's sleep
- Goal adherence rate (X of 7 nights met)

**Session log:** Expandable rows, newest first.
- Each row: Start time — End time, Duration, Goal met/not
- Expand: full start/end timestamps, duration in hours+minutes

**Empty state:** "No sleep data yet — start a session on your StudyBud!" + CTA.

---

### 4G. Settings Screen

**Grouped sections:**

**Device Controls:**
- Brightness slider (0–100)
- Volume slider (0–100)
- Idle Timeout number input (5–300 seconds)

**Appearance:**
- Theme toggle (Light / Dark) — also available in header

**Data:**
- Export All Data — downloads localStorage as JSON file
- Clear All Data — confirmation modal then clear

---

## 5. Notifications

**Position:** Top-right corner. Stack up to 2 visible notifications max. If a third arrives, the oldest is replaced.

**Behavior:**
- Auto-dismiss after 4 seconds
- Click to dismiss immediately
- No sound

**Triggered by ALL events:**
- Timer phase complete
- Todo added / toggled / deleted
- Water glass added / removed / goal changed
- Breathing session started / completed
- Sleep session started / completed
- Connection status changes (connected / disconnected / reconnecting)
- Settings updated
- Screen changed on ESP32

**Content:**
- Icon matching the feature (e.g., timer icon for timer events)
- Short message: "Timer phase complete!", "Todo 'Buy groceries' completed", etc.
- Timestamp

---

## 6. Data Architecture

### 6A. ESP32 Storage Plan

**Current state:** All data is in RAM only. No persistence. NVS is initialized but unused by app code.

**Plan:**

**Phase 1 (immediate):** In-memory sync only.
- All data lives in `app_state_t` (RAM) on ESP32
- Web app caches in localStorage as backup
- No flash writes needed

**Phase 2 (future):** NVS for critical settings.
- NVS is small (24KB) but sufficient for: brightness, volume, timeout, water goal, sleep goal
- Timer presets (10 × ~54 bytes = ~540 bytes) could fit
- Key-value pairs: `nvs_set_blob` for preset arrays

**Phase 3 (future):** SD card for bulk data.
- TF card slot exists on ESP32-S3-LCD-2.8C hardware
- FAT32 partition for: breathing session logs, sleep session logs, water daily logs, todo history
- Also: serve web app static files from SD card (remove dependency on Vite proxy in production)
- CSV/JSON export directly from SD card via ESP32 HTTP server

**Why not SD card now:** The immediate goal is bidirectional sync between web app and ESP32 display. Persistence is secondary — the web app already caches in localStorage. SD card integration is a separate task that requires hardware testing and filesystem configuration.

### 6B. Web App Data Strategy

**Hybrid approach:**
- On page load: load cached state from localStorage
- On WebSocket connect: fetch `full_sync` from ESP32, merge into state, update localStorage
- Push-only updates from ESP32: whenever any state changes, ESP32 broadcasts the relevant `*_sync` message
- When disconnected: show cached data + disable action buttons

**localStorage keys:**
```
studybud_event_log       — array of events (max 500)
studybud_todos           — todo array
studybud_presets         — preset array
studybud_water           — { glasses, goal, history[] }
studybud_breathing       — { exercises[], session_history[], stats }
studybud_sleep           — { session_logs[], nightly_goal }
studybud_settings        — { brightness, volume, timeout }
studybud_home_card_order — ordered array of card IDs
studybud_theme           — "light" | "dark"
```

---

## 7. LVGL → Web App Integration Plan

### 7A. Critical Architectural Issue: Dual app_state

The display and networking layers each have their own `app_state_t` instance. They do NOT share state. This is the single biggest integration gap.

**Fix needed:** Unify into a single `app_state_t` singleton that both display code and networking code read/write. The networking `app_state.c` already owns the real instance — display code just needs to include the same header.

### 7B. LVGL Changes Required

#### 1. Todo Screen (`screen_todos.c`)
**Problem:** Hardcoded static array of 6 dummy todos. Independent of `app_state_get()->todos[]`.

**Required changes:**
- Remove static `display_todo_t todo_items[]` array
- Read from `app_state_get()->todos[]` instead
- Wire up: when web app sends `todo_add`/`todo_update`/`todo_delete`, display screen reflects changes
- On toggling a todo via encoder: call `app_state_broadcast_todo_toggled()` (already done)
- Add/Del via encoder: need UI for text input on LVGL (or accept that edits come only from web)

#### 2. Breathing Screen (`screen_breathing.c`)
**Problem:** Single generic cycle. Does not use `app_state->exercises[]`. No exercise selector.

**Required changes:**
- Add exercise selector to LVGL screen (scroll through available exercises via encoder)
- Use `app_state_get()->exercises[exercise_id]` timings instead of hardcoded 3.5s cycle
- When selecting an exercise, send `app_state_broadcast_breathing_exercise_selected(exercise_id)`
- Display exercise name during active state

#### 3. Timer Presets (`screen_timer_presets.c`)
**Problem:** Uses local `timer_store.c` array, independent of `app_state_t` presets.

**Required changes:**
- Replace `timer_store.c` reads with `app_state_get()->presets[]`
- When web app creates/edits/deletes presets via WebSocket, LVGL carousel reflects changes
- `timer_store.c` can be deprecated or merged into app_state

#### 4. Water Screen (Missing)
**Problem:** `SCREEN_WATER` ID declared but no screen file exists. Menu navigates to a null pointer.

**Required changes:**
- Create `screen_water.c` — simple screen showing glass count and goal
- Read from `app_state_get()->water`
- Encoder: rotate to add/remove glasses, press to toggle
- Or at minimum: remove SCREEN_WATER from menu until implemented

#### 5. Home Screen Water Label
**Problem:** `water_label` always shows "0/8" regardless of actual water state.

**Fix:** Read from `app_state_get()->water.glasses` and `app_state_get()->water.goal`.

#### 6. Sleep Data in app_state
**Problem:** Sleep tracking is entirely local to `sleep_store.c`. No fields in `app_state_t`.

**Required changes:**
- Add `sleep_data_t` to `app_state_t` (or at minimum: sleep goal, last session duration, weekly history array)
- Add WebSocket message types: `sleep_session_start`, `sleep_session_end`, `get_sleep_history`
- Add broadcast: `broadcast_sleep_sync()`

#### 7. Settings Screen (Missing)
**Problem:** `SCREEN_SETTINGS` ID declared but no file exists.

**Required changes:**
- Create `screen_settings.c` — simple screen showing brightness, volume, idle timeout
- Read from `app_state_get()->settings`
- Encoder to adjust values

#### 8. Broadcast Integration
**Problem:** Broadcast functions (`broadcast_timer_sync()`, etc.) are only called when WebSocket messages modify state. They are NOT called when LVGL screen code modifies state.

**Required changes:**
- When LVGL timer changes state (pause/reset/skip), call `app_state_broadcast_timer_sync()`
- When LVGL breathing starts/stops, call `app_state_broadcast_breathing_sync()`
- When LVGL settings change, call `app_state_broadcast_settings_sync()`

---

## 8. WebSocket Protocol Extensions

### Current (27 message types)
Already implemented in `app_state_handle_message()`.

### New message types needed for full integration

| Type | Direction | Purpose |
|---|---|---|
| `sleep_session_start` | Web → ESP32 | Start sleep tracking |
| `sleep_session_end` | Web → ESP32 | End sleep tracking, return duration |
| `get_sleep_history` | Web → ESP32 | Fetch sleep log |
| `sleep_sync` | ESP32 → Web | Sleep state broadcast |
| `sleep_goal` | Web → ESP32 | Set nightly sleep goal (in app_state) |
| `breathing_exercise_add` | Web → ESP32 | Add custom exercise |
| `breathing_exercise_update` | Web → ESP32 | Edit custom exercise |
| `breathing_exercise_delete` | Web → ESP32 | Delete custom exercise |
| `breathing_exercise_sync` | ESP32 → Web | Exercise list broadcast |

---

## 9. Implementation Phases

### Phase 1: Core Web App
- [ ] CSS variables + theme toggle (light/dark)
- [ ] Tab bar navigation (responsive: top tabs desktop, bottom tabs mobile)
- [ ] WebSocket connection with auto-connect
- [ ] localStorage caching (load/save state)
- [ ] Notification toast system
- [ ] Event log page

### Phase 2: Screen Implementation
- [ ] Home dashboard (draggable cards)
- [ ] Timer/Presets card grid + modal + drag reorder
- [ ] Todos list (inline add, checkbox, tag colors)
- [ ] Breathing dashboard + custom exercises + charts
- [ ] Sleep dashboard + chart + session log
- [x] Settings page (grouped sections) — *implemented before Water/Sedentary*
- [x] Notifications (top-right toast system) — *implemented before Water/Sedentary*
- [x] Event Log (separate page) — *implemented before Water/Sedentary*

> **Note:** Water and Sedentary screens are deferred until their LVGL counterparts exist. Settings, Notifications, and Event Log are exceptions — implemented now as they are independent of LVGL screen state.

### Phase 3: LVGL Integration
- [ ] Unify app_state (single singleton for both display and networking)
- [ ] Todo screen: read from app_state, remove hardcoded array
- [ ] Breathing screen: exercise selector, use app_state timings
- [ ] Timer presets: replace timer_store with app_state presets
- [ ] Create water screen (LVGL) or wire up home label
- [ ] Create settings screen (LVGL)
- [ ] Add sleep data to app_state + WebSocket messages
- [ ] Wire up broadcast calls from LVGL code

### Phase 4: Polish & Data
- [ ] Add breathing/sleep session history to app_state
- [ ] Add breathing/sleep/water daily history to app_state
- [ ] Export/import JSON from Settings
- [ ] Empty states for all screens
- [ ] Responsive testing (mobile + desktop)
- [ ] Drag reorder persistence

### Phase 5: Future
- [ ] NVS persistence for settings + presets
- [ ] SD card filesystem for bulk history + static file serving
- [ ] Web app served from ESP32 directly (no Vite proxy needed in production)
- [ ] Offline mode with full localStorage operation

---

## 10. Design Principles

1. **Web supplements, not replicates.** The ESP32 shows real-time data and handles physical interaction (encoder, buzzer). The web app shows history, stats, and provides convenient text input.

2. **Push-only sync.** ESP32 broadcasts changes automatically. Web app never polls.

3. **localStorage first.** Web app works offline with cached data. ESP32 connection enhances it.

4. **Minimal LVGL changes.** Prioritize changes that make the LVGL display aware of web-driven state changes. Don't duplicate complex UI (like the breathing circle animation) on the web.

5. **8px border-radius** throughout. Cards, buttons, modals.

6. **Hover effects:** Shadow increase + scale 1.02 on cards.

7. **Charts:** Hover tooltips, goal lines, trend lines where appropriate.
