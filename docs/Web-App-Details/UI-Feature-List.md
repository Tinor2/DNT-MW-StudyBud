# StudyBud — UI Feature List

## Overview

StudyBud is a **stress management companion** — a desk device that helps you stay calm, hydrated, focused, and balanced. It's not just a study timer; it's a holistic wellness tool for your workspace.

The device has two UIs that share the same state via WebSocket:

- **Web App** — Svelte SPA (Progressive Web App) served from ESP32's TF card, cached by browser for offline access
- **Display UI** — LVGL/C running natively on the ESP32's 480×480 round LCD

The ESP32 is the single source of truth. The web app is for **creation and management** (adding tasks, editing presets, configuring settings). The display is for **execution and monitoring** (starting timers, marking tasks complete, logging water). During work sessions, the user shouldn't need to touch their phone.

---

## Feature Matrix

| Feature | Web App (Svelte) | Display (LVGL) | Sync | Priority |
|---|---|---|---|---|
| Home (Clock) | Dashboard with stats | Clock + session info | Device → Display | V1 |
| Timer | Full controls + presets | Countdown + progress ring | Bidirectional | V1 |
| Timer Presets | Editor with Pomodoro highlight | Scrollable radial list | Bidirectional | V1 |
| Todo List | CRUD + priority tags | Radial task list + encoder | Bidirectional | V1 |
| Water Tracker | Log + history + goals | Intake counter + encoder log | Bidirectional | V1 |
| Breathing | Exercise selector + guide | Guided breathing animation | Bidirectional | V1 |
| Sedentary Tracker | Settings + history | Movement alert + timer | Device → Display | V1 |
| Tamagotchi | Stats + care actions | Pet display + interactions | Bidirectional | V2 |
| Idle Background | Upload + gallery | Full-screen pixel art | App → Device | V1 |
| Settings | Full config panel | Quick adjustments | Bidirectional | V1 |
| Notifications | History + preferences | Toast overlay | Device → Both | V1 |
| Connection Status | WS indicator + reconnect | WiFi icon (in Home) | Device → Both | V1 |

---

## Feature Details

### 1. Home (Clock + Dashboard)

**Purpose:** The default screen. Shows the time and subtle device status. This is what the display shows most of the time — calm, informative, unobtrusive.

**Web App:**
- Dashboard overview: today's stats at a glance
- Active session indicator (if timer is running)
- Quick links to all features
- Recent activity feed

**Display (LVGL):**
- Large digital clock (centered, clean font)
- Day and date below clock
- Subtle status indicators (bottom of screen):
  - Session count for today (e.g., "3 sessions")
  - Water intake (e.g., "5/8 glasses")
  - Tamagotchi mood (if implemented)
- No status bar — information is integrated into the layout
- Pixel art idle background shows through when display is idle

**WebSocket Messages:**
| Type | Direction | Payload |
|---|---|---|
| `clock_update` | Device → App | `{time, date, day_of_week}` |
| `home_status` | Device → App | `{sessions_today, water_glasses, tamagotchi_mood}` |

---

### 2. Timer

**Purpose:** Focus and break timer with configurable presets. The timer is a tool within the stress management ecosystem — not the sole focus.

**Web App:**
- Large circular progress ring with countdown
- Start / Pause / Reset / Skip buttons
- Current phase label (Focus / Break)
- Preset selector with Pomodoro highlighted (bold, different color)
- Visual notification when timer completes

**Display (LVGL):**
- Two screens within the Timer feature:
  1. **Preset List** — Radial scroll of presets, Pomodoro highlighted
  2. **Timer Screen** — Large countdown digits, progress ring, phase label
- Encoder: rotate to scroll presets, press to start/pause
- Different color for focus (warm amber) vs break (soft blue)
- Speaker plays gentle chime on completion
- User presses encoder to start breaks (not auto-start)

