# StudyBud — Display Interaction Guide

## Overview

This document defines how the user interacts with the StudyBud display via the rotary encoder. The display is a 480×480 round LCD with a single input device: a rotary encoder with push button.

**Input device:** Rotary encoder (rotate + short press + long press)

---

## Navigation Model

### Menu Wheel

The primary navigation method is a **global menu wheel**, accessed via a **long press** from any screen.

**How it works:**
1. User long-presses the encoder from any screen
2. Current screen fades out (300ms)
3. Menu wheel appears — circular arrangement of pixel art icons around the screen perimeter
4. User rotates encoder to scroll through icons
5. Active icon is **enlarged and centered** (radial magnification)
6. Short press selects the highlighted app
7. Long press dismisses the menu and returns to the previous screen

### Menu Wheel Layout

```
         ╭───────╮
        ╱  HOME   ╲
       │  🕐       │
       │           │
  ╭────┤     ●     ├────╮
  │    │  (center)  │    │
  │    ╰─────┬─────╯    │
  │          │          │
  │    ╭─────┴─────╮    │
  │    │  TIMER ⏳  │    │
   ╲   ╰───────────╯   ╱
    ╲                  ╱
     ╰────────────────╯

     (icons continue around circle)
```

**Visible at once:** 4-6 icons (depending on icon size and screen real estate)
**Scrolling:** Rotate encoder to cycle through all icons
**Active icon:** Centered, enlarged to ~1.5x scale, with label below
**Inactive icons:** Smaller, slightly faded, positioned around the arc

### Feature List (Menu Order)

| Position | Feature | Icon | Notes |
|---|---|---|---|
| 1 | Home | 🕐 Clock | Default screen. Shows time + device status |
| 2 | Timer | ⏳ Hourglass | Preset list → Timer screen |
| 3 | Todos | ☑️ Checklist | Task list with radial scroll |
| 4 | Water | 💧 Droplet | Water intake tracker |
| 5 | Breathing | 🫁 Circle | Breathing/calmness exercises |
| 6 | Sedentary | 🪑 Chair | Movement reminders |
| 7 | Tamagotchi | 🥚 Creature | Virtual pet companion |
| 8 | Backgrounds | 🖼️ Frame | Idle background selector |
| 9 | Notifications | 🔔 Bell | Notification history |
| 10 | Settings | ⚙️ Gear | Device settings |

**Note:** This list will grow over time. The menu wheel scales by reducing icon size or allowing faster scrolling through more items. Maximum practical limit: ~15 items before the wheel feels cluttered.

---

## Encoder Input Map

### Input Actions

| Action | Duration | Context | Result |
|---|---|---|---|
| **Rotate** | — | Menu wheel | Scroll through icons |
| **Rotate** | — | Radial list (todos, presets) | Scroll through items |
| **Rotate** | — | Settings (brightness, volume) | Adjust value |
| **Rotate** | — | Timer running | No action (locked) |
| **Short press** | < 300ms | Menu wheel | Select highlighted app |
| **Short press** | < 300ms | App screen | Context-dependent (see per-app below) |
| **Short press** | < 300ms | Radial list | Toggle select / mark complete |
| **Long press** | > 300ms | Any app screen | Open menu wheel |
| **Long press** | > 300ms | Menu wheel | Dismiss menu, return to previous screen |
| **Long press** | > 300ms | Timer running | Pause timer (with confirmation) |

### Per-App Encoder Behavior

#### Home Screen
- **Rotate:** No action (clock display)
- **Short press:** Toggle between time and device info (WiFi, uptime)
- **Long press:** Open menu wheel

#### Timer
- **Rotate:** Scroll presets (on preset list screen)
- **Short press:** Start / Pause / Resume
- **Long press:** Open menu wheel (or pause if running, with confirmation)

#### Timer Preset List
- **Rotate:** Scroll through presets (radial magnification)
- **Short press:** Select preset → transition to timer screen
- **Long press:** Back to timer (or menu wheel)

#### Todos
- **Rotate:** Scroll through tasks (radial magnification)
- **Short press:** Toggle task complete / incomplete
- **Long press:** Open menu wheel

#### Water Tracker
- **Rotate:** No action (single value display)
- **Short press:** Log one glass of water (+1)
- **Double press:** Undo last glass (-1) — supported (encoder code detects double-click within 300ms)
- **Long press:** Open menu wheel

#### Breathing Exercise
- **Rotate:** Scroll through exercise types (if multiple available)
- **Short press:** Start / Stop breathing exercise
- **Long press:** Open menu wheel (stops exercise first)

#### Sedentary Tracker
- **Rotate:** No action
- **Short press:** Dismiss sedentary alert
- **Long press:** Open menu wheel

#### Tamagotchi
- **Rotate:** Scroll through actions (Feed, Play, Pet)
- **Short press:** Execute selected action
- **Long press:** Open menu wheel

#### Settings
- **Rotate:** Scroll through settings items
- **Short press:** Select item to adjust (brightness, volume, etc.)
- **Long press:** Back to parent menu (or menu wheel)

---

