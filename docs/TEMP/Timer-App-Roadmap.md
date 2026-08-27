# Timers Module — Implementation Roadmap

## Goal

Implement the Timers Module as three screens — a horizontal preset carousel, a running
countdown screen, and a preset editor (Standard / Pomodoro) — following the same
established patterns as `screen_breathing.c` (multi-state single screen, focus toggles,
edit-mode) and `screen_menu.c` / `screen_idle_background.c` (radial-scroll focus lists).

Two of the three screen IDs already exist and are unused: `ui_manager.h`'s
`screen_id_t` enum already reserves `SCREEN_TIMER` and `SCREEN_TIMER_PRESETS` — only
`SCREEN_TIMER_EDIT` needs to be added.

---

## Adaptation notice: the original spec assumes touch/keyboard widgets

The source spec calls for an `lv_dropdown_create()` mode selector and three separate
per-digit up/down arrow buttons (9 individually-tappable targets) for the HH:MM:SS
picker. Neither maps onto a single rotary encoder. Every interaction below has been
redesigned around the same constraint the rest of this codebase already runs on —
confirmed by reading `ui_manager.c`:

| Event a screen handler receives | When |
|---|---|
| `state = LV_INDEV_STATE_REL`, `enc_diff != 0` | Rotation (left/right) |
| `state = LV_INDEV_STATE_PR`, `enc_diff == 0` | A single synthetic click, sent once, only if it wasn't a long press |

No screen ever learns hold duration, and there is no "swipe" or "tap a specific widget"
gesture available — only **rotate to move focus / adjust a value** and **click to
select**. Dropdowns become click-to-cycle capsules; the three-column digit picker
becomes a click-to-arm, rotate-to-adjust, click-to-advance-segment flow (the same shape
`screen_breathing.c` already uses for its cycle-count edit mode).

---

## File structure

```
main/display/
├── screens/
│   ├── screen_timer_presets.c   ← Screen 1: horizontal carousel (NEW)
│   ├── screen_timer_presets.h
│   ├── screen_timer.c           ← Screen 2: running timer + safety check (NEW)
│   ├── screen_timer.h
│   ├── screen_timer_edit.c      ← Screen 3: Standard/Pomodoro editor (NEW)
│   └── screen_timer_edit.h
└── utils/
    └── timer_store.c/.h         ← Persisted presets (NVS) (NEW)
```

Three separate files rather than one combined file (unlike Breathing's four small
states in one file) because each of these three screens is independently as large as
the whole Breathing feature — matches the naming `LVGL-Roadmap.md`'s Phase 2 already
anticipated (`screen_timer.c`, `screen_timer_presets.c`).

---

## Architecture note: this is the first *bidirectional* screen

Breathing, Todos, and Sleep are all local-only or device-authoritative. Per
`UI-Feature-List.md`'s feature matrix, **Timer is marked "Bidirectional"** — starting a
timer from the web app must update the display immediately, and vice versa. That means
`screen_timer.c` can't just keep its running state (`remaining_seconds`, `phase`,
`is_running`) in file-scope statics the way `screen_breathing.c` does — per
`Connection-Plan.md`'s single-source-of-truth model, this state needs to live somewhere
a future WebSocket handler can also read and write (`app_state_t`), with the screen
re-rendering whenever that shared state changes rather than owning it outright. No
WebSocket server exists yet in this repo, so this roadmap keeps the running state in
`screen_timer.c` statics for now — same as every other screen in the queue — but
structures the update path through a single `timer_apply_state()` function so that
swapping the source of truth later (shared struct instead of local statics) doesn't
require rewriting the rendering logic.

---

## Navigation map

