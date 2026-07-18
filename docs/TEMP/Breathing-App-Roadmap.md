# Breathing App — Implementation Roadmap

## Goal

Build a 4-state breathing exercise screen that works identically on the SDL2 simulator and the ESP32 physical display. The screen guides the user through cycle selection, instruction display, an animated IN/OUT breathing loop, and a completion summary.

---

## File Structure

```
main/display/
├── screens/
│   ├── screen_breathing.c    ← All 4 states in one file
│   └── screen_breathing.h
└── utils/
    ├── session_store.c       ← File-based session persistence
    └── session_store.h
```

Both `.c` files are compiled for **both** targets (ESP32 + simulator) via their respective CMakeLists.txt. No code is copied — the same source is referenced from both build systems.

---

## State Machine

```
┌─────────────────┐
│  STATE_A:       │  BEGIN button + Cycles selector (1-40)
│  SELECTION      │  Encoder rotates focus between buttons
│                 │  Press on BEGIN → State B
│                 │  Press on Cycles → edit mode (rotate to change)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  STATE_B:       │  "It's your Nth time today!"
│  INSTRUCTION    │  "Take a moment for yourself."
│                 │  "Press encoder dial to begin..."
│                 │  Press encoder → State C
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  STATE_C:       │  Badge expands "IN" (4s) → contracts "OUT" (2.5s)
│  ACTIVE LOOP    │  Counter increments on each exhale
│                 │  Loops until counter == cycle limit
│                 │  Press encoder at any time → State D (abort)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│  STATE_D:       │  "You completed N cycles!"
│  COMPLETION     │  [Home]  /  [Restart Exercise]
│                 │  Encoder rotates focus, press selects
└─────────────────┘
```

---

## Layout Specifications (480×480 Round Display)

### State A: Selection

| Element | Size | Position | Style |
|---|---|---|---|
| `btn_begin` | 120×120 (base) | Center, Y-40 | Radius=CIRCLE, bg=LV_COLOR_PRIMARY, text="BEGIN", font=Montserrat_20 |
| `btn_cycles` | 70×70 (base) | Center, Y+60 | Radius=CIRCLE, bg=LV_COLOR_SECONDARY, text="3", font=Montserrat_28 |
| Title label | auto | Top center, Y+30 | "Breathing", font=Montserrat_20, color=LV_COLOR_TEXT |

**Teeter-totter focus animation:**
- On `LV_EVENT_FOCUSED`: Grow focused button by +15px width/height, shrink unfocused back to base size
- Use `lv_obj_set_style_transform_width(obj, 15, 0)` and `lv_obj_set_style_transform_height(obj, 15, 0)` for the focused object, set to `0` for defocused
- Transition time: 200ms (matches design system easing)

**Cycles edit mode:**
- When `btn_cycles` is focused and pressed, set a `bool edit_mode` flag
- In edit mode, encoder rotation increments/decrements `cycle_count` (clamped 1–40)
- Visual: add `LV_STATE_CHECKED` state to `btn_cycles` to show a border highlight
- Second press exits edit mode

### State B: Instruction

All State A elements fade out (opacity 0 over 300ms). Text elements fade in:

| Element | Position | Style |
|---|---|---|
| "It's your Nth time today!" | Center, Y-60 | Font=Montserrat_16, color=LV_COLOR_TEXT |
| "Take a moment for yourself." | Center, Y-10 | Font=Montserrat_14, color=LV_COLOR_TEXT_SECONDARY |
| "Follow the onscreen prompt." | Center, Y+30 | Font=Montserrat_14, color=LV_COLOR_TEXT_MUTED |
| "Press encoder dial to begin..." | Center, Y+70 | Font=Montserrat_14, color=LV_COLOR_PRIMARY |

### State C: Breathing Loop

| Element | Size | Position | Style |
|---|---|---|---|
| `lbl_counter` | auto (text) | Center (0,0) | Font=Montserrat_48, color=LV_COLOR_TEXT_MUTED, opacity=LV_OPA_30 |
| `badge` | 100×100 (base) → 260×260 (expanded) | Center (0,0) | Radius=CIRCLE, bg=LV_COLOR_PRIMARY |
| `lbl_badge_text` | auto | Inside badge, center | "IN" or "OUT", Font=Montserrat_20, color=white |

**Inhale animation (4000ms):**
```
badge width:  100 → 260  (lv_anim_t, lv_obj_set_width)
badge height: 100 → 260  (lv_anim_t, lv_obj_set_height)
badge bg:     LV_COLOR_PRIMARY → LV_COLOR_INFO  (color interpolation)
```
- 260px covers the Montserrat 48 counter (~30px wide, ~50px tall) with margin
- Animation easing: `lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out)`

**Exhale animation (2500ms):**
```
badge width:  260 → 100
badge height: 260 → 100
badge bg:     LV_COLOR_INFO → LV_COLOR_PRIMARY
lbl_counter:  increment by 1
lbl_badge_text: "OUT"
```

**Timing constants (hardcoded for now):**
```c
#define INHALE_TIME_MS   4000
#define EXHALE_TIME_MS   2500
```
These will be made configurable via the Svelte web app in a future phase.

### State D: Completion

