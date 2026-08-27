# Focus Friend — Feature Overview

Focus Friend is a desk companion system comprising an embedded LVGL display application and a responsive web companion, synchronised over WebSocket. Below are the eight core feature sections. Each section presents the LVGL and web app as distinct products with separate analyses, while demonstrating how they remain deeply intertwined through shared state and bidirectional communication.

---

## 1. Home Dashboard & Menu Navigation

### LVGL Application

The LVGL home screen serves as the primary landing interface on the 480×480 circular display.

- Displays the current time, total seed count, user level, water intake summary, and recent session summaries at a glance.
- A radial menu wheel is accessed via the rotary encoder, presenting nine navigation targets: Home, Tamagotchi, Breathing, Water, Sleep, Timer, Todos, Backgrounds, and Settings.
- The menu uses a scroll-and-press interaction model, with the encoder's rotary axis cycling through options and the press input confirming selection.
- The home screen acts as a persistent anchor, always returning the user to a familiar starting point after completing any task.

**Images:** `01-home.png`, `02-menu-home.png` through `11-menu-settings.png`

### Web Application

The web companion reinterprets the home experience through a browser-based dashboard optimised for both desktop and mobile viewports.

- A bottom-tab navigation bar replaces the radial menu, providing one-tap access to all feature sections.
- Summary cards surface key metrics — pending todos, water intake, breathing sessions, sleep data, and device connection status — without requiring navigation into individual apps.
- When the device is offline, a connection prompt appears, guiding the user to reconnect before attempting to interact with synchronised features.
- The web dashboard is read-heavy by design: it presents the most important information immediately, while deeper interaction occurs within individual feature tabs.

**Images:** `24-home-dark.png`, `38-mobile-home-light.png`, `50-mobile-home-dark.png`

### How They Connect

Both surfaces display the same shared state, but the LVGL interface prioritises real-time interaction through physical input, while the web app prioritises overview and accessibility across devices. The WebSocket connection ensures that actions taken on one platform — logging water, completing a breathing session, toggling a todo — are reflected on the other within moments.

---

## 2. Tamagotchi & Gamification System

### LVGL Application

The Tamagotchi is the gamification centrepiece of Focus Friend on the embedded display, spanning three distinct sub-screens.

- **Pet View:** Displays a growing plant visual with a level chip, giving the user an immediate visual indicator of their progression and investment in the system.
- **Goals Screen:** Presents a daily goals checklist — up to three selectable habits — with seed rewards for each completed goal. Toggling a goal awards +20 seeds; completing all three triggers a +30 bonus.
- **Streaks Screen:** Tracks five activity types — focus, water, breathing, sleep, and todos — with multiplier values ranging from 100% to 200% and milestone markers that reward sustained consistency.
- Seed earnings are context-sensitive: todos (+10), water (+5 per glass), breathing (+10 + 1 per cycle), focus sessions (+25), sleep (+5 to +50 tiered by duration), and a +30 bedtime bonus for logging sleep before a configured time.
- An XP-curve level system governs progression, ensuring early levels are reached quickly to build motivation while later levels require sustained engagement.

**Images:** `tama-01-pet.png`, `tama-02-goals.png` through `tama-05-goals-r3.png`, `tama-06-streaks.png` through `tama-08-streaks-r2.png`, `tama-09-goal-toggled.png`

### Web Application

The web Tamagotchi page provides a data-rich counterpart to the visual LVGL experience.

- A seeds overview panel shows total seeds, today's earnings, current level, and a progress bar toward the next level.
- Daily goals are displayed as toggles with the same reward structure as the device, allowing the user to manage goals from the browser.
- Streak multipliers and milestone markers are presented with greater numerical detail than the circular display permits, including historical streak data.
- The web app is the primary place where the user reviews their long-term gamification trajectory, while the LVGL display is where the moment-to-moment seed earning occurs.

**Images:** `25-tamagotchi-plant-dark.png`, `26-tamagotchi-goals-dark.png`, `27-tamagotchi-goals-full-dark.png`, `39-mobile-tamagotchi-plant-light.png`, `40-mobile-tamagotchi-goals-light.png`, `41-mobile-tamagotchi-goals-full-light.png`

### How They Connect

The Tamagotchi system is the most deeply intertwined feature. Every seed-earning action on the LVGL display is broadcast to the web companion via WebSocket, updating totals, streaks, and goal progress in real time. Conversely, toggling a daily goal on the web sends a message back to the device, updating the LVGL goals screen and awarding seeds accordingly. The plant's growth state, level, and streak multipliers are shared across both surfaces, ensuring a single unified gamification experience regardless of which interface the user is engaging with.

---

## 3. Guided Breathing

### LVGL Application

The breathing app on the embedded display delivers a focused, distraction-free guided breathing experience.

