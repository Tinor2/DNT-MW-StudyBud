# Breathing Tracker — Implementation Plan

## Overview

Implement a 4-state breathing exercise screen (Selection → Instruction → Active Loop → Completion) following the existing codebase patterns. All screen code is shared between the SDL2 simulator and ESP32 with zero platform-specific `#ifdef`.

## Decisions

- **Session persistence**: In-memory only (`static int`), no NVS/file
- **Font**: Montserrat 20 (not 28) for cycles button
- **Platform**: Both simulator + ESP32
- **Spec source**: `docs/TEMP/Breathing-App-Roadmap.md`

## Files to Create

### 1. `main/display/utils/session_store.h`
Header for in-memory session persistence API.

### 2. `main/display/utils/session_store.c`
Pure in-memory counter (`static int breath_count`). No `#ifdef`, no platform code. Identical behavior everywhere.

### 3. `main/display/screens/screen_breathing.h`
Function declarations matching the pattern of `screen_todos.h`.

### 4. `main/display/screens/screen_breathing.c`
All 4 states in one file:

| State | LVGL Objects | Encoder Behavior | Animations |
|-------|-------------|-----------------|------------|
| **A: Selection** | Title label, BEGIN button (120x120, circle, PRIMARY), Cycles button (70x70, circle, SECONDARY) | Rotate: focus between buttons. Press BEGIN → State B. Press Cycles → edit mode | Teeter-totter: `transform_width/height` +15 on FOCUSED |
| **B: Instruction** | 4 centered text labels | Press → State C | Fade out State A, fade in text |
| **C: Active Loop** | Badge (100→260px circle), lbl_badge_text ("IN"/"OUT"), lbl_counter | Press anytime → abort to State D | Inhale 4s expand, exhale 2.5s contract, counter++ |
| **D: Completion** | Result label, Home/Restart buttons | Rotate: toggle focus. Press: select | Clean layout |

## Files to Modify

### 5. `main/display/ui_manager.c`
- Add `#include "screens/screen_breathing.h"`
- Register `screens[SCREEN_BREATHING] = screen_breathing_create()`
- Register `screen_event_handlers[SCREEN_BREATHING] = screen_breathing_encoder_event`

### 6. `main/CMakeLists.txt`
Add `screen_breathing.c` and `session_store.c` to SRCS.

### 7. `simulator/CMakeLists.txt`
Add `screen_breathing.c` and `session_store.c` to `studybud_screens`.

## Implementation Order

1. Create `utils/session_store.h` + `utils/session_store.c`
2. Create `screens/screen_breathing.h`
3. Create `screens/screen_breathing.c` (all 4 states)
4. Wire into `ui_manager.c`
5. Update both `CMakeLists.txt` files
6. Build and test in simulator
