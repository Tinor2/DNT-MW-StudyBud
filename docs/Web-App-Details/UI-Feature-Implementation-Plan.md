# StudyBud — UI Feature Implementation Plan

## Overview

StudyBud is a **stress management companion** — not just a study timer. This document covers the implementation plan for both UIs:

1. **Web App** — Svelte SPA (Progressive Web App) served from TF card, cached by browser for offline access
2. **Display UI** — LVGL v8.2.0/C running natively on ESP32's 480×480 LCD

**Design philosophy:** The web app is for **creation and management** (adding tasks, editing presets, configuring settings). The display is for **execution and monitoring** (starting timers, marking tasks complete, logging water). During work sessions, the user shouldn't need to touch their phone.

---

## Technology Decisions

| Decision | Choice | Reasoning |
|---|---|---|
| Web framework | **Svelte SPA** (compiled via Vite) | Smallest output (~10-30KB gzipped), no runtime overhead, PWA cached |
| Web routing | **svelte-spa-router** | Client-side SPA routing, no server-side rendering needed |
| Web serving | **ESP32 HTTP server** from TF card | Zero external dependencies, hot-swappable assets |
| Display framework | **LVGL v8.2.0** | Industry-standard embedded GUI, vendor demo already has working reference |
| Communication | **WebSocket** (JSON) | Bidirectional real-time, <20ms LAN latency |
| State management | **ESP32 as single source of truth** | Both UIs read/write to same state, no sync conflicts |
| Typography | **Nunito** (Google Fonts) | Clean, slightly rounded, friendly. Via `lv_font_conv` for LVGL |
| Theme system | **Single-file palette** | CSS variables (web) + `#define` constants (LVGL). Easy to swap entire palette |
| Icons | **48×48 pixel art** | Custom-made for menu wheel. Personality layer |

---

## Web App Architecture

### Project Structure

```
studybud-webapp/
├── src/
│   ├── App.svelte                      # Main shell + route definitions
│   ├── main.js                         # Entry point
│   ├── app.css                         # Global styles + Tailwind imports
│   ├── lib/
│   │   ├── components/
│   │   │   ├── layout/
│   │   │   │   ├── Sidebar.svelte      # Collapsible sidebar nav
│   │   │   │   ├── Header.svelte       # Mobile hamburger nav
│   │   │   │   └── PageShell.svelte    # Page wrapper with transitions
│   │   │   ├── home/
│   │   │   │   ├── Dashboard.svelte    # Overview stats
│   │   │   │   ├── SessionCards.svelte # Today's sessions
│   │   │   │   └── QuickActions.svelte # Quick links
│   │   │   ├── timer/
│   │   │   │   ├── TimerView.svelte    # Main timer display
│   │   │   │   ├── PresetList.svelte   # Preset selector
│   │   │   │   └── PresetEditor.svelte # Create/edit presets
│   │   │   ├── todos/
│   │   │   │   ├── TodoList.svelte     # Task list with CRUD
│   │   │   │   ├── TodoItem.svelte     # Individual task
│   │   │   │   └── PriorityTag.svelte  # High/Med/Low badge
│   │   │   ├── water/
│   │   │   │   ├── WaterTracker.svelte # Main water view
│   │   │   │   ├── WaterHistory.svelte # Past 7 days
│   │   │   │   └── WaterGoal.svelte    # Goal settings
│   │   │   ├── breathing/
│   │   │   │   ├── BreathingLibrary.svelte  # Exercise list
│   │   │   │   ├── BreathingPlayer.svelte   # Guided exercise
│   │   │   │   └── BreathingEditor.svelte   # Custom exercises
│   │   │   ├── sedentary/
│   │   │   │   └── SedentarySettings.svelte
│   │   │   ├── tamagotchi/
│   │   │   │   └── TamagotchiView.svelte    # V2
│   │   │   ├── backgrounds/
│   │   │   │   ├── BackgroundGallery.svelte
│   │   │   │   └── BackgroundUpload.svelte
│   │   │   ├── notifications/
│   │   │   │   ├── NotificationHistory.svelte
│   │   │   │   └── NotificationPreferences.svelte
│   │   │   ├── settings/
│   │   │   │   ├── SettingsPanel.svelte
│   │   │   │   ├── BrightnessControl.svelte
│   │   │   │   ├── VolumeControl.svelte
│   │   │   │   └── ArchiveViewer.svelte
│   │   │   └── ConnectionStatus.svelte
│   │   ├── stores/
│   │   │   ├── websocket.js            # WebSocket connection store
│   │   │   ├── appState.js             # Reactive state mirror (single store)
│   │   │   └── auth.js                 # Authentication state
│   │   └── utils/
│   │       ├── api.js                  # REST API helpers
│   │       ├── theme.js                # Theme/palette helpers
│   │       └── time.js                 # Timer formatting
│   ├── routes/
│   │   ├── Login.svelte                # Login page
│   │   ├── PasswordSetup.svelte        # First-time password setup
│   │   └── App.svelte                  # Authenticated app shell
│   └── theme.css                       # CSS custom properties (palette)
├── static/
│   ├── icons/                          # App icons (PWA)
│   ├── fonts/                          # Self-hosted Nunito
│   └── manifest.json                   # PWA manifest
├── package.json
└── vite.config.js
```