## Screen Transitions

### Transition Types

| Transition | When | Duration | Animation |
|---|---|---|---|
| App → Menu wheel | Long press | 300ms | Fade out current, fade in menu |
| Menu wheel → App | Short press | 300ms | Fade out menu, fade in app |
| Screen → Sub-screen | Short press | 200ms | Slide left (forward) |
| Sub-screen → Screen | Long press | 200ms | Slide right (back) |
| Idle → Active | Any encoder action | 400ms | Fade in from idle |
| Active → Idle | Timeout | 800ms | Slow fade to idle |

### LVGL Implementation

```c
// Screen transitions use lv_scr_load_anim()
lv_scr_load_anim(target_screen, LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false);

// Slide transitions for sub-screens
lv_scr_load_anim(sub_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
lv_scr_load_anim(parent_screen, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, false);
```

---

## Radial List Scrolling

For lists displayed on the round screen (todos, presets, breathing exercises), the UI uses a **radial magnification** layout.

### How It Works

```
        ╭─────────────╮
       ╱   Task A (dim) ╲
      │                   │
      │  ▶ Task B (large) │  ← Active item, centered, enlarged
      │                   │
       ╲  Task C (dim)   ╱
        ╰─────────────╯

- Active item: centered, 1.2-1.5x scale, full opacity
- Adjacent items: smaller, faded, positioned above/below on arc
- Items beyond adjacent: hidden (off-screen)
```

### Behavior

| State | Display |
|---|---|
| 0 items | "No items" message centered |
| 1 item | Single item centered, full size |
| 2 items | Active centered, other faded below |
| 3+ items | Active centered, adjacent above/below faded, rest hidden |

### Edge Cases

| Case | Handling |
|---|---|
| Empty list | Show "No tasks yet" / "No presets" with subtle icon |
| Very long text | Wrap to 2 lines max, then ellipsis (see Text Wrapping) |
| Single item | Center it, no scrolling needed |
| Many items (10+) | Scroll cycles through all items smoothly |
| Rapid scrolling | Throttle updates to ~60fps, smooth animation |

### LVGL Implementation

The radial list uses **existing LVGL widgets styled creatively** (not a fully custom widget):

- **`lv_roller`** — For scrollable lists with center-item highlighting. Styled with custom fonts and colors to match the radial aesthetic.
- **`lv_list`** — For simpler lists with custom positioning and opacity gradients.
- Active item: full opacity, larger font
- Adjacent items: reduced opacity, smaller font
- Smooth scroll animation via `lv_anim_t`

```c
// Radial list using lv_roller with custom styling
lv_obj_t *roller = lv_roller_create(parent);
lv_roller_set_options(roller, "Task A\nTask B\nTask C\nTask D", LV_ROLLER_MODE_NORMAL);
lv_obj_set_style_text_font(roller, &font_16, 0);
lv_obj_set_style_text_color(roller, LV_COLOR_TEXT, 0);

// Animate opacity gradient for adjacent items
// (implemented via scroll event callback)
```

---

## Text Wrapping on Display

### Rules

1. **Maximum 2 lines** of text per item
2. After 2 lines, truncate with ellipsis (`...`)
3. No horizontal scrolling or marquee
4. Word-wrap at word boundaries (not mid-word)
5. If a single word is longer than 2 lines, truncate with ellipsis at the line limit

### LVGL Implementation

```c
// Set label to wrap text
lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);

// After wrapping, check if text exceeds 2 lines
// If so, manually truncate with ellipsis
lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);  // Truncates with "..."
```

**Note:** `LV_LABEL_LONG_DOT` truncates at the label boundary and adds "..." — this is the simplest approach. For 2-line limiting, we may need a custom check:

```c
// Custom 2-line truncation
int max_lines = 2;
int line_height = lv_font_get_line_height(font);
int max_height = line_height * max_lines;
lv_obj_set_height(label, max_height);
lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
```

### Font Sizing for Readability

On a 480×480 round display:
- **Task text:** 16px font — readable at arm's length
- **Labels:** 12px font — secondary information
- **Large digits (timer):** 36px font — glanceable from distance
- **Menu icons:** 48×48px pixel art — recognizable at a glance

---

## Notification Panel

### Design

The notification panel is a **toast overlay** that appears at the top of the current screen. It doesn't interrupt the current app.

```
    ╭──────────────────╮
    │  💧 Time for     │  ← Toast notification
    │     water!       │
    ╰──────────────────╯
    ╭──────────────────╮
    │                  │
    │   (current app   │
    │    content)      │
    │                  │
    ╰──────────────────╯
```

### Behavior

