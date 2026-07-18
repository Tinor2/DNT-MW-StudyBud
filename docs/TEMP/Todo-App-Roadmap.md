# To-Do List App — Implementation Roadmap

## Goal

Build a scrollable task list screen with focus magnification, priority color tags, checkbox completion with strikethrough animation, and a 500ms hold-then-shuffle behavior. The screen uses hardcoded example items for now and will receive live data from the Svelte web app via WebSocket in a future phase.

---

## File Structure

```
main/display/screens/
├── screen_todos.c    ← List UI + completion logic
└── screen_todos.h
```

Single `.c` file compiled for both targets (ESP32 + simulator). No platform-specific code in the screen.

---

## Screen Layout (480×480 Round Display)

```
            /======================\
           |           ∧          |  ← Up arrow (hidden at top)
           |  O  First task...  (R)|
           | (O) Active Task... (O)|  ← Focused: magnified, centered
           |     Extended detail   |
           |  O  Third task...  (G)|
           |           ∨          |  ← Down arrow (hidden at bottom)
            \======================/
```

### Element Specifications

| Element | Size | Position | Style |
|---|---|---|---|
| `todo_container` | 400×380 | Center, Y+20 | Scrollable, bg=LV_COLOR_BG_CARD, radius=16, border=LV_COLOR_BORDER |
| `arrow_up` | auto | Container top center, Y+5 | LV_SYMBOL_UP, color=LV_COLOR_TEXT_MUTED, hidden when at scroll top |
| `arrow_down` | auto | Container bottom center, Y-5 | LV_SYMBOL_DOWN, color=LV_COLOR_TEXT_MUTED, hidden at scroll bottom |
| Task row (focused) | 380×auto | Centered via scroll snap | Font=Montserrat_20, 2-line wrap, full opacity |
| Task row (unfocused) | 380×56 | Stack vertically | Font=Montserrat_14, single line ellipsis, opacity=LV_OPA_60 |
| Priority indicator | 12×12 circle | Left edge of row, X+8 | Border or bg color mapped to priority |
| Checkbox | auto | Left of text, X+30 | LV_SYMBOL_WIFI (empty) / LV_SYMBOL_OK (checked) |

### Priority Color Mapping

Uses existing theme colors — no new definitions needed:

| Priority | Theme Constant | Hex Value | Visual |
|---|---|---|---|
| 0 (Red/Urgent) | `LV_COLOR_ERROR` | #D4847A | Soft red |
| 1 (Orange/Medium) | `LV_COLOR_WARNING` | #E8C97A | Warm amber |
| 2 (Green/Low) | `LV_COLOR_SUCCESS` | #A8C5A0 | Sage green |

---

## Example Data

```c
typedef struct {
    const char *text;
    int priority;       // 0=Red, 1=Orange, 2=Green
    bool completed;
} todo_item_t;

static todo_item_t todo_items[] = {
    {"Finish project report",    0, false},   // Red
    {"Reply to emails",          1, false},   // Orange
    {"Buy groceries",            2, false},   // Green
    {"Schedule dentist",         1, false},   // Orange
    {"Read 20 pages",            2, false},   // Green
    {"Clean desk",               0, false},   // Red
};
#define TODO_COUNT (sizeof(todo_items) / sizeof(todo_items[0]))
```

---

## Core Behaviors

### 1. Focus Magnification

**Mechanism:** LVGL scroll snap centers the focused item. Style changes are applied via `LV_EVENT_FOCUSED` / `LV_EVENT_DEFOCUSED` on each row.

**Focused state:**
```c
lv_obj_set_style_text_font(row, &lv_font_montserrat_20, 0);
lv_obj_set_style_text_opa(row, LV_OPA_COVER, 0);          // Full opacity
lv_obj_set_style_pad_ver(row, 12, 0);                       // Extra vertical padding for 2-line wrap
lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);           // Allow text wrapping
```

**Unfocused state:**
```c
lv_obj_set_style_text_font(row, &lv_font_montserrat_14, 0);
lv_obj_set_style_text_opa(row, LV_OPA_60, 0);              // Muted
lv_obj_set_style_pad_ver(row, 6, 0);                        // Compact
lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);            // Ellipsis on overflow
```

**Scroll snap configuration:**
```c
lv_obj_set_scroll_snap_y(container, LV_SCROLL_SNAP_CENTER);
```

### 2. Navigation Arrows

Arrow visibility is driven by a scroll event callback on `todo_container`:

```c
static void todo_scroll_cb(lv_event_t *e) {
    lv_obj_t *container = lv_event_get_target(e);
    lv_coord_t scroll_y = lv_obj_get_scroll_y(container);
    lv_coord_t scroll_top = lv_obj_get_scroll_top(container);

    // Up arrow: hidden when at absolute top
    if (scroll_y <= 0) {
        lv_obj_add_flag(arrow_up, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(arrow_up, LV_OBJ_FLAG_HIDDEN);
    }

    // Down arrow: hidden when at absolute bottom
    lv_coord_t scroll_bottom = lv_obj_get_scroll_bottom(container);
    if (scroll_y >= scroll_top - scroll_bottom) {
        lv_obj_add_flag(arrow_down, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(arrow_down, LV_OBJ_FLAG_HIDDEN);
    }
}
```

### 3. Completion Flow (Encoder Press)

When the encoder is pressed on a focused, uncompleted task:

**Step 1 — Instant visual feedback (0ms):**
```c
// Check the checkbox
lv_checkbox_set_checked(cb, true);

// Strikethrough the label
lv_obj_set_style_text_decor(label, LV_TEXT_DECOR_STRIKE_THROUGH, 0);

// Mute the text color
lv_obj_set_style_text_color(label, lv_color_make(80, 80, 80), 0);

// Mark in data array
todo_items[index].completed = true;
```

**Step 2 — 500ms freeze:**
```c
typedef struct {
    lv_obj_t *row;
    lv_obj_t *container;
    int item_index;
} shuffle_data_t;

shuffle_data_t *data = malloc(sizeof(shuffle_data_t));
data->row = row;
data->container = container;
data->item_index = index;

// One-shot timer (repeat_count = 1)
lv_timer_t *timer = lv_timer_create(shuffle_timer_cb, 500, data);
timer->repeat_count = 1;
```

**Step 3 — Shuffle down (at 500ms):**
```c
static void shuffle_timer_cb(lv_timer_t *timer) {
    shuffle_data_t *data = (shuffle_data_t *)timer->user_data;

    // Move completed row to bottom of layout
    lv_obj_move_background(data->row);

    // Reduce its opacity to indicate completion
    lv_obj_set_style_text_opa(data->row, LV_OPA_40, 0);

    // Scroll to center the next uncompleted item
    // Find first uncompleted child and scroll to it
    for (int i = 0; i < lv_obj_get_child_cnt(data->container); i++) {
        lv_obj_t *child = lv_obj_get_child(data->container, i);
        if (!is_row_completed(child)) {
            lv_obj_scroll_to_view(child, LV_ANIM_ON);
            break;
        }
    }

    free(data);
    lv_timer_del(timer);
}
```

### 4. Encoder Input Handling

```c
void screen_todos_encoder_event(lv_indev_data_t *data) {
    // Rotation: LVGL handles scroll natively via indev driver
    // No manual scroll needed — encoder rotation scrolls the container

    // Press: toggle checkbox on focused item
    if (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0) {
        lv_obj_t *focused = get_focused_row();  // Find child with LV_STATE_FOCUSED
        if (focused && !is_row_completed(focused)) {
            complete_task(focused);  // Triggers the 3-step flow above
        }
    }
}
```

**Note on scroll:** The LVGL encoder indev driver automatically scrolls focused objects into view when `enc_diff` is non-zero. By adding each row to an `lv_group_t`, the encoder rotation will cycle focus through rows, and LVGL handles the scroll positioning.

---

## Simulator ↔ ESP32 Portability

| Concern | Strategy |
|---|---|
| **Source code** | Single `screen_todos.c` compiled for both targets. Zero `#ifdef` in screen code. |
| **Scroll behavior** | LVGL's built-in scroll engine works identically on both platforms — no platform-specific scroll code. |
| **Font rendering** | Montserrat fonts are compiled into LVGL on both targets. Same pixel output. |
| **Focus/group** | `lv_group_t` + encoder indev works the same on both platforms. Simulator keyboard maps to `enc_diff`/`state`. |
| **Timer (500ms hold)** | `lv_timer_create()` runs inside `lv_timer_handler()` on both platforms. Timing is driven by LVGL tick, which is sourced from `SDL_GetTicks()` (simulator) or `esp_timer` (ESP32) — both millisecond-accurate. |
| **Color consistency** | Theme colors are `#define` macros using `lv_color_hex()`. RGB565 color space is identical on both targets (16-bit depth confirmed in `lv_conf.h`). |
| **Round display** | Simulator applies circle mask in flush callback. ESP32 has physical round LCD. Screen code assumes 480×480 square — circle mask is applied at display layer, not UI layer. |