### Routing (svelte-spa-router)

The web app uses **`svelte-spa-router`** for client-side routing (plain SPA, not SvelteKit):

```js
// src/App.svelte
<script>
  import Router from 'svelte-spa-router';
  import Login from './routes/Login.svelte';
  import PasswordSetup from './routes/PasswordSetup.svelte';
  import AppShell from './routes/App.svelte';

  const routes = {
    '/login': Login,
    '/setup': PasswordSetup,
    '/': AppShell,       // Authenticated routes (home, timer, todos, etc.)
    '*': Login,          // Fallback to login
  };
</script>

<Router {routes} />
```

### Authentication

Single-user auth — one password for the device:

- **Storage:** Password hash stored in ESP32 NVS (bcrypt, ~60 bytes)
- **Session:** Simple session token (random string) stored in NVS + browser localStorage
- **Flow:** Login → receive token → store in localStorage → include in WebSocket URL as query param (`?token=xxx`)
- **No registration page** — password is set once via `PasswordSetup.svelte` (first-time setup)
- **WebSocket auth:** Pre-handshake callback validates token before WebSocket handshake completes
- **Token refresh:** Re-authenticate if connection returns 401

### Navigation

- **Desktop:** Collapsible sidebar (left side) with pixel art icons + labels
- **Mobile:** Hamburger menu (top) that opens a slide-out drawer
- **Route transitions:** Svelte transitions with fade/slide animations

### Development Workflow

```
1. npm run dev          → Vite dev server on localhost:5173
2. Vite proxies /ws to  → ESP32 (studybud.local:80)
3. Edit .svelte files   → Hot reload in browser
4. Test WebSocket       → Real-time communication with ESP32
```

### Production Build

```
npm run build → dist/
    ├── index.html
    ├── assets/
    │   ├── app.[hash].js      (~10-30KB gzipped)
    │   └── style.[hash].css   (~5-15KB gzipped)
    ├── fonts/                 # Self-hosted Nunito
    └── manifest.json          (PWA)
         │
         ▼
    Copy to TF card → ESP32 serves via HTTP server
    Browser caches via PWA service worker
```

---

## Display UI Architecture (LVGL)

### Hardware Notes

**LEDC Conflict (known issue to fix):**
The speaker (GPIO 20) and LCD backlight (GPIO 6) both use `LEDC_TIMER_0` and `LEDC_CHANNEL_0`. This must be resolved by assigning the speaker to `LEDC_TIMER_1` / `LEDC_CHANNEL_1`.

**PWM Buzzer Limitation:**
The speaker is a simple PWM buzzer (not I2S). It can only produce frequency tones via LEDC — not WAV files or complex audio. All sound design must use simple frequency patterns (e.g., 800Hz for 200ms = tap, 1200Hz → 1600Hz → 2000Hz = ascending chime).

