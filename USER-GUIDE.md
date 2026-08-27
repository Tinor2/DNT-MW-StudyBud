# Focus Friend — User Guide

Focus Friend is a desk companion that helps you manage study, stress, and daily habits. It has two parts: a physical device with a round display and rotary encoder, and a responsive web companion you open in a browser. Both stay in sync over your Wi-Fi network.

---

## The Physical Device

The device sits on your desk and is controlled with a single rotary encoder — a knob that turns and presses.

### Controls

| Action | How |
|--------|-----|
| **Scroll** | Turn the encoder left or right to move through lists and menus |
| **Select / Confirm** | Press the encoder in (short press) |
| **Open the menu** | Press and hold the encoder for about one second from any screen |
| **Go back to Home** | Open the menu, then scroll to Home and press |

When you press and hold, a coloured glow ring appears around the edge of the display and fills up as you hold. Once it is full, the menu opens. A small navigation bubble at the top of the screen shows "Menu" during this hold.

### Home Screen

The first thing you see when the device starts. It shows:

- The **current time** (large, centred)
- The **day of the week**
- How many **seeds you have earned today**
- Your current **level** and progress toward the next level
- A **hint** at the bottom — press the encoder to open the Tamagotchi screen

The home screen is purely informational. Pressing the encoder from home takes you to the Tamagotchi.

### The Menu

The menu is a vertical scrollable list of all apps. Scroll to highlight an app, then press to open it. The available apps, in order, are:

1. **Home** — clock and daily summary
2. **Tamagotchi** — plant pet, daily goals, and streaks
3. **Breathing** — guided breathing exercises
4. **Water** — water intake tracker
5. **Stretch Break** — movement break alerts and history
6. **Sleep** — manual sleep logger
7. **Timer** — Pomodoro and focus timer presets
8. **Todos** — task list
9. **Backgrounds** — idle screensaver artwork
10. **Settings** — brightness, volume, timeout, reading light

---

## Feature-by-Feature Walkthrough

### Tamagotchi

Your plant pet grows as you complete healthy habits. This screen has three views, cycled by pressing the encoder:

- **Pet View** — shows your plant and current level chip. The plant grows through stages as you earn seeds.
- **Goals** — up to three daily goals you can toggle on or off. Each completed goal awards seeds. Completing all three triggers a bonus.
- **Streaks** — shows your streak across five activities (focus, water, breathing, sleep, todos). Longer streaks give higher multipliers, up to 2×.

Seeds are the currency of the gamification system. You earn them by using every feature of Focus Friend.

### Breathing

Choose from guided breathing exercises. Each exercise has a different rhythm of inhale, hold, exhale, and optional second hold. When you select an exercise:

1. A pulsing circle animates on screen — it expands during inhale and contracts during exhale.
2. Text instructions appear (e.g., "Breathe in...").
3. Complete the required number of cycles to earn seeds.
4. A 30-minute cooldown prevents rapid re-earning.

### Water

A simple glass counter. Press the encoder to add or remove glasses. A progress bar shows how close you are to your daily goal. When you reach the goal, a celebration animation plays and you earn bonus seeds.

### Stretch Break

After a configurable interval (default 60 minutes), a popup alert appears on screen prompting you to take a break. You have two options:

- **Snooze** — dismiss the alert and postpone it
- **Take Break** — acknowledge the break, earn seeds, and navigate to the stretch break screen

You can take up to 5 stretch breaks per day, each earning seeds.

### Sleep

Sleep is manually logged in five stages:

1. **Intro** — explains the feature
2. **Start** — press to begin logging when you go to bed
3. **Active** — the device tracks elapsed time while you sleep
4. **Summary** — when you wake, press to stop logging. You see your sleep duration and earn tiered seeds (more hours = more seeds)
5. **Weekly** — a rolling seven-day summary of your sleep patterns

A bedtime bonus rewards you for starting your sleep log before a configured time.

### Timer

The timer screen has two parts:

- **Preset List** — scroll through saved focus session presets (e.g., "Pomodoro 25/5", "Deep Work 50/10"). Select one to start it.
- **Active Timer** — shows a countdown ring, current phase (focus, break, or long break), and elapsed time.

In Pomodoro mode, the timer automatically cycles through focus and break phases. Completing a focus session earns seeds and bumps your focus streak.

### Todos

A scrollable task list. Scroll to a task and press to toggle it done or undone. Completed tasks move to the bottom with a strikethrough. Each task completion earns seeds.