**Key principle:** The screen code interacts with LVGL APIs only. All platform differences are abstracted by the display driver, input driver, and font system — none of which the screen code touches directly.

---

## Animation Fidelity Checklist

| Spec Requirement | Implementation | Verification |
|---|---|---|
| Focused item centered in viewport | `lv_obj_set_scroll_snap_y(container, LV_SCROLL_SNAP_CENTER)` | Rotate encoder, confirm focused item is always vertically centered |
| Focused item is magnified | `LV_EVENT_FOCUSED` → set Montserrat_20, 2-line wrap | Visually confirm font size difference between focused/unfocused |
| Unfocused items show ellipsis | `lv_label_set_long_mode(label, LV_LABEL_LONG_DOT)` | Verify long text shows "..." when not focused |
| Unfocused items are muted | `lv_obj_set_style_text_opa(row, LV_OPA_60, 0)` | Confirm unfocused items are semi-transparent |
| Up arrow hidden at top | Scroll callback checks `scroll_y <= 0` | Scroll to absolute top, verify arrow disappears |
| Down arrow hidden at bottom | Scroll callback checks scroll boundary | Scroll to absolute bottom, verify arrow disappears |
| Checkbox + strikethrough on press | `lv_checkbox_set_checked()` + `LV_TEXT_DECOR_STRIKE_THROUGH` | Press Enter on task, confirm strikethrough appears instantly |
| 500ms freeze before shuffle | `lv_timer_create(cb, 500, data)` with `repeat_count=1` | Press Enter, count 1-Mississippi, then see item move |
| Completed item moves to bottom | `lv_obj_move_background(row)` | After 500ms, confirm completed item is at bottom of list |
| Completed item is visually muted | `lv_obj_set_style_text_opa(row, LV_OPA_40, 0)` | Confirm shuffled item is very faint |
| Priority colors match spec | Error=#D4847A, Warning=#E8C97A, Success=#A8C5A0 | Visually confirm 3 distinct colors on left edge |
| Scroll is smooth | `LV_ANIM_ON` on scroll-to-view calls | Confirm smooth scroll animation, not instant jump |

---

## Implementation Order

1. Create `screen_todos.h` — function declarations
2. Create `screen_todos.c` — build incrementally:
   - Step 1: Static list rendering (all 6 items, no interaction)
   - Step 2: Scroll snap + focus magnification (Montserrat_20 vs _14)
   - Step 3: Navigation arrows (scroll callback)
   - Step 4: Priority color indicators (left border/bg per priority)
   - Step 5: Checkbox + strikethrough on press
   - Step 6: 500ms timer + shuffle to bottom
   - Step 7: Completed item opacity reduction
3. Wire into `ui_manager.c` — register screen + handler
4. Verify menu entry in `screen_menu.c` (target = `SCREEN_TODOS`)
5. Update ESP32 `main/CMakeLists.txt` — add to SRCS
6. Update simulator `simulator/CMakeLists.txt` — add to `studybud_screens`
7. Build simulator, test full flow
8. Verify animations match timing specs

---

## Dependencies

| Dependency | Version | Notes |
|---|---|---|
| LVGL | 8.2.0 | Already in `simulator/lvgl/` and via ESP-IDF component |
| `lv_checkbox` | Built-in | `LV_USE_CHECKBOX 1` in `lv_conf.h` |
| `lv_list` | Built-in | `LV_USE_LIST 1` in `lv_conf.h` |
| `lv_label` | Built-in | `LV_USE_LABEL 1` in `lv_conf.h` |
| `lv_anim` | Built-in | Used for scroll animations |
| Montserrat fonts | 14, 16, 20 | Already enabled in `lv_conf.h` |
| SDL2 | Any | Simulator only, via `brew install sdl2` |

---

## Future Enhancements (Out of Scope)

- **Live data from Svelte app:** Replace hardcoded `todo_items[]` with WebSocket-fed data
- **Task creation/deletion:** Add UI for creating new tasks via encoder
- **Drag to reorder:** Touch/mouse-based reorder (not applicable to encoder-only input)
- **Due dates / reminders:** Extended task metadata
- **Categories / tags:** Beyond the 3 priority levels