**WebSocket Messages:**
| Type | Direction | Payload |
|---|---|---|
| `timer_update` | Device → App | `{remaining, phase, preset_id, running}` |
| `timer_command` | App → Device | `{action: "start" \| "pause" \| "reset" \| "skip"}` |
| `timer_complete` | Device → App | `{phase, preset_id}` |

---

### 3. Timer Presets

**Purpose:** Save and switch between different timer configurations.

**Web App:**
- List of presets with name, duration, color
- **Pomodoro preset** — bold, different color, pinned at top (default)
- Create / edit / delete presets
- Drag to reorder
- Color picker for each preset
- Default presets: "Pomodoro" (25/5/15), "Custom"

**Display (LVGL):**
- Radial scroll list of preset names
- Pomodoro highlighted with accent color and bold text
- Encoder: rotate to scroll, press to select
- Selected preset transitions to timer screen

**WebSocket Messages:**
| Type | Direction | Payload |
|---|---|---|
| `presets_sync` | Device → App | `{presets: [{id, name, focus, break_duration, color, is_pomodoro}]}` |
| `preset_select` | App → Device | `{preset_id}` |
| `preset_add` | App → Device | `{name, focus, break_duration, color}` |
| `preset_update` | App → Device | `{id, ...fields}` |
| `preset_delete` | App → Device | `{id}` |

---

### 4. Todo List

**Purpose:** Simple task management. Not the focus of the product, but a useful tool for tracking what needs to be done.

**Web App:**
- List of tasks with checkboxes
- Add / edit / delete tasks (keyboard input)
- Priority tags: High / Medium / Low (color-coded)
- Drag to reorder
- Mark complete with strike-through animation
- Task count and completion percentage
- **No due dates** — keep it simple
- **4 AM archive:** Completed tasks auto-archive daily at 4 AM (not midnight)

**Display (LVGL):**
- Radial scroll list showing 2-3 tasks at a time
- Active task centered and enlarged
- Completed tasks: strikethrough + 40% opacity, moved to bottom
- Encoder: rotate to scroll, press to toggle complete
- Task count summary (e.g., "3/7 done")
- Long text: wrap to 2 lines, then ellipsis

**WebSocket Messages:**
| Type | Direction | Payload |
|---|---|---|
| `todo_sync` | Device → App | `{tasks: [{id, text, done, priority, order}]}` |
| `todo_add` | App → Device | `{text, priority, order}` |
| `todo_update` | App → Device | `{id, text?, done?, priority?, order?}` |
| `todo_delete` | App → Device | `{id}` |
| `todo_archive` | Device → App | `{archived: [{id, text, completed_at}]}` |

---

### 5. Water Tracker

**Purpose:** Track daily water intake with gentle reminders. Inspired by Water Llama — the potential mascot may appear here.

**Web App:**
- Daily glass count with visual representation
- Log a glass with tap button
- History view (past 7 days)
- Daily goal setting (configurable, default: 8 glasses)
- Reminder interval setting
- Milestone celebrations (halfway, goal reached)

**Display (LVGL):**
- Current intake: large number centered (e.g., "5")
- Goal displayed below (e.g., "/ 8 glasses")
- Progress bar or visual fill
- Encoder press: log one glass (+1)
- Double press: undo last glass (-1)
- Pulsing water drop icon as reminder
- Speaker plays water drop sound on log
- Milestone notification toast (e.g., "Halfway there!")

**WebSocket Messages:**
| Type | Direction | Payload |
|---|---|---|
| `water_update` | Device → App | `{glasses, goal, last_log_time}` |
| `water_log` | App → Device | `{action: "add" \| "remove"}` |
| `water_goal` | App → Device | `{goal: 8}` |
| `water_reminder` | Device → App | `{type: "reminder", message: "Time for water!"}` |

---

### 6. Breathing / Calmness Exercise

**Purpose:** Guided breathing exercises to reduce stress. Core wellness feature.