- Up to eight breathing exercises are available, each with a distinct rhythm and duration profile.
- A pulsing circle animation expands and contracts in sync with inhale and exhale instructions, providing clear visual guidance without requiring text-heavy prompts.
- Users select an exercise, complete a series of cycles, and earn seeds upon session completion — with a 30-minute cooldown enforced between sessions to prevent repetitive exploitation.
- The session store tracks completed cycles and manages cooldown timers, ensuring seed rewards are only awarded for genuine engagement.

**Images:** `19-breath-selection.png`, `20-breath-cycles-focused.png`, `21-breath-instruction.png`, `41-breath-cycles-editing.png`

### Web Application

The web breathing page serves as both a control surface and an analytics dashboard.

- Available exercises are listed with options to select or edit parameters, allowing the user to customise session duration and cycle count from the browser.
- Breathing analytics include favourite exercise, average session time, weekly session count, and a cycle bar chart with selectable 3-day, weekly, and monthly range views.
- Evaluation grades summarise the user's breathing habits over time, providing feedback that the LVGL display's limited screen area cannot accommodate.
- Selecting an exercise on the web triggers the corresponding animation on the physical device, bridging the two surfaces into a single interactive loop.

**Images:** `28-breathing-dark.png`, `42-mobile-breathing-light.png`, `53-mobile-breathing-dark.png`

### How They Connect

The breathing feature demonstrates asymmetric coupling: the LVGL display handles the real-time animated experience, while the web app handles configuration and retrospective analysis. Exercise selection flows from web to device; session completion flows from device to web. The cooldown timer and seed rewards are managed on the device but visible on both surfaces, ensuring the user always has an accurate picture of their breathing activity.

---

## 4. Water Tracking

### LVGL Application

The water tracker on the embedded display is designed for rapid, frictionless interaction.

- A simple glass counter with +/- buttons allows the user to log water intake with minimal distraction from their study flow.
- An editable daily goal sets the target, and a circular progress bar provides immediate visual feedback on how close the user is to completion.
- A celebration animation plays when the goal is reached, providing a small moment of positive reinforcement.
- Each glass logs +5 seeds, with a +20 bonus awarded automatically upon goal completion.

**Images:** `22-water-default.png`, `23-water-plus.png`, `24-water-goal.png`, `25-water-added.png`

### Web Application

The web water page extends the tracker with historical context and goal management.

- A glass counter with +/- buttons mirrors the LVGL interface, allowing the user to log water from their browser.
- A goal setter with a stepper control lets the user adjust their daily target without navigating into settings.
- A progress bar provides the same at-a-glance feedback as the device.
- Historical glasses-per-day charts with 3-day, weekly, and monthly range views allow the user to identify patterns in their hydration habits over time.

**Images:** `29-water-dark.png`, `30-water-full-dark.png`, `43-mobile-water-light.png`, `44-mobile-water-full-light.png`, `54-mobile-water-dark.png`

### How They Connect

Water tracking is fully bidirectional: logging a glass on either platform updates the other in real time. The celebration animation triggers on the LVGL display when the goal is reached, regardless of whether the final glass was logged on the device or the web. The web app's charting capabilities give the user a longer-term view of their habits, while the LVGL display keeps the interaction immediate and tactile.

---

## 5. Sleep Tracking

### LVGL Application

The sleep tracker on the embedded display operates across a carefully structured five-state flow.

- **Intro:** Presents the sleep feature and invites the user to begin logging.
- **Start:** The user confirms they are going to sleep, initiating the timer.
- **Active:** The device tracks elapsed time while the user sleeps.
- **Summary:** Upon waking, the user stops logging and receives a summary of their sleep duration with tiered seed rewards (+5 to +50) scaling based on how long they slept.
- **Weekly:** A seven-day rolling summary shows recent sleep patterns at a glance.
- A +30 bedtime bonus rewards the user for starting their sleep log before a configured bedtime, encouraging consistent sleep hygiene.
- There is no automatic sleep-cycle detection — sleep is entirely manually logged by the user.

**Images:** `26-sleep-intro.png`, `27-sleep-weekly.png`, `28-sleep-start.png`, `29-sleep-active.png`, `42-sleep-start-focused.png`

### Web Application

The web sleep page provides the analytical depth that the circular display cannot.

- A sleep log records each session with duration, bedtime, and wake time.
- Weekly averages for sleep duration, bedtime, and wake time are calculated automatically.
- A sleep window visualisation shows the consistency of the user's sleep schedule over time.
- A sleep evaluation section provides qualitative feedback on sleep quality and consistency, drawing on historical data that accumulates across weeks of use.

**Images:** `31-sleep-dark.png`, `32-sleep-full-dark.png`, `45-mobile-sleep-light.png`, `55-mobile-sleep-dark.png`

### How They Connect

Sleep logging is initiated and completed on the LVGL display — the user physically presses the encoder to start and stop tracking. The duration and seed rewards are calculated on the device and broadcast to the web companion, where the data is logged into the historical record. The web app's evaluation and visualisation capabilities transform raw sleep durations into actionable insights, closing the loop between momentary action and long-term habit awareness.