```
                          ┌──────────────┐
        (long-press,      │  Menu Wheel  │
         any screen)  ───▶│   (global)   │
                          └──────┬───────┘
                                 │ click "Timer"
                                 ▼
                  ╔═══════════════════════════╗
                  ║   SCREEN_TIMER_PRESETS     ║   ← cross-screen fade (300ms)
                  ║   (horizontal carousel)    ║
                  ╚═════════════╤══════════════╝
                                 │
              rotate: shift which preset is centered
              click on centered preset (not "+"): enter action-focus
                                 │
              ┌──────────────────┴───────────────────┐
              │ rotate: toggle EDIT / PLAY            │
              ▼                                       ▼
        click EDIT                              click PLAY
              │                                       │
              ▼                                       ▼
  ╔═══════════════════════╗                ╔═══════════════════════╗
  ║   SCREEN_TIMER_EDIT     ║                ║      SCREEN_TIMER      ║
  ║  (existing preset       ║                ║   (running countdown)  ║
  ║   loaded)                ║                ╚═══════════╤═══════════╝
  ╚═══════════╤═════════════╝                            │
              │ click Save                    click Back │
              ▼                                          ▼
     back to SCREEN_TIMER_PRESETS              relabel Back → "ARE YOU SURE?"
     (carousel reflects change)                 reveal "Nevermind", rotate toggles
                                                          │
              click Cancel (X)              click ARE YOU SURE?  │  click Nevermind
              │                                          │        │
              ▼                                          ▼        ▼
     back to SCREEN_TIMER_PRESETS             back to SCREEN_TIMER_PRESETS
     (no changes saved)                        (timer stops)   (resume running,
                                                                 no state lost)


        centered on the "+" bubble (last carousel slot)
              │ click
              ▼
     SCREEN_TIMER_EDIT (blank/new preset mode)
```

---

## Screen 1: `screen_timer_presets.c` — Horizontal Carousel

### Layout

This is the horizontal twin of the radial-scroll pattern already implemented in
`screen_menu.c` / `screen_idle_background.c` — same magnification-by-distance-from-center
technique, just computed along the X axis with a horizontal flex row instead of a
vertical column. The "flanking index bubbles" are simply the *unfocused* rendering of
adjacent presets (smaller, muted, showing only their index) — no separate widget type is
needed; it's the same defocused-row styling `update_focus_styles()` already produces in
those two files, ported to a horizontal `lv_obj_set_scroll_dir(container, LV_DIR_HOR)`
with `lv_obj_set_scroll_snap_x(container, LV_SCROLL_SNAP_CENTER)`.

```
   ╭───╮                                                    ╭───╮
  │  1  │        ┌─────────────────────────────┐          │  +  │
   ╰───╯        │        Pomodoro 2             │           ╰───╯
  (prev,        │    Short: 5m | Long: 15m      │          (add new,
   muted,       │          24:58                │           right edge,
   Y0, small)   │      [ EDIT ]   [ ▶ ]         │           always last)
                └─────────────────────────────┘
                (centered, full size, full opacity)
```

| Element | Behavior |
|---|---|
| Centered card | Title ("Pomodoro 2" / "Timer 1"), Pomodoro subtext (only if `type == MODE_POMODORO`), time formatted `mm:ss` (< 1hr) or `hh:mm:ss` (≥ 1hr), `[EDIT]` + `[▶]` buttons |
| Flanking previews | Small circle, index number only, muted opacity — same styling `update_focus_styles()` already applies to non-focused rows |
| "+" bubble | Always the last carousel slot. No EDIT/PLAY pair — a distinct row type (`ROW_ADD_NEW`) that goes straight to the editor on click |

**Deliberate deviation from `screen_menu.c`'s wrap-around:** per the spec, the left side
stays blank when the first preset is centered, and the right side stays blank (except
for the "+" bubble at the very end) when the last preset is centered. This means
**rotation clamps rather than wraps** here — `new_idx` is bounded to
`[0, preset_count]` instead of using modulo, unlike every other radial list in the
codebase. Worth double-checking this is intentional before you build it, since it's the
one list in the app that doesn't loop.

### Focus tiers (adaptation decision — flag for confirmation)

Because each preset card has *two* actions (Edit, Play) rather than the single action
every other radial list in this codebase uses, browsing and acting are split into two
tiers:

- **Tier 1 (browse):** rotate shifts which preset is centered (clamped, not wrapped).
  Click on a centered normal preset → enters Tier 2. Click on the centered "+" bubble →
  jumps straight to `SCREEN_TIMER_EDIT` in new-preset mode (no Tier 2, nothing to play
  yet).