**QMI8658A Accelerometer:**
The sedentary tracker uses the QMI8658A accelerometer (I2C address `0x6B`). Driver code exists in the vendor demo but must be copied into the project and added to `CMakeLists.txt`.

### Source Location

The LVGL driver and example UI exist in the vendor demo:
- `ESP32-S3-LCD-2.8C-Demo/ESP-IDF/ESP32-S3-LCD-2.8C-Test/main/LVGL_Driver/`
- `ESP32-S3-LCD-2.8C-Demo/ESP-IDF/ESP32-S3-LCD-2.8C-Test/main/LVGL_UI/`

These will be adapted into the main StudyBud project.

### Module Structure

```
main/
├── display/
│   ├── lvgl_driver/
│   │   ├── LVGL_Driver.c             # LVGL init, flush callback, tick timer
│   │   └── LVGL_Driver.h
│   ├── lv_theme.h                    # Color palette + font defines
│   ├── screens/
│   │   ├── screen_home.c             # Clock + status (default screen)
│   │   ├── screen_home.h
│   │   ├── screen_menu.c             # Menu wheel navigation
│   │   ├── screen_menu.h
│   │   ├── screen_timer_presets.c    # Preset radial list
│   │   ├── screen_timer_presets.h
│   │   ├── screen_timer.c            # Timer countdown + progress
│   │   ├── screen_timer.h
│   │   ├── screen_todos.c            # Todo radial list
│   │   ├── screen_todos.h
│   │   ├── screen_water.c            # Water tracker
│   │   ├── screen_water.h
│   │   ├── screen_breathing.c        # Breathing exercise
│   │   ├── screen_breathing.h
│   │   ├── screen_tamagotchi.c       # Virtual pet (V2)
│   │   ├── screen_tamagotchi.h
│   │   ├── screen_idle.c             # Background image display
│   │   ├── screen_idle.h
│   │   ├── screen_notifications.c    # Notification history
│   │   ├── screen_notifications.h
│   │   ├── screen_settings.c         # Quick settings
│   │   └── screen_settings.h
│   ├── widgets/
│   │   ├── radial_list.c             # Reusable radial scroll widget
│   │   ├── radial_list.h
│   │   ├── progress_ring.c           # Circular progress indicator
│   │   ├── progress_ring.h
│   │   ├── toast.c                   # Notification toast overlay
│   │   ├── toast.h
│   │   ├── menu_wheel.c              # Menu wheel navigation
│   │   └── menu_wheel.h
│   ├── fonts/
│   │   ├── font_12.c                 # Nunito 12px (via lv_font_conv)
│   │   ├── font_16.c                 # Nunito 16px
│   │   ├── font_20_bold.c            # Nunito 20px Bold
│   │   ├── font_28.c                 # Nunito 28px
│   │   └── font_36.c                 # Nunito 36px (timer digits)
│   ├── icons/
│   │   ├── icon_home.c               # 48×48 pixel art (LVGL bitmap)
│   │   ├── icon_timer.c
│   │   ├── icon_todos.c
│   │   ├── icon_water.c
│   │   ├── icon_breathing.c
│   │   ├── icon_sedentary.c
│   │   ├── icon_tamagotchi.c
│   │   ├── icon_backgrounds.c
│   │   ├── icon_notifications.c
│   │   └── icon_settings.c
│   ├── ui_manager.c                  # Screen navigation, encoder routing
│   └── ui_manager.h
```

### Encoder Navigation

The rotary encoder controls the display UI:
- **Rotate:** Scroll through items / adjust values
- **Short press:** Select / toggle / confirm
- **Long press:** Open menu wheel (from any screen) or back

See `Display-Interaction-Guide.md` for full encoder behavior.

### Widgets

Uses **existing LVGL widgets styled creatively** (not fully custom widgets):