| Element | Position | Style |
|---|---|---|
| "You completed N cycles!" | Center, Y-50 | Font=Montserrat_16, color=LV_COLOR_TEXT |
| "[Home]" button | Center, Y+20 | Font=Montserrat_16, bg=LV_COLOR_PRIMARY_LIGHT when focused |
| "[Restart]" button | Center, Y+70 | Font=Montserrat_16, bg=LV_COLOR_PRIMARY_LIGHT when focused |

---

## Session Persistence

**API:**
```c
void session_store_init(void);
int  session_store_get_breath_count(void);
void session_store_increment_breath_count(void);
```

**File format:** `~/.studybud_sessions`
```
2026-07-18
3
```
Line 1 = date of last session, line 2 = count. If date != today, reset count to 0 on read.

**ESP32 adaptation:** Replace file I/O with NVS (Non-Volatile Storage) using the same API. The header abstracts the storage backend.

---

## Simulator ↔ ESP32 Portability

| Concern | Strategy |
|---|---|
| **Source code** | Same `.c` files compiled for both targets. No `#ifdef` in screen code. |
| **Logging** | `esp_log.h` mock in simulator → `printf()`. Real ESP-IDF `esp_log.h` on hardware. Include path priority ensures correct header is picked up. |
| **Tick source** | Simulator: `SDL_GetTicks()` via `LV_TICK_CUSTOM`. ESP32: `esp_timer` at 2ms. Both feed `lv_tick_inc()` — LVGL animations run at same speed. |
| **Display flush** | Simulator: `SDL_UpdateTexture()`. ESP32: `esp_lcd_panel_draw_bitmap()`. Screen code never calls flush directly — LVGL handles it via `flush_cb`. |
| **Input** | Simulator: keyboard → `enc_diff`/`state`. ESP32: GPIO → `enc_diff`/`state`. Same `lv_indev_data_t` struct, same `ui_manager_encoder_event()` entry point. |
| **File I/O** | `session_store` uses `#ifdef __APPLE__` / `#ifdef __linux__` for simulator paths, `#ifdef ESP_PLATFORM` for NVS. Separate implementation file keeps screen code clean. |
| **Round display** | Simulator applies circle mask in `sdl_flush_cb`. ESP32 has physical round LCD. Screen code assumes 480×480 square canvas — circle mask is applied at the display layer, not the UI layer. |

**Key principle:** Screen code (`screen_breathing.c`) contains **zero platform-specific code**. All platform differences are handled by:
1. `esp_log.h` mock (simulator) vs real header (ESP32)
2. `session_store.c` (different backend implementations)
3. Display driver (`SDL_Driver.c` vs `LVGL_Driver.c`)
4. Input driver (keyboard vs GPIO encoder)

---

## Animation Fidelity Checklist

| Spec Requirement | Implementation | Verification |
|---|---|---|
| BEGIN button expands, cycles shrinks (teeter-totter) | `transform_width/height` on FOCUSED/DEFOCUSED events | Visually confirm in simulator: rotate encoder between buttons |
| Cycles button enters edit mode on press | `bool edit_mode` flag, rotation changes `cycle_count` | Press on cycles, rotate, verify number changes 1–40 |
| Badge expands to cover counter completely | 260×260 covers Montserrat 48 text at center | Confirm counter is invisible when badge is expanded |
| Inhale takes exactly 4 seconds | `lv_anim_set_time(&a, 4000)` | Time with stopwatch in simulator |
| Exhale takes exactly 2.5 seconds | `lv_anim_set_time(&a, 2500)` | Time with stopwatch in simulator |
| Counter increments on exhale contraction | Increment in exhale animation `ready_cb` | Verify count increases after each OUT phase |
| Gradient transitions on badge | Color animation via custom exec callback | Visually confirm smooth color shift |
| Completion shows cycle count | `lv_label_set_text_fmt(lbl, "You completed %d cycles!", count)` | Complete 3 cycles, verify "3" displayed |
| Abort via encoder press during loop | Check `LV_INDEV_STATE_PR` in active state → jump to State D | Press Enter during IN/OUT, verify completion screen |

---

## Implementation Order

1. Create `session_store.h/.c` — standalone utility, no LVGL dependencies
2. Create `screen_breathing.h` — function declarations
3. Create `screen_breathing.c` — implement states sequentially:
   - Step 1: State A (selection) with teeter-totter focus
   - Step 2: State B (instruction) with text fade-in
   - Step 3: State C (active loop) with IN/OUT animations
   - Step 4: State D (completion) with Home/Restart
4. Wire into `ui_manager.c` — register screen + handler
5. Add to `screen_menu.c` menu items (verify target = `SCREEN_BREATHING`)
6. Update ESP32 `main/CMakeLists.txt` — add `.c` files to SRCS
7. Update simulator `simulator/CMakeLists.txt` — add `.c` files to `studybud_screens`
8. Build simulator, test all 4 states
9. Verify animations match timing specs

---

## Dependencies

| Dependency | Version | Notes |
|---|---|---|
| LVGL | 8.2.0 | Already in `simulator/lvgl/` and via ESP-IDF component |
| SDL2 | Any | Simulator only, via `brew install sdl2` |
| Montserrat fonts | 14, 16, 20, 28, 48 | Already enabled in `lv_conf.h` |
| `lv_anim` API | Built into LVGL 8.2 | No extra dependencies |
| `lv_checkbox` | Built into LVGL 8.2 | `LV_USE_CHECKBOX 1` in `lv_conf.h` |