---

## 6. Timer & Pomodoro

### LVGL Application

The timer app on the embedded display is the primary focus session tool.

- A scrollable preset list holds up to 16 customisable focus sessions, each with a name, focus duration, break duration, and optional Pomodoro flag.
- A countdown display with a circular progress ring provides clear visual feedback during active sessions.
- The timer cycles through focus, break, and long-break phases automatically when Pomodoro mode is enabled.
- Completing a focus session earns 25 seeds and increments the focus streak, feeding directly into the Tamagotchi gamification system.
- Preset management — adding, editing, and selecting presets — is handled entirely on the device via the rotary encoder.

**Images:** `33-timer-presets.png`, `34-timer-preset2.png`

### Web Application

The web timer page functions as a management and analytics companion.

- Timer status is displayed in read-only mode — the user can see the current countdown, active phase, and session count, but cannot control the timer from the browser.
- Full CRUD management for presets is available: add, edit, delete, and select presets from the web interface.
- Pomodoro analytics include sessions completed today, total focus time, pomodoros this week, and break history.
- The web app is where the user reviews their focus productivity over days and weeks, while the LVGL display is where the actual focus sessions occur.

**Images:** `34-timer-dark.png`, `47-mobile-timer-light.png`, `57-mobile-timer-dark.png`

### How They Connect

The timer feature uses a hub-and-spoke model: the LVGL display is the active control point, while the web app is the passive monitor and configuration tool. Preset changes on the web sync to the device, ensuring the user can prepare sessions from their browser before sitting down to study. Session completions on the device flow to the web, where they accumulate into Pomodoro analytics. The 25-seed reward for each completed session is calculated on the device and broadcast to both the LVGL gamification screen and the web Tamagotchi page.

---

## 7. Todos

### LVGL Application

The todo list on the embedded display is optimised for quick task management during study sessions.

- Up to 32 items can be stored, each with a text label, done flag, priority level, and manual sort order.
- A scroll-and-snap interaction model allows the user to browse and toggle tasks efficiently with the rotary encoder.
- Checking off a task awards +10 seeds, providing immediate positive reinforcement.
- The todo store persists to the SD card, ensuring tasks survive power cycles and device restarts.

**Images:** `30-todos-populated.png`, `31-todos-toggled.png`, `32-todos-mixed.png`

### Web Application

The web todos page provides full CRUD capabilities that the circular display's limited input model cannot easily support.

- Tasks can be added, edited, deleted, and toggled from the browser, giving the user complete control over their task list.
- Colour-coded priorities provide visual hierarchy, and a completed tasks section keeps finished items accessible without cluttering the active list.
- Contextual info/insight icons surface patterns in the user's todo completion habits, offering feedback that goes beyond simple list management.
- The web app is where the user curates their task list; the LVGL display is where they interact with it during active study.

**Images:** `33-todos-dark.png`, `46-mobile-todos-light.png`, `56-mobile-todos-dark.png`

### How They Connect

Todo state is fully synchronised between device and web. Toggling a task on the LVGL display updates the web list and awards seeds; adding or editing a task on the web pushes the change to the device. This bidirectional sync means the user can prepare their task list on the laptop before a study session, then work through it on the physical device without interruption.

---

## 8. Backgrounds & Sedentary/Stretch Breaks

### LVGL Application

The backgrounds and sedentary features are device-native functions that prioritise the physical desk companion experience.

- **Backgrounds:** The idle screensaver displays rotating artwork when the device is not in active use, serving as an ambient decoration that keeps the companion present on the desk even during passive moments.
- **Sedentary/Stretch Breaks:** A background alert system prompts the user to take a movement or water break at configurable intervals. Acknowledging the break awards +10 seeds.
- The sedentary alert is designed to appear between app interactions, avoiding disruption during active focus sessions.
- Both features are implemented entirely on the LVGL side, with no standalone web equivalents.

**Images:** `40-backgrounds.png`, `39-sedentary.png`

### Web Application

The web companion provides limited but meaningful support for these device-centric features.

- A sedentary tab syncs break data from the device, recording when breaks were taken and how many seeds were earned.
- The web app does not independently trigger sedentary reminders — the alert logic runs exclusively on the embedded display.
- There is no web equivalent of the backgrounds/screensaver feature, as it serves a purely physical-desk purpose.
- The web sedentary tab functions as a passive data mirror, allowing the user to review their break history alongside other habit data.

**Images:** `35-sedentary-dark.png`, `48-mobile-sedentary-light.png`

### How They Connect

These two features represent the most asymmetric pairing in the system. The LVGL display is the sole active surface — it triggers alerts, displays artwork, and manages the underlying timers. The web app's role is limited to recording and displaying the data that flows from the device. This asymmetry reflects the physical nature of both features: screensavers only make sense on a physical desk display, and movement reminders are most effective when delivered by a device in the user's immediate environment.