| Widget | Purpose | LVGL Base | Approach |
|---|---|---|---|
| Radial list | Magnifying center item for todos, presets | `lv_roller` or `lv_list` | Custom styling with opacity gradient + center enlargement |
| Progress ring | Circular timer progress | `lv_arc` | Custom arc styling with theme colors |
| Toast overlay | Non-intrusive notification | `lv_obj` + `lv_label` | Positioned at top, animated fade in/out |
| Menu wheel | Circular icon navigation | `lv_img` + `lv_label` | Existing LVGL objects positioned in radial layout |

---

## Shared State Architecture

### ESP32 State Model

```c
typedef struct {
    // === TIMER ===
    timer_state_t timer;                // remaining, phase, preset_id, running
    preset_t presets[MAX_PRESETS];
    int preset_count;
    int active_preset_id;

    // === TODOS ===
    todo_item_t todos[MAX_TODOS];       // id, text, done, priority, order
    int todo_count;
    todo_item_t archived[MAX_ARCHIVED]; // 4 AM archive
    int archived_count;

    // === WATER ===
    int water_glasses;
    int water_goal;                     // default: 8
    uint32_t water_last_log;

    // === BREATHING ===
    breathing_exercise_t exercises[MAX_EXERCISES];
    int exercise_count;
    breathing_state_t breathing;        // active_exercise, phase, progress

    // === SEDENTARY ===
    bool sedentary_enabled;
    int sedentary_timeout;              // minutes
    uint32_t last_movement;
    bool sedentary_alert_active;

    // === TAMAGOTCHI (V2) ===
    tamagotchi_state_t tamagotchi;      // happiness, hunger, energy, health

    // === IDLE / BACKGROUND ===
    int current_background;
    char background_files[MAX_BACKGROUNDS][64];
    int idle_timeout;                   // seconds

    // === SETTINGS ===
    settings_t settings;                // brightness, volume, theme, etc.

    // === NOTIFICATIONS ===
    notification_t notifications[MAX_NOTIFICATIONS];
    int notification_count;

    // === SYSTEM ===
    uint32_t global_seq;                // Sequence number for conflict detection
    uint32_t uptime;
    int wifi_rssi;
} app_state_t;
```

### Sync Flow

```
Web App                ESP32                 LVGL Display
  │                     │                       │
  │── todo_add ────────►│                       │
  │                     │── apply_command()     │
  │                     │── update state        │
  │                     │── broadcast WS ──────►│ (web app gets update)
  │                     │── refresh LVGL ──────►│ (display updates)
  │◄── todo_sync ───────│                       │
  │                     │                       │
```

### Single-Writer Pattern

```
HTTP/WS Task (Core 0)          State Manager Task (Core 0)         LVGL (Core 1)
       │                              │                                    │
       │── Queue: {cmd} ─────────────►│                                    │
       │                              │── validate + write app_state_t     │
       │                              │── broadcast WS to all clients      │
       │                              │── trigger LVGL refresh             │
       │                              │                                    │
       │                              │                    reads app_state_t ──► paint
```

- Only the State Manager task writes to `app_state_t` (single writer, no races)
- LVGL only reads — no lock needed
- HTTP handlers enqueue commands, never touch `app_state_t` directly

---

## WebSocket Message Reference

### Device → App

| Type | Payload | When |
|---|---|---|
| `full_sync` | `{timer, todos, water, breathing, presets, settings, stats}` | On reconnect |
| `clock_update` | `{time, date, day_of_week}` | Every minute |
| `home_status` | `{sessions_today, water_glasses, tamagotchi_mood}` | On change |
| `timer_update` | `{remaining, phase, preset_id, running}` | Every second (when running) |
| `timer_complete` | `{phase, preset_id}` | Session ends |
| `todo_sync` | `{tasks: [{id, text, done, priority, order}]}` | On change |
| `presets_sync` | `{presets: [{id, name, focus, break_duration, color, is_pomodoro}]}` | On change |
| `water_update` | `{glasses, goal, last_log_time}` | On change |
| `breathing_update` | `{phase: "inhale"\|"hold"\|"exhale", progress}` | During exercise |
| `sedentary_alert` | `{type: "reminder", message}` | Inactivity timeout |
| `tamagotchi_sync` | `{happiness, hunger, energy, health}` | On change |
| `background_sync` | `{current, available: [{id, name}]}` | On change |
| `settings_sync` | `{brightness, volume, idle_timeout, ...}` | On change |
| `notification` | `{type, message, timestamp}` | On event |
| `heartbeat` | `{uptime, wifi_rssi, free_heap}` | Every 5 seconds |