Tasks have priority levels indicated by a coloured dot:
- Red — high priority
- Orange — medium priority
- Green — low priority
- Grey — no priority

### Backgrounds

The idle screensaver displays rotating artwork when the device is not in active use. This keeps Focus Friend as a pleasant presence on your desk even when you are not interacting with it.

### Settings

Adjust device settings by scrolling to an option and pressing to change it:

- **Brightness** — screen brightness level
- **Volume** — alert and notification volume
- **Idle Timeout** — how long before the screensaver activates
- **Reading Light** — turns the screen into a warm amber light for reading or relaxing at night

---

## The Web Companion

Open the web companion in any browser on the same Wi-Fi network as the device. It connects automatically and mirrors the device state in real time.

### Connecting

1. Open the web app in your browser.
2. The header shows a connection status indicator: green (connected), yellow (connecting), or red (disconnected).
3. Click the status dot to connect or disconnect manually.
4. When connected, changes on the device appear on the web, and vice versa.

If the device is not on the network, the web app still works in demo mode with sample data.

### Tab Navigation

The bottom tab bar provides access to every feature:

| Tab | What It Shows |
|-----|---------------|
| **Home** | Dashboard with summary cards for todos, water, breathing, sleep, and device status |
| **Tamagotchi** | Seeds overview, daily goals checklist, streaks with multipliers, and level progress |
| **Breathing** | Exercise list, breathing analytics (favourite exercise, avg time, weekly count), and cycle bar charts |
| **Water** | Glass counter, daily goal setter, progress bar, and historical glasses-per-day chart |
| **Sleep** | Sleep log with weekly averages, bedtime/wake averages, and sleep evaluation |
| **Todos** | Full task list with add, edit, delete, and toggle. Colour-coded priorities and a completed section |
| **Timer** | Timer status monitor, preset list with add/delete, and Pomodoro analytics |
| **Stretch Break** | Break history, daily cap progress, and health insight |
| **Settings** | Brightness, volume, idle timeout, reading light, theme toggle, and state export |

### Header Controls

- **Connection status dot** — click to connect/disconnect
- **Status badge** — shows current connection state text
- **Event Log button** — opens a panel showing all WebSocket messages (useful for debugging)
- **Theme toggle** — switches between light and dark mode

### Web-Specific Features

Some features are richer on the web than on the device:

- **Charts and analytics** — breathing cycles, water intake, sleep duration, and focus sessions are all visualised with bar charts and range selectors (3-day, weekly, monthly).
- **Evaluations** — each feature has an analysis card with a verdict (good / needs work / poor) and health-oriented advice.
- **Info icons** — small icons on analysis cards reveal contextual tips about the mental health benefits of each habit.
- **Todo management** — the web app supports full CRUD (create, read, update, delete) for tasks, while the device only supports toggling.
- **Preset management** — timer presets and breathing exercises can be added, edited, and deleted from the web.

### URL Parameters

| Parameter | Effect |
|-----------|--------|
| `?debug` | Logs all WebSocket messages to the browser console |
| `?demo` | Auto-connects to the device on page load |
| `?reset` | Clears all demo data and reloads with fresh state |

---

## How the Two Stay in Sync

The device and web app communicate over WebSocket through your local Wi-Fi network. Every action on one side is broadcast to the other:

- **Device to web** — completing a breathing session, logging sleep, checking a todo, earning seeds, timer state changes
- **Web to device** — toggling a todo, adding water, selecting a breathing exercise or timer preset, adjusting settings

The web app polls for a full sync every 10 seconds when connected. It also reconnects automatically with exponential backoff if the connection drops.

---

## The Seeds Economy

Seeds are Focus Friend's gamification currency. Every healthy action earns seeds, which grow your plant and level up your account.

| Action | Seeds Earned |
|--------|-------------|
| Check off a todo | +10 |
| Log a glass of water | +5 |
| Reach your water goal | +20 bonus |
| Complete a breathing session | +10 + 1 per cycle |
| Complete a focus session | +25 |
| Track sleep (tiered by duration) | +5 to +50 |
| Start sleep before your bedtime | +30 bonus |
| Complete a daily goal | +20 each |
| Complete all daily goals | +30 bonus |
| Take a stretch break | +10 |

Levels follow an XP curve — early levels are quick to reach, while later levels require sustained engagement. Streak multipliers (up to 2×) reward consistency across days.