| Property | Value |
|---|---|
| Position | Top of screen (within safe zone) |
| Size | ~320×80px (fits within round display's top arc) |
| Duration | 4 seconds visible, then fade out (400ms) |
| Stacking | One at a time (queue system) |
| Dismiss | Auto-dismiss after timeout, or short press to dismiss early |
| Priority | Timer alerts > Sedentary > Water > Others |

### Notification Types

| Type | Icon | Sound | Trigger |
|---|---|---|---|
| Timer complete | ⏳ | Chime | Focus/break session ends |
| Break ready | ☕ | Gentle note | Break timer starts |
| Water reminder | 💧 | Water drop | Configurable interval |
| Sedentary alert | 🪑 | Gentle tap | Inactivity timeout |
| Task reminder | ☑️ | Soft pop | Optional daily reminder |
| Tamagotchi need | 🥚 | Cute sound | Pet needs attention |

### Queue System

```c
typedef struct {
    notification_type_t type;
    char message[64];
    uint32_t timestamp;
    bool dismissed;
} notification_t;

// Queue holds up to 10 pending notifications
notification_t notification_queue[MAX_NOTIFICATIONS];
int queue_head = 0;
int queue_count = 0;

// Show next notification in queue
void show_next_notification() {
    if (queue_count == 0) return;
    notification_t *n = &notification_queue[queue_head];
    display_toast(n->message, n->type);
    queue_head = (queue_head + 1) % MAX_NOTIFICATIONS;
    queue_count--;
}
```

---

## Idle Mode

### Activation

| Property | Value |
|---|---|
| Default timeout | 60 seconds (configurable: 15s, 30s, 60s, 2min, 5min, Never) |
| Trigger | No encoder activity for timeout duration |
| Entry animation | Slow fade (800ms) from active screen to idle |

### What's Displayed

**PENDING** — The user will provide concept designs. Initial plan:

- Full-screen pixel art background (from TF card)
- Optional clock overlay (small, unobtrusive)
- Optional session indicators (subtle, in corner)
- No interactive elements — purely display

### Wake

| Action | Result |
|---|---|
| Any encoder action (rotate, press) | Wake to the last active screen |
| Timer completing during idle | Show timer alert toast over idle screen |

### Transitions

```
Active Screen ──(timeout)──► Idle Screen
     ▲                            │
     │         (encoder action)   │
     └────────────────────────────┘
```

---

## Completed Tasks on Display

### Behavior

When a task is marked complete (via encoder press):

1. **Visual change:** Task text gets strikethrough + opacity reduces to 40%
2. **Position change:** Task animates to the bottom of the list
3. **Remaining tasks:** Shift up to fill the gap
4. **No immediate deletion:** Completed tasks remain visible in the list
5. **4 AM reset:** Completed tasks are archived (moved to web app archive, hidden on display)

### Animation

```
Before:                    After:
╭──────────────╮          ╭──────────────╮
│ ▶ Task B     │          │ ▶ Task C     │  ← Shifted up
│   Task C     │          │   Task D     │
│   Task D     │          │──────────────│
│──────────────│          │ ~~Task A~~   │  ← Struck through, faded
│ ~~Task A~~   │          ╰──────────────╯
╰──────────────╯
```

### Archive Logic

- **Trigger:** 4:00 AM daily (not midnight — users study through midnight)
- **Action:** All completed tasks → `archived_todos[]` in web app
- **Display:** Completed tasks hidden from display list
- **Web app:** Archived tasks visible in "Archive" section
- **User control:** Display can optionally show archived tasks if user enables via web app settings

---

## Screen Layout Examples

### Home Screen (Clock)

```
        ╭───────────────╮
       ╱                 ╲
      │    ┌─────────┐    │
      │    │  14:32   │    │  ← Large time display
      │    │  Thursday │    │  ← Day + date
      │    └─────────┘    │
      │                   │
      │  🌿 3 sessions    │  ← Subtle session count
      │  💧 5/8 glasses   │  ← Water progress
       ╲                 ╱
        ╰───────────────╯
```

### Timer Screen

```
        ╭───────────────╮
       ╱                 ╲
      │                   │
      │     ┌───────┐     │
      │     │ 25:00 │     │  ← Large countdown
      │     └───────┘     │
      │                   │
      │    ╭─────────╮    │
      │    │ ▓▓▓▓░░░ │    │  ← Progress ring
      │    ╰─────────╯    │
      │                   │
      │   FOCUS  ⏳       │  ← Phase + preset name
       ╲                 ╱
        ╰───────────────╯
```

### Todos Screen

```
        ╭───────────────╮
       ╱                 ╲
      │    My Tasks       │
      │                   │
      │  ╭─────────────╮  │
      │  │ ▶ Read       │  │  ← Active (large, centered)
      │  │   chapter 5  │  │
      │  ╰─────────────╯  │
      │                   │
      │    Practice math  │  ← Adjacent (smaller, faded)
      │                   │
      │   3/7 done        │  ← Counter
       ╲                 ╱
        ╰───────────────╯
```

### Water Tracker Screen

```
        ╭───────────────╮
       ╱                 ╲
      │                   │
      │    💧 Water       │
      │                   │
      │     ┌─────┐       │
      │     │  5  │       │  ← Current glasses
      │     │ / 8 │       │  ← Goal
      │     └─────┘       │
      │                   │
      │   ▓▓▓▓▓░░░░       │  ← Progress bar
      │                   │
      │  Press to log +1  │  ← Hint text
       ╲                 ╱
        ╰───────────────╯
```