### App → Device

| Type | Payload | When |
|---|---|---|
| `timer_command` | `{action: "start"\|"pause"\|"reset"\|"skip"}` | User action |
| `preset_select` | `{preset_id}` | User selects preset |
| `preset_add` | `{name, focus, break_duration, color}` | User creates preset |
| `preset_update` | `{id, ...fields}` | User edits preset |
| `preset_delete` | `{id}` | User deletes preset |
| `todo_add` | `{text, priority, order}` | User adds task |
| `todo_update` | `{id, text?, done?, priority?, order?}` | User edits task |
| `todo_delete` | `{id}` | User deletes task |
| `water_log` | `{action: "add"\|"remove"}` | User logs water |
| `water_goal` | `{goal: 8}` | User changes goal |
| `breathing_start` | `{exercise_id}` | User starts exercise |
| `breathing_stop` | `{}` | User stops exercise |
| `sedentary_settings` | `{enabled?, timeout_minutes?}` | User changes settings |
| `sedentary_dismiss` | `{}` | User dismisses alert |
| `tamagotchi_action` | `{action: "feed"\|"play"\|"pet"}` | V2 |
| `background_select` | `{background_id}` | User selects background |
| `settings_update` | `{field, value}` | User changes setting |

---

## Feature Implementation Phases

### Phase 1: Foundation (Week 1-2)

**Goal:** Working timer with web app + display sync.

- [ ] Fix LEDC conflict (speaker on TIMER_1/CHANNEL_1, backlight on TIMER_0/CHANNEL_0)
- [ ] Set up Svelte project with Vite + svelte-spa-router
- [ ] Implement WebSocket store (connection, reconnection, message routing)
- [ ] Set up theme system (CSS variables + LVGL defines)
- [ ] Integrate LVGL into main project (from vendor demo reference)
- [ ] Build menu wheel navigation (LVGL, using existing widgets creatively)
- [ ] Build basic encoder input handling
- [ ] Implement timer core logic on ESP32
- [ ] Build timer display screen (LVGL) with progress ring
- [ ] Build timer web UI (Svelte) with preset selector
- [ ] End-to-end: start timer on web → see it on display
- [ ] Add Nunito font to LVGL (via `lv_font_conv`)
- [ ] Copy QMI8658A driver from vendor demo into project

### Phase 2: Core Features (Week 3-4)

**Goal:** Todos + water tracker working end-to-end.

- [ ] Todo list: ESP32 state + WebSocket messages
- [ ] Todo list: web UI (CRUD, priority tags, drag reorder)
- [ ] Todo list: LVGL display (radial list, 2-line text wrapping, ellipsis)
- [ ] Todo archive: 4 AM rollover logic
- [ ] Water tracker: ESP32 state + WebSocket messages
- [ ] Water tracker: web UI (log, history, goals)
- [ ] Water tracker: LVGL display (counter + encoder log)
- [ ] Notification toast widget (LVGL)
- [ ] Notification system (queue, display, dismiss)

### Phase 3: Wellness Features (Week 5-6)

**Goal:** Breathing exercises + sedentary tracker.

- [ ] Breathing: ESP32 state + WebSocket messages
- [ ] Breathing: web UI (exercise library, editor)
- [ ] Breathing: LVGL display (guided animation, tone cues)
- [ ] Breathing: speaker integration (soft tone cues)
- [ ] Sedentary: accelerometer integration (QMI8658A)
- [ ] Sedentary: ESP32 state + WebSocket messages
- [ ] Sedentary: web UI (settings, history)
- [ ] Sedentary: LVGL toast notification on alert
- [ ] Sedentary: speaker integration (gentle tap)