- **Tier 2 (act):** rotate toggles focus between `[EDIT]` and `[▶]` (same two-way toggle
  shape as `screen_breathing.c`'s Selection state). Click executes the focused action.

There's no dedicated "back out of Tier 2 without picking" gesture — the assumption is
that landing on Edit and hitting Cancel, or landing on Play and hitting Back, absorbs
that case. **Worth confirming this doesn't feel like a dead end** before building it —
if it does, adding a distinct third rotate step that exits back to Tier 1 is a small
change.

---

## Screen 2: `screen_timer.c` — Running Timer

### Layout

```
                    Pomodoro 2                    ← title, TOP_MID Y+35, montserrat_20

              ╭─────────────────────╮
             │    ╭───────────╮      │            ← outer progress arc (lv_arc),
             │   │             │     │              340px diameter, angle =
             │   │   19:58     │     │              (remaining/total) × 360°
             │   │             │     │
             │    ╰───────────╯      │            ← countdown label, CENTER,
              ╰─────────────────────╯                montserrat_36
                 (Phase: Session)                 ← phase label, CENTER Y+55,
                                                      montserrat_14, only if Pomodoro

              [ || ]      [ R ]      [ BACK ]     ← 3 controls, BOTTOM_MID Y-40
```

| Element | Size / Position | Style |
|---|---|---|
| Title | auto, `TOP_MID Y+35` | montserrat_20, preset name |
| Progress arc | 340×340, `CENTER` | `lv_arc`, angle driven by `remaining/total`, `LV_COLOR_PRIMARY` (focus) / `LV_COLOR_INFO` (break) |
| Countdown label | auto, `CENTER` | montserrat_36, format via the same `mm:ss` / `hh:mm:ss` helper used on Screen 1 |
| Phase label | auto, `CENTER Y+55` | montserrat_14, `LV_COLOR_TEXT_SECONDARY`, hidden for Standard timers |
| `btn_pause_resume` | 70×70 circle, `BOTTOM_MID X-100 Y-40` | icon `⏸`/`▶` toggles per state |
| `btn_restart` | 70×70 circle, `BOTTOM_MID Y-40` | icon `↺` |
| `btn_back` | 70×70 circle, `BOTTOM_MID X+100 Y-40` | text/icon relabels in safety-check mode |

### Encoder behavior

- Rotate: three-way wrap toggle across `[Pause/Resume, Restart, Back]` — same modulo
  pattern `screen_menu_encoder_event` already uses for `new_idx`.
- Click on Pause/Resume: toggles `is_running`, updates the 1s countdown timer
  (`lv_timer_pause`/`lv_timer_resume`), relabels icon.
- Click on Restart: resets `remaining_seconds` to the preset's configured duration for
  the current phase, keeps running state as-is.
- Click on Back: same safety-check pattern as Sleep Tracker's AWAKE?/AreYouSure flow and
  Breathing's abort gesture — relabel to `"ARE YOU SURE?"` in `LV_COLOR_ERROR`, reveal
  `"Nevermind"`, `focus_index = 0`. Rotate now toggles only between those two. Click
  `ARE YOU SURE?` → `ui_manager_switch_screen(SCREEN_TIMER_PRESETS)`. Click
  `Nevermind` → revert labels/colors, restore the 3-way focus ring. **The countdown
  keeps running underneath the confirmation dialog** unless it was already paused —
  the Back confirmation is a UI overlay, not an implicit pause.

### Pomodoro auto-phase-completion behavior

`TEMP-qa-UI-Decisions.md` (Q18) is explicit that breaks must **not** auto-start — the
user presses the encoder to begin the next phase, and the speaker plays a sound when a
phase completes. Implement this as a fourth transient sub-state:

```c
if (remaining_seconds == 0) {
    play_chime();                     // speaker
    is_running = false;
    phase_complete_awaiting_press = true;
    lv_label_set_text(countdown_label, "Press to start " next_phase_name);
}
```

While `phase_complete_awaiting_press` is true, the only accepted input is a click,
which advances `phase` (Session → Short Break, or → Long Break per the configured
cadence) and resumes the countdown. This state is not in the original spec doc — it's
pulled directly from the QA decisions doc, and needs to be layered on top of the base
running-timer behavior above.

---

## Screen 3: `screen_timer_edit.c` — Standard / Pomodoro Editor

### Layout (Standard mode)

```
              ┌─────────────────────────┐
              │  Timer Type | Timer  ▾  │            ← mode capsule, TOP_MID Y+30
              └─────────────────────────┘               260×44, click-to-cycle

                    00  :  30  :  00                  ← HH:MM:SS picker, CENTER Y-20
                                                          montserrat_28, active segment
                                                          gets a highlight border when armed

              ┌────┐            ┌──────────┐
              │ X  │            │   SAVE   │           ← BOTTOM_LEFT / BOTTOM_RIGHT,
              └────┘            └──────────┘             Y-35
```

### Layout (Pomodoro mode — same screen, `container_pomo_buttons` revealed)

```
              ┌─────────────────────────┐
              │ Timer Type | Pomodoro ▾ │
              └─────────────────────────┘

                    00  :  25  :  00

           [ Session ]  [ Short Break ]  [ Long Break ]   ← revealed only in this mode,
                                                              CENTER Y+70, flex row,
                                                              selected phase gets border
                                                              highlight (same visual
                                                              language as focus styling
                                                              elsewhere)

              ┌────┐            ┌──────────┐
              │ X  │            │   SAVE   │
              └────┘            └──────────┘
```

### Focus ring (Tier 1, wraps — this is a settings form, not the no-wrap carousel)

Order: `mode_capsule` → `[session, short_break, long_break]` (only present/counted when
`type == MODE_POMODORO`) → `time_picker` → `cancel` → `save` → wraps back to
`mode_capsule`. Rotate advances `focus_index` through whichever of these are currently
visible; click behavior branches on which one is focused:

| Focused element | Click behavior |
|---|---|
| Mode capsule | Toggles `type` between `MODE_TIMER` / `MODE_POMODORO` immediately (only 2 values, no arm-then-adjust needed — unlike the time picker). Toggling to Pomodoro inserts the 3 phase buttons into the focus ring and reveals `container_pomo_buttons`; toggling to Timer removes them and hides it — directly reusing the existing `mode_dropdown_cb` show/hide logic from the reference sample, just triggered by a click-cycle instead of an `lv_dropdown` value-changed event. |
| Phase button (Pomodoro only) | Sets `active_phase` to Session/Short/Long, updates the time picker's displayed values to that phase's stored duration, adds a border highlight to the selected button (clear the others) |
| Time picker | Click **arms edit mode** on the HH segment (border highlight on HH digits). While armed: rotate adjusts the active segment's value (HH 0–23, MM/SS 0–59, wrapping); click **advances to the next segment** (HH → MM → SS → exits edit mode back to the Tier 1 ring). This click-to-advance-segment sequence replaces the spec's three separate up/down-arrow button pairs, which have no equivalent on a single encoder. |
| Cancel (X) | Discards all changes, `ui_manager_switch_screen(SCREEN_TIMER_PRESETS)` |
| Save | Writes the current `type` + duration(s) to `timer_store` (new preset if opened via "+", update if opened via EDIT), `ui_manager_switch_screen(SCREEN_TIMER_PRESETS)` |

**Crucial rule preserved from the spec:** the 3 phase buttons only exist in the LVGL
tree — not just hidden via opacity — when `type == MODE_POMODORO`. In Timer mode
they're absent from both the screen and the focus ring, so rotating never lands on
them.

---

## New storage: `utils/timer_store.c/.h`

```c
typedef enum { TIMER_STANDARD, TIMER_POMODORO } timer_type_t;

typedef struct {
    int      id;
    char     name[24];
    timer_type_t type;
    uint32_t duration_sec;        // used when type == TIMER_STANDARD
    uint32_t session_sec;         // used when type == TIMER_POMODORO
    uint32_t short_break_sec;
    uint32_t long_break_sec;
    bool     is_default_pomodoro; // the pinned/bold Pomodoro preset from UI-Feature-List.md
} timer_preset_t;

void timer_store_init(void);
int  timer_store_count(void);
timer_preset_t *timer_store_get(int index);
int  timer_store_add(const timer_preset_t *preset);
void timer_store_update(int id, const timer_preset_t *preset);
void timer_store_delete(int id);
```

NVS-backed (not in-memory) — presets need to survive reboots, unlike Breathing's
in-memory-only cycle counter. Seed with a default "Pomodoro" preset
(`is_default_pomodoro = true`, 25/5/15 per `UI-Feature-List.md`) on first boot so the
carousel is never empty.

---

## Wiring changes required

1. **`ui_manager.h`** — add `SCREEN_TIMER_EDIT` to the `screen_id_t` enum.
   (`SCREEN_TIMER` and `SCREEN_TIMER_PRESETS` already exist, unused — no change needed
   there.)
2. **`ui_manager.c`** — register all three: `screens[SCREEN_TIMER_PRESETS]`,
   `screens[SCREEN_TIMER]`, `screens[SCREEN_TIMER_EDIT]`, plus their three
   `screen_event_handlers[...]` entries.
3. **`screen_menu.c`** — the existing `menu_items[]` entry
   `{ LV_SYMBOL_PLAY, "Timer", SCREEN_TIMER }` currently points straight at the running
   countdown screen. That's worth fixing as part of this work — it should point at
   `SCREEN_TIMER_PRESETS` (the carousel) instead, since jumping directly into
   `SCREEN_TIMER` with no preset selected doesn't have a defined behavior in this spec.
4. **Both `CMakeLists.txt`** — add all three new `.c` files plus `timer_store.c` to
   `SRCS` / `studybud_screens`.

---

## Implementation order

1. `utils/timer_store.h` + `.c` — storage layer, seed default Pomodoro preset, unit-test
   add/update/delete independent of any UI
2. `screens/screen_timer_presets.h` + `.c`:
   - Step 1: static horizontal list of cards, no interaction (port `apply_radial_scroll`
     from `screen_menu.c` to the X axis)
   - Step 2: Tier 1 browse (clamped rotate, no wrap)
   - Step 3: Tier 2 action focus (EDIT/PLAY toggle)
   - Step 4: "+" bubble special-case (skips Tier 2)
3. `screens/screen_timer_edit.h` + `.c`:
   - Step 1: mode capsule click-cycle + phase-button show/hide
   - Step 2: time picker arm/adjust/advance-segment flow
   - Step 3: phase button selection + highlight
   - Step 4: Cancel / Save wiring to `timer_store`
4. `screens/screen_timer.h` + `.c`:
   - Step 1: static layout, countdown timer tick, arc update
   - Step 2: Pause/Resume/Restart/Back 3-way focus ring
   - Step 3: Back → safety-check sub-state (reuse Sleep/Breathing pattern)
   - Step 4: Pomodoro phase-complete-awaiting-press sub-state + chime
5. Wire into `ui_manager.c` + fix `screen_menu.c`'s existing Timer entry (section above)
6. Update both `CMakeLists.txt`
7. Build simulator, test the full loop: Carousel → Play → Running → Back-confirm →
   Carousel; Carousel → Edit → change mode/durations → Save → Carousel (reflects
   change); Carousel → "+" → new preset → Save → appears in carousel
8. Pomodoro-specific test: let a Session phase run to zero, confirm it does **not**
   auto-start the break and instead waits for a click, per `TEMP-qa-UI-Decisions.md`

---

## Open questions worth resolving before you start coding

- **Tier 2's lack of an escape hatch** (Screen 1) — confirm landing on Edit-then-Cancel
  or Play-then-Back is an acceptable way to back out of an accidental Tier 2 entry,
  rather than needing a dedicated "return to browsing" rotate step.
- **Carousel clamping vs. every other list's wrap-around** — confirm the no-wrap
  behavior at the first/last preset is intentional, since it's the only list in the
  app that behaves this way.
- **Running-state ownership** — this roadmap keeps `remaining_seconds` / `phase` /
  `is_running` as `screen_timer.c` statics for now, same as every other screen. Once a
  WebSocket server exists, this state needs to move to a shared `app_state_t` so the web
  app's `timer_command` messages and the display agree on one truth — worth deciding now
  whether to build `screen_timer.c` against a `timer_apply_state()` indirection from day
  one (cheap now, expensive to retrofit) or defer it like Breathing/Todos deferred their
  own live-sync work.
- **Long-press during a running countdown** — same question as Sleep Tracker: the
  global 800ms long-press-to-menu gesture isn't screen-aware, so it will interrupt a
  running timer's display (though the countdown itself, if moved to shared state,
  would keep running server-side). Worth confirming that's acceptable.