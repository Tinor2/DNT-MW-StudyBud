# StudyBud Version Log

Automated version log tracking the evolution of the StudyBud LVGL embedded app and its companion Svelte web dashboard.

Screenshots captured via headless SDL simulator with automated navigation scripts and Chrome headless for web screenshots.

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

**Menu:** Home · Timer · Todos · Water · Breathing · Settings · Backgrounds

| Home | Menu | Backgrounds |
|------|------|-------------|
| ![Home](04-idle-background-2026-07-23/lvgl-home-2026-07-23.png) | ![Menu](04-idle-background-2026-07-23/lvgl-menu-2026-07-23.png) | ![Backgrounds](04-idle-background-2026-07-23/lvgl-backgrounds-2026-07-23.png) |

---

## 05 — Sleep Tracker
**Date:** 2026-07-25 | **Commit:** `b23bf63`

Added Sleep tracking screen with sleep duration and quality logging.

**Menu:** Home · Timer · Todos · Water · Breathing · Settings · Backgrounds · Sleep

| Home | Menu | Sleep |
|------|------|-------|
| ![Home](05-sleep-tracker-2026-07-25/lvgl-home-2026-07-25.png) | ![Menu](05-sleep-tracker-2026-07-25/lvgl-menu-2026-07-25.png) | ![Sleep](05-sleep-tracker-2026-07-25/lvgl-sleep-2026-07-25.png) |

---

## 06 — New Menu Logos
**Date:** 2026-07-26 | **Commit:** `9dbab40`

Added custom SVG-based logos to each menu item for a polished visual identity.

**Menu:** Home · Timer · Todos · Water · Breathing · Settings · Backgrounds · Sleep

| Home | Menu with Logos |
|------|-----------------|
| ![Home](06-new-logos-2026-07-26/lvgl-home-2026-07-26.png) | ![Menu](06-new-logos-2026-07-26/lvgl-menu-with-logos-2026-07-26.png) |

---

## 07 — Pomodoro Timer App
**Date:** 2026-07-26 | **Commit:** `5bf5a79`

Added full timer screen with Pomodoro presets (25/5, 50/10, custom) and countdown display.

**Menu:** Home · Timer · Todos · Water · Breathing · Settings · Backgrounds · Sleep

| Home | Menu | Timer Presets |
|------|------|---------------|
| ![Home](07-timer-app-2026-07-26/lvgl-home-2026-07-26.png) | ![Menu](07-timer-app-2026-07-26/lvgl-menu-2026-07-26.png) | ![Timer Presets](07-timer-app-2026-07-26/lvgl-timer-presets-2026-07-26.png) |

---

## 08 — Svelte Web Dashboard (Basic)
**Date:** 2026-07-23 | **Commit:** `f4cb574`

First version of the companion Svelte web app for remote monitoring of the LVGL device via WiFi/Bluetooth.

| Svelte Basic |
|--------------|
| ![Svelte Basic](08-svelte-started-2026-07-23/svelte-basic-2026-07-23.png) |

---

## 09 — Svelte Web Dashboard (Polished)
**Date:** 2026-07-30 | **Commit:** `48e7509`

Refined Svelte dashboard with shared app_state, live display sync, and improved networking layers.

| Svelte Polished |
|-----------------|
| ![Svelte Polished](09-svelte-polished-2026-07-30/svelte-polished-2026-07-30.png) |

---

## 10 — Latest Build (Current)
**Date:** 2026-08-02 | **Branch:** `fix_timer_LVLGL`

All features combined: Tamagotchi pet, Stretch Break, Timer, Todos, Breathing, Water, Sleep, Backgrounds, and Settings. 10-item menu with full navigation.

**Menu:** Home · Tamagotchi · Breathing · Water · Stretch Break · Sleep · Timer · Todos · Backgrounds · Settings

| Home | Menu | Tamagotchi | Breathing | Water |
|------|------|------------|-----------|-------|
| ![Home](10-latest-final-2026-08-02/lvgl-home-2026-08-02.png) | ![Menu](10-latest-final-2026-08-02/lvgl-menu-2026-08-02.png) | ![Tamagotchi](10-latest-final-2026-08-02/lvgl-tamagotchi-2026-08-02.png) | ![Breathing](10-latest-final-2026-08-02/lvgl-breathing-2026-08-02.png) | ![Water](10-latest-final-2026-08-02/lvgl-water-2026-08-02.png) |

| Stretch Break | Sleep | Timer | Todos | Backgrounds |
|---------------|-------|-------|-------|-------------|
| ![Stretch](10-latest-final-2026-08-02/lvgl-stretch-break-2026-08-02.png) | ![Sleep](10-latest-final-2026-08-02/lvgl-sleep-2026-08-02.png) | ![Timer](10-latest-final-2026-08-02/lvgl-timer-2026-08-02.png) | ![Todos](10-latest-final-2026-08-02/lvgl-todos-2026-08-02.png) | ![Backgrounds](10-latest-final-2026-08-02/lvgl-backgrounds-2026-08-02.png) |