### Phase 4: Personality & Customization (Week 7-8)

**Goal:** Idle backgrounds, tamagotchi (V2), settings.

- [ ] Idle background: JPEG storage on TF card
- [ ] Idle background: web UI (upload, gallery, select)
- [ ] Idle background: LVGL display (JPEG decode → RGB565)
- [ ] Tamagotchi: ESP32 state + WebSocket messages (V2)
- [ ] Tamagotchi: web UI (V2)
- [ ] Tamagotchi: LVGL display (V2)
- [ ] Settings: full web UI panel
- [ ] Settings: LVGL quick-access screen
- [ ] Speaker: all sound events implemented

### Phase 5: Auth & Polish (Week 9-10)

**Goal:** Web app auth, PWA, provisioning, final polish.

- [ ] Web app: login page
- [ ] Web app: password setup page (first-time only, single-user)
- [ ] Web app: session management (token in NVS + localStorage)
- [ ] WebSocket auth: pre-handshake callback with query string token
- [ ] PWA setup (manifest.json, service worker, icons)
- [ ] WiFi captive portal provisioning
- [ ] mDNS discovery
- [ ] Connection status UI (both platforms)
- [ ] Error handling, edge cases
- [ ] Performance optimization
- [ ] Testing on real hardware

---

## Font Integration

### Step 1: Download Font

```bash
# Download Nunito from Google Fonts
wget https://fonts.google.com/download?family=Nunito
unzip Nunito.zip -d fonts/
```

### Step 2: Convert for LVGL

```bash
npm install -g lv_font_conv

# Generate font at different sizes (bpp=4 for anti-aliasing)
lv_font_conv --bpp 4 --size 12 --fonts fonts/static/Nunito-Regular.ttf -o main/display/fonts/font_12.c --format lvgl --lv-include lv_theme.h
lv_font_conv --bpp 4 --size 16 --fonts fonts/static/Nunito-Regular.ttf -o main/display/fonts/font_16.c --format lvgl --lv-include lv_theme.h
lv_font_conv --bpp 4 --size 20 --fonts fonts/static/Nunito-SemiBold.ttf -o main/display/fonts/font_20_bold.c --format lvgl --lv-include lv_theme.h
lv_font_conv --bpp 4 --size 28 --fonts fonts/static/Nunito-Bold.ttf -o main/display/fonts/font_28.c --format lvgl --lv-include lv_theme.h
lv_font_conv --bpp 4 --size 36 --fonts fonts/static/Nunito-Bold.ttf -o main/display/fonts/font_36.c --format lvgl --lv-include lv_theme.h
```

### Step 3: Register in LVGL

```c
#include "fonts/font_12.c"
#include "fonts/font_16.c"
#include "fonts/font_20_bold.c"
#include "fonts/font_28.c"
#include "fonts/font_36.c"

// In LVGL init:
lv_theme_t *theme = lv_theme_default_init(disp, LV_COLOR_PRIMARY, LV_COLOR_SECONDARY, false, &font_16);
```

---

## File Deployment to TF Card

```
TF Card Layout:
/
├── firmware/              # Optional: OTA firmware files
├── webapp/                # Svelte build output (PWA cached by browser)
│   ├── index.html
│   ├── assets/
│   │   ├── app.[hash].js
│   │   └── style.[hash].css
│   └── fonts/             # Self-hosted Nunito
│       ├── Nunito-Regular.woff2
│       └── Nunito-Bold.woff2
├── backgrounds/           # Idle background images
│   ├── default.jpeg
│   └── custom_*.jpeg
└── config/                # Device config backups

Note: Speaker sounds are generated in real-time via PWM buzzer (LEDC).
No audio files are stored on the TF card — all sounds are frequency tones
produced by the ESP32's LEDC peripheral at runtime.
```