**Web App:**
- Library of breathing exercises (4-7-8, Box Breathing, Deep Breath, etc.)
- Exercise details: name, description, duration, pattern
- Create / edit custom exercises
- Session history (how many exercises completed)
- Favorites / quick access

**Display (LVGL):**
- List of exercises (radial scroll)
- During exercise:
  - Animated circle that expands/contracts with breathing rhythm
  - Text cue: "Breathe in..." / "Hold..." / "Breathe out..."
  - Subtle color shifts (lavender → sage → blue)
  - Speaker plays soft tone cues (inhale/exhale)
- Encoder press: start / stop exercise
- Long press: exit to menu

**WebSocket Messages:**
| Type | Direction | Payload |
|---|---|---|
| `breathing_sync` | Device → App | `{exercises: [{id, name, pattern, duration}]}` |
| `breathing_start` | App → Device | `{exercise_id}` |
| `breathing_stop` | App → Device | `{}` |
| `breathing_update` | Device → App | `{phase: "inhale" \| "hold" \| "exhale", progress}` |

---

### 7. Sedentary Tracker

**Purpose:** Remind the user to move after prolonged sitting. Uses the QMI8658A accelerometer to detect movement (or lack thereof).

**Web App:**
- Enable / disable sedentary alerts
- Set inactivity timeout (default: 45 minutes)
- Set reminder interval
- History of movement reminders
- Activity log (when user moved, how long since last movement)

**Display (LVGL):**
- No persistent screen — this is a background feature
- When inactivity timeout is reached:
  - Toast notification: "Time to stretch!"
  - Gentle tap sound from speaker
  - Encoder press to dismiss
- After dismissal, timer resets
- Subtle movement detection via accelerometer (if available)

**WebSocket Messages:**
| Type | Direction | Payload |
|---|---|---|
| `sedentary_update` | Device → App | `{enabled, timeout_minutes, last_movement}` |
| `sedentary_alert` | Device → App | `{type: "reminder", message: "Time to stretch!"}` |
| `sedentary_settings` | App → Device | `{enabled?, timeout_minutes?}` |
| `sedentary_dismiss` | App → Device | `{}` |

---

### 8. Tamagotchi (Virtual Pet)

**Purpose:** A virtual companion that lives on the desk buddy. Adds personality and engagement. The pet reacts to the user's habits (hydration, focus, movement).

**Note:** This is a V2 feature — planned but not in initial scope.

**Web App:**
- Pet stats: happiness, hunger, energy, health
- Care actions: Feed, Play, Pet
- Activity history
- Customization (pet name, appearance)

**Display (LVGL):**
- Pet displayed on its own screen (pixel art creature)
- Pet reacts to user behavior:
  - Happy when user hydrates regularly
  - Sad when sedentary for too long
  - Excited when user completes tasks
  - Sleepy at night
- Encoder: scroll through care actions, press to execute
- Pet has idle animations (blinking, bouncing, sleeping)

**WebSocket Messages:**
| Type | Direction | Payload |
|---|---|---|
| `tamagotchi_sync` | Device → App | `{happiness, hunger, energy, health, mood}` |
| `tamagotchi_action` | App → Device | `{action: "feed" \| "play" \| "pet"}` |
| `tamagotchi_update` | Device → App | `{stat, delta}` |

---

### 9. Idle Background

**Purpose:** Display a custom pixel art image when the device is idle. This is where the desk buddy's personality shines.

**Web App:**
- Gallery of available backgrounds
- Upload custom images (JPEG, stored on TF card)
- Preview before applying
- Select / delete backgrounds
- Auto-switch on idle (configurable timeout)

**Display (LVGL):**
- Full-screen pixel art background when idle
- Optional clock overlay (small, unobtrusive)
- Optional session indicators (subtle)
- Transitions to active UI on any encoder interaction
- Image decoded from JPEG on TF card → RGB565 in PSRAM

