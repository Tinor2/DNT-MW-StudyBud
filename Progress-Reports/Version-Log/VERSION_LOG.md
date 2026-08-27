# StudyBud Version Log

Automated version log tracking the evolution of the StudyBud LVGL embedded app and its companion Svelte web dashboard.

Screenshots captured via headless SDL simulator with automated navigation scripts and Puppeteer (Chrome headless) for web screenshots in light mode.

---

## 02 — Todo List + Periwinkle Theme
**Date:** 2026-07-18 | **Commit:** `54aa33c`

Initial functional build with todo list screen, radial scroll menu, long-press navigation, and periwinkle theme.

**Menu:** Home · Timer · Todos · Water · Breathing · Settings

| Home | Menu | Todos |
|------|------|-------|
| ![Home](02-todo-periwinkle-2026-07-18/lvgl-home-2026-07-18.png) | ![Menu](02-todo-periwinkle-2026-07-18/lvgl-menu-2026-07-18.png) | ![Todos](02-todo-periwinkle-2026-07-18/lvgl-todos-2026-07-18.png) |

---

## 03 — Breathing Exercise App
**Date:** 2026-07-21 | **Commit:** `e27ac4c`

Added breathing exercise screen with guided inhale/exhale animation.

**Menu:** Home · Timer · Todos · Water · Breathing · Settings

| Home | Menu | Breathing |
|------|------|-----------|
| ![Home](03-breathing-app-2026-07-21/lvgl-home-2026-07-21.png) | ![Menu](03-breathing-app-2026-07-21/lvgl-menu-2026-07-21.png) | ![Breathing](03-breathing-app-2026-07-21/lvgl-breathing-2026-07-21.png) |

---

## 04 — Idle Background Animation
**Date:** 2026-07-23 | **Commit:** `093e68f`

Added Backgrounds menu item and idle background selection screen with animated tree theme.

**Menu:** Home · Timer · Todos · Water · Breathing · Backgrounds · Settings

| Home | Menu | Backgrounds |
|------|------|-------------|
| ![Home](04-idle-background-2026-07-23/lvgl-home-2026-07-23.png) | ![Menu](04-idle-background-2026-07-23/lvgl-menu-2026-07-23.png) | ![Backgrounds](04-idle-background-2026-07-23/lvgl-backgrounds-2026-07-23.png) |

---

## 05 — Sleep Tracker
**Date:** 2026-07-25 | **Commit:** `b23bf63`

Added Sleep tracking screen with sleep duration and quality logging.

**Menu:** Home · Timer · Todos · Water · Breathing · Backgrounds · Sleep · Settings

| Home | Menu | Sleep |
|------|------|-------|
| ![Home](05-sleep-tracker-2026-07-25/lvgl-home-2026-07-25.png) | ![Menu](05-sleep-tracker-2026-07-25/lvgl-menu-2026-07-25.png) | ![Sleep](05-sleep-tracker-2026-07-25/lvgl-sleep-2026-07-25.png) |

---

## 06 — New Menu Logos
**Date:** 2026-07-26 | **Commit:** `9dbab40`

Added custom SVG-based logos to each menu item for a polished visual identity.

**Menu:** Home · Timer · Todos · Water · Breathing · Backgrounds · Sleep · Settings

| Home | Menu with Logos |
|------|-----------------|
| ![Home](06-new-logos-2026-07-26/lvgl-home-2026-07-26.png) | ![Menu](06-new-logos-2026-07-26/lvgl-menu-with-logos-2026-07-26.png) |

---

## 07 — Pomodoro Timer App (v1)
**Date:** 2026-07-26 | **Commit:** `5bf5a79`

First timer implementation: horizontal carousel with a default "Pomodoro" preset (25/5/15), action mode (Play/Edit), running countdown screen, and preset editor.

**Menu:** Home · Timer · Todos · Water · Breathing · Backgrounds · Sleep · Settings

| Preset Carousel | Timer Running | Timer Edit |
|-----------------|---------------|------------|
| ![Presets](07-timer-app-2026-07-26/lvgl-timer-presets-v1-2026-07-26.png) | ![Running](07-timer-app-2026-07-26/lvgl-timer-running-v1-2026-07-26.png) | ![Edit](07-timer-app-2026-07-26/lvgl-timer-edit-v1-2026-07-26.png) |

---

## 07b — Timer Save Fix
**Date:** 2026-07-27 | **Commit:** `0c3c6a0`

Fixed bug where creating a new timer preset overwrote the first entry instead of adding. Updated timer_store to use active_preset_id for edit detection.

| Preset Carousel | Timer Running | Timer Edit |
|-----------------|---------------|------------|
| ![Presets](07b-timer-save-fix-2026-07-27/lvgl-timer-presets-v2-2026-07-27.png) | ![Running](07b-timer-save-fix-2026-07-27/lvgl-timer-running-v2-2026-07-27.png) | ![Edit](07b-timer-save-fix-2026-07-27/lvgl-timer-edit-v2-2026-07-27.png) |

---

## 08 — Svelte Web Dashboard (Basic)
**Date:** 2026-07-23 | **Commit:** `f4cb574`

First version of the companion Svelte web app. Raw WebSocket debug console for communicating with the ESP32 device — connection panel, message log, and send interface.

| WebSocket Debug Console |
|-------------------------|
| ![Svelte Basic](08-svelte-basic-2026-07-23/svelte-basic-ws-console-2026-07-23.png) |

---

## 09 — Svelte Web Dashboard (Polished)
**Date:** 2026-07-30 | **Commit:** `48e7509`

Full tabbed dashboard with Home overview, Timer presets, Todos, Breathing, Sleep, and Settings. Event log overlay, notification toasts, and light/dark theme system.

| Home Dashboard | Timer Presets | Todos |
|----------------|---------------|-------|
| ![Home](09-svelte-polished-2026-07-30/svelte-home-dashboard-2026-07-30.png) | ![Timer](09-svelte-polished-2026-07-30/svelte-timer-presets-2026-07-30.png) | ![Todos](09-svelte-polished-2026-07-30/svelte-todos-2026-07-30.png) |

| Breathing | Sleep | Settings |
|-----------|-------|----------|
| ![Breathing](09-svelte-polished-2026-07-30/svelte-breathing-2026-07-30.png) | ![Sleep](09-svelte-polished-2026-07-30/svelte-sleep-2026-07-30.png) | ![Settings](09-svelte-polished-2026-07-30/svelte-settings-2026-07-30.png) |

---

## 09b — Svelte Web Dashboard (Latest)
**Date:** 2026-08-01 | **Commit:** `4fdd1fe`

Full-featured dashboard with 9 tabs, pixel-art logos, Tamagotchi plant + goals, bar charts, analytics, health insights, reading light, and streak system.

| Home | Tamagotchi Plant | Tamagotchi Goals |
|------|------------------|------------------|
| ![Home](09b-svelte-latest-2026-08-01/svelte-home-dashboard-2026-08-01.png) | ![Tamagotchi](09b-svelte-latest-2026-08-01/svelte-tamagotchi-plant-2026-08-01.png) | ![Goals](09b-svelte-latest-2026-08-01/svelte-tamagotchi-goals-2026-08-01.png) |

| Breathing | Water | Sleep |
|-----------|-------|-------|
| ![Breathing](09b-svelte-latest-2026-08-01/svelte-breathing-2026-08-01.png) | ![Water](09b-svelte-latest-2026-08-01/svelte-water-2026-08-01.png) | ![Sleep](09b-svelte-latest-2026-08-01/svelte-sleep-2026-08-01.png) |

| Todos | Timer | Stretch Break | Settings |
|-------|-------|---------------|----------|
| ![Todos](09b-svelte-latest-2026-08-01/svelte-todos-2026-08-01.png) | ![Timer](09b-svelte-latest-2026-08-01/svelte-timer-presets-2026-08-01.png) | ![Stretch](09b-svelte-latest-2026-08-01/svelte-stretch-break-2026-08-01.png) | ![Settings](09b-svelte-latest-2026-08-01/svelte-settings-2026-08-01.png) |

---

## 10 — Latest Build (Current)
**Date:** 2026-08-02 | **Branch:** `fix_timer_LVLGL`

All features combined: Tamagotchi pet, Stretch Break, Timer (presets + edit), Todos, Breathing, Water, Sleep, Backgrounds, and Settings. 10-item menu with full navigation.

**Menu:** Home · Tamagotchi · Breathing · Water · Stretch Break · Sleep · Timer · Todos · Backgrounds · Settings

| Home | Menu | Tamagotchi | Breathing | Water |
|------|------|------------|-----------|-------|
| ![Home](10-latest-final-2026-08-02/lvgl-home-2026-08-02.png) | ![Menu](10-latest-final-2026-08-02/lvgl-menu-2026-08-02.png) | ![Tamagotchi](10-latest-final-2026-08-02/lvgl-tamagotchi-2026-08-02.png) | ![Breathing](10-latest-final-2026-08-02/lvgl-breathing-2026-08-02.png) | ![Water](10-latest-final-2026-08-02/lvgl-water-2026-08-02.png) |

| Stretch Break | Sleep | Timer | Todos | Backgrounds |
|---------------|-------|-------|-------|-------------|
| ![Stretch](10-latest-final-2026-08-02/lvgl-stretch-break-2026-08-02.png) | ![Sleep](10-latest-final-2026-08-02/lvgl-sleep-2026-08-02.png) | ![Timer](10-latest-final-2026-08-02/lvgl-timer-2026-08-02.png) | ![Todos](10-latest-final-2026-08-02/lvgl-todos-2026-08-02.png) | ![Backgrounds](10-latest-final-2026-08-02/lvgl-backgrounds-2026-08-02.png) |

### Timer Sub-Screens (Latest)

| Timer Presets | Timer Edit |
|---------------|------------|
| ![Timer Presets](10-latest-final-2026-08-02/lvgl-timer-presets-latest-2026-08-02.png) | ![Timer Edit](10-latest-final-2026-08-02/lvgl-timer-edit-latest-2026-08-02.png) |