**WebSocket Messages:**
| Type | Direction | Payload |
|---|---|---|
| `background_sync` | Device → App | `{current, available: [{id, name}]}` |
| `background_select` | App → Device | `{background_id}` |
| `background_upload` | App → Device | JPEG file (HTTP POST) |

---

### 10. Settings

**Purpose:** Device configuration and preferences.

**Web App:**
- Display brightness slider
- Speaker volume (0-100%, default 40%)
- WiFi network management
- Theme / color palette selection
- Idle timeout duration
- Water reminder interval
- Sedentary timeout
- Breathing exercise preferences
- Tamagotchi settings (V2)
- Factory reset option
- Archived tasks viewer

**Display (LVGL):**
- Quick access via menu wheel
- Brightness adjustment
- Volume adjustment
- Minimal UI — most settings managed via web app

**WebSocket Messages:**
| Type | Direction | Payload |
|---|---|---|
| `settings_sync` | Device → App | `{brightness, volume, idle_timeout, ...}` |
| `settings_update` | App → Device | `{field, value}` |

---

### 11. Notifications

**Purpose:** Non-intrusive alerts from various features. Accessible as a toast overlay and a dedicated notification history screen.

**Web App:**
- Notification preferences (enable/disable per type)
- Notification history
- Push notification support (silent, no audio)

**Display (LVGL):**
- **Toast overlay:** Appears at top of current screen, auto-dismisses after 4 seconds
- **Notification history screen:** Accessible from menu wheel, shows past notifications
- Priority queue: Timer > Sedentary > Water > Breathing > Tamagotchi
- Speaker plays subtle sound for each notification type

**WebSocket Messages:**
| Type | Direction | Payload |
|---|---|---|
| `notification` | Device → App | `{type, message, timestamp}` |
| `notification_dismiss` | App → Device | `{notification_id}` |
| `notification_clear` | App → Device | `{}` |

---

### 12. Connection Status & Auth

**Purpose:** Show WiFi/WebSocket connection and manage authentication.

**Web App:**
- Green/red indicator for connection status
- Reconnection button
- Device IP / hostname display
- Signal strength indicator
- **Login page:** Single password input (one user per device)
- **Session management:** Token stored in browser localStorage
- **Token refresh:** Re-authenticate if token expires

**Display (LVGL):**
- Shown on Home screen (not a persistent status bar)
- WiFi icon + signal strength
- Visible in Settings menu
- No dedicated screen — integrated into existing UI

**WebSocket Messages:**
| Type | Direction | Payload |
|---|---|---|
| `heartbeat` | Device → App | `{uptime, wifi_rssi, free_heap, ...}` |

---

## Architecture Notes

### Phone Independence

The desk buddy is designed to function **without the phone** during work sessions:

| Task | Phone Needed? | Display Action |
|---|---|---|
| Start/pause timer | No | Encoder press |
| Mark task complete | No | Encoder press |
| Log water | No | Encoder press |
| Start breathing exercise | No | Encoder press |
| Dismiss sedentary alert | No | Encoder press |
| Add new task | Yes (web app) | Keyboard input |
| Edit presets | Yes (web app) | Full editor |
| Change settings | Yes (web app) | Full panel |
| Upload backgrounds | Yes (web app) | File picker |
| Login | Yes (web app, first time) | N/A |

### Speaker Usage

The speaker plays subtle sounds across multiple features:

| Feature | Sound | When |
|---|---|---|
| Timer | Soft chime | Session complete |
| Water | Water drop | Glass logged |
| Breathing | Soft tone | Inhale/exhale cues |
| Sedentary | Gentle tap | Inactivity alert |
| Tasks | Subtle pop | Task completed |
| Tamagotchi | Cute sound | Pet needs attention (V2) |
| Navigation | Soft click | Menu selection |

All sounds are short, gentle, and non-jarring. Volume is adjustable (default 40%).
