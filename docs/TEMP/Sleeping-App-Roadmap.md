# Sleep Tracker — Implementation Roadmap

## Goal

Implement the Sleep Tracker (Intro → Start → Active Log → Confirm → Summary, plus a
Weekly Review branch) as a single `screen_sleep.c/.h` pair, following the exact patterns
already established by `screen_breathing.c` (multi-state single-screen machine) and
`screen_todos.c` / `screen_menu.c` (radial focus + LVGL idioms). No new UI patterns are
introduced — everything below reuses code shapes that already exist in the repo.

**Correction from the previous draft:** the clock on the Start screen is a plain live
readout of the current time (same mechanism as `screen_home.c`'s `time_label`), not an
editable "set bedtime" control. Logging works like a stopwatch: clicking Start Session
records `time(NULL)` as the start timestamp; clicking to end the log later records a second
timestamp; the displayed/stored sleep duration is simply `end_time - start_time`. There is
no bedtime-editing UI, no `edit_mode` flag, and no rotate-to-adjust-time interaction
anywhere in this feature.

---

## Hard constraint: the real encoder contract

This is the actual event contract every screen handler receives — confirmed by reading
`ui_manager.c`, not the docs:

| Event your handler receives | When |
|---|---|
| `state = LV_INDEV_STATE_REL`, `enc_diff != 0` | Rotation (left/right). Forwarded **immediately**, even while the button is held down. |
| `state = LV_INDEV_STATE_PR`, `enc_diff == 0` | A **single synthetic click** — sent once, on button release, only if it wasn't a long press. |

`ui_manager_encoder_event()` swallows the raw press/release cycle itself (to detect the
800ms long-press-opens-menu gesture) and only ever forwards this one synthetic
`PR + enc_diff=0` "click" to the active screen. **The screen never needs to know, and never
finds out, how long the button was held** — it only ever learns "a click happened." That
matches exactly what you described: the screen code just needs a binary click signal, and
that's all `ui_manager` gives it. Everything below is designed around that single click
primitive plus rotation.

One thing worth flagging while we're in this territory: `screen_breathing.c`'s
`STATE_ACTIVE` handler checks for `data->state == LV_INDEV_STATE_REL` to try to measure tap
duration for an abort gesture. Per the contract above, that branch is only ever reached via
rotation forwarding (which also carries `REL`), never an actual button release — so that
particular piece of logic doesn't do what its comments imply. Not something the Sleep
Tracker needs to fix, but worth knowing so the same dead pattern isn't copied forward.

---

## File structure

```
main/display/
├── screens/
│   ├── screen_sleep.c       ← All 5 states in one file (mirrors screen_breathing.c)
│   └── screen_sleep.h
└── utils/
    ├── sleep_store.c        ← Start/stop timestamps, 7-day history (NEW)
    └── sleep_store.h
```

Add to both `main/CMakeLists.txt` and `simulator/CMakeLists.txt` (same two-file pattern
`Breathing-App-Roadmap.md` used for `screen_breathing.c` + `session_store.c`).

---

## How screens vs. states work (important distinction)

Two different kinds of "moving to a new screen" exist in this codebase, and the Sleep
Tracker roadmap uses both:

1. **Cross-screen navigation** — handled entirely by `ui_manager.c`. Going from the Menu
   wheel to the Sleep Tracker, or a long-press back to the Menu, calls
   `ui_manager_switch_screen()`, which does `lv_scr_load_anim(screens[screen],
   LV_SCR_LOAD_ANIM_FADE_ON, 300, 0, false)` — a full 300ms cross-fade between two distinct
   `lv_obj_t` screen roots.
2. **In-screen state transitions** — everything *within* the Sleep Tracker (Intro → Start →
   Active → Summary → Weekly) happens inside **one single `lv_obj_t` screen object**,
   exactly like `screen_breathing.c`'s four states. Moving between these states means
   hiding one group of widgets and revealing/fading in another — via
   `lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN)` / `lv_obj_clear_flag(...)` plus a manual
   opacity `lv_anim_t` (the same `animate_style()` helper `screen_breathing.c` already
   defines), not `lv_scr_load_anim`. This is why the whole Sleep Tracker is one `.c` file
   instead of five.

---

## Navigation map (the full user journey)

```
                         ┌─────────────┐
        (long-press,     │  Menu Wheel │
         any screen) ───▶│  (global)   │
                         └──────┬──────┘
                                │ click "Sleep"
                                ▼
                    ╔═══════════════════════╗
                    ║   SCREEN_SLEEP loads   ║   ← cross-screen fade (300ms)
                    ║   always resets to     ║
                    ║   SLEEP_STATE_INTRO    ║
                    ╚═══════════╤═══════════╝
                                │
              ┌─────────────────┴─────────────────┐
              │ rotate: toggle focus (2 targets)   │
              │ click: go to focused target        │
              ▼                                    ▼
     focus=0 "Start Session"              focus=1 "Weekly Review"
              │                                    │
              ▼                                    ▼
   ┌─────────────────────┐              ┌─────────────────────┐
   │  SLEEP_STATE_START   │              │ SLEEP_STATE_WEEKLY  │
   │  (live clock,        │              │  (chart, avg, BACK) │
   │   Start / Back)      │              └──────────┬──────────┘
   └──────────┬───────────┘                         │ click BACK
              │ click Start Session                 ▼
              │ (records start_time)         back to SLEEP_STATE_INTRO
              ▼
   ┌──────────────────────┐
   │  SLEEP_STATE_ACTIVE   │◀────────────┐
   │  (pulsing badge,      │             │ click "Nevermind"
   │   AWAKE? button,      │             │ (cancel, stay logging)
   │   live clock)         │             │
   └──────────┬─────────────┘             │
              │ click AWAKE?              │
              ▼                          │
   ┌──────────────────────┐             │
   │ confirm sub-mode      │─────────────┘
   │ (same screen, button  │
   │  relabels + Nevermind │
   │  appears; rotate      │
   │  toggles focus)       │
   └──────────┬─────────────┘
              │ click "ARE YOU SURE?"
              │ (records end_time, saves to sleep_store)
              ▼
   ┌──────────────────────┐
   │ SLEEP_STATE_SUMMARY   │
   │ (duration result)     │
   └──────────┬─────────────┘
              │ ANY input (rotate or click)
              ▼
        back to SLEEP_STATE_START
```

`SLEEP_STATE_INTRO` is the only re-entry point when you leave and come back to the
feature (via long-press → Menu → Sleep again) — `screen_sleep_create()` doesn't persist
state across screen switches, matching `screen_breathing.c`'s behavior of always starting
fresh at its Selection state.

**Open question:** there's no explicit "Home" shortcut anywhere in this flow (unlike
Breathing's Completion screen, which has its own Home button) — leaving the feature
currently always requires the global 800ms long-press → Menu → Home. Worth deciding whether
that's acceptable or whether Summary/Intro should get an explicit Home button like Breathing
has.

---

## Per-screen layout & behavior

All positions assume the 480×480 round display, `LV_ALIGN_CENTER` as origin `(0,0)`, and
follow the same visual conventions already used across `screen_home.c`, `screen_menu.c`,
and `screen_breathing.c` (title top at `Y+30`, `montserrat_20` for titles, circular
primary-action buttons, capsule secondary buttons).

### 1. `SLEEP_STATE_INTRO`

```
                    Sleep                     ← title, Y+30, montserrat_20
              ┌───────────────┐
              │               │
              │  START        │               ← btn_start_session
              │  SESSION      │                 240×240 circle, centered Y-20
              │               │                 LV_COLOR_PRIMARY bg
              └───────────────┘
                ( Weekly Review )              ← btn_weekly_review
                                                  160×50 capsule, centered Y+150
                                                  LV_COLOR_PRIMARY_DARK bg
```

| Element | Size / Position | Style |
|---|---|---|
| Title "Sleep" | auto, `TOP_MID Y+30` | montserrat_20, `LV_COLOR_TEXT` |
| `btn_start_session` | 240×240, `CENTER Y-20` | circle, `LV_COLOR_PRIMARY`, label "START SESSION" montserrat_20 |
| `btn_weekly_review` | 160×50, `CENTER Y+150` | capsule (radius 25), `LV_COLOR_PRIMARY_DARK`, label "Weekly Review" montserrat_16 |

**Encoder behavior** — identical shape to `screen_breathing.c`'s `STATE_SELECTION`:
- Rotate (any `enc_diff != 0`): `focus_index = focus_index == 0 ? 1 : 0` (two-way toggle,
  not a wrapping list — same as breathing's BEGIN/Cycles toggle, since there are only 2
  targets).
- Click (`PR + enc_diff==0`): `focus_index==0` → `transition_to_start()`; `focus_index==1`
  → `transition_to_weekly()`.
- Focus visuals: reuse `animate_style()` for border-width (0→4) and opacity (80%→100%) on
  the focused element, exactly as breathing's `update_focus_styles()` does.

### 2. `SLEEP_STATE_START`

```
                 Good evening                 ← greeting, Y+45, montserrat_16
                  ╭───────╮
                 │ 23:47   │                   ← live clock inside a
                  ╰───────╯                      circular frame, Y-30
                                                  montserrat_36, updates every 1s

              ┌───────────────┐
              │ START SESSION │                ← btn_start_session
              └───────────────┘                  200×56 capsule, Y+120

                    Back                       ← btn_back
                                                  120×40 capsule, Y+185
```

| Element | Size / Position | Style |
|---|---|---|
| Greeting label | auto, `CENTER Y-90` | montserrat_16, `LV_COLOR_TEXT_SECONDARY`, text set from local hour ("Good evening" / "Good morning" etc.) |
| Clock frame | 180×180 circle (decorative border only, `bg_opa=TRANSP`), `CENTER Y-30` | 3px border, `LV_COLOR_BORDER` |
| Clock label | auto, centered inside frame | montserrat_36, `LV_COLOR_TEXT`, updated by a 1000ms `lv_timer_t` exactly like `screen_home.c`'s `update_time_cb` |
| `btn_start_session` | 200×56, `CENTER Y+120` | capsule (radius 28), `LV_COLOR_PRIMARY`, label montserrat_16 |
| `btn_back` | 120×40, `CENTER Y+185` | capsule (radius 20), `LV_COLOR_PRIMARY_DARK`, label montserrat_14 |

**Encoder behavior** — two focus targets (clock is display-only, not focusable):
- Rotate: `focus_index` toggles between `btn_start_session` (0) and `btn_back` (1).
- Click on `btn_start_session`: `sleep_store_start_session()` (writes start timestamp to
  NVS immediately — see storage section), then `transition_to_active()`.
- Click on `btn_back`: `transition_to_intro()`.

### 3. `SLEEP_STATE_ACTIVE`

```
                  ┌──────────┐
                  │  AWAKE?  │                 ← btn_awake, TOP_MID Y+40
                  └──────────┘                   160×50 capsule

                   ╭────────╮
                  │          │
                  │ Goodnight!│                ← pulsing badge, CENTER
                  │          │                    200×200 circle,
                   ╰────────╯                     opacity oscillates
                                                   (reuse start_pulse_animation()
                                                    from screen_idle_background.c)

                     23:52                     ← live clock, BOTTOM_MID Y-40
```

**Confirm sub-mode** (after clicking AWAKE?, same screen, in place):

```
                  ┌────────────────┐
                  │ ARE YOU SURE?  │           ← btn_awake, relabeled + LV_COLOR_ERROR bg
                  └────────────────┘

                     Nevermind                 ← btn_nevermind appears, Y+100 below btn_awake
                                                   140×40 capsule, LV_COLOR_PRIMARY_DARK
```

| Element | Size / Position | Style |
|---|---|---|
| `btn_awake` | 160×50, `TOP_MID Y+40` | capsule, `LV_COLOR_SUCCESS` normally / `LV_COLOR_ERROR` (`0xE74C3C`) in confirm mode |
| Pulsing badge | 200×200 circle, `CENTER` | `LV_COLOR_PRIMARY`, opacity anim 30%↔80% over 1500ms, `LV_ANIM_REPEAT_INFINITE` — literally reuse `start_pulse_animation()` |
| "Goodnight!" label | centered inside badge | montserrat_16, white |
| Clock label | auto, `BOTTOM_MID Y-40` | montserrat_16, live-updating 1s timer (same mechanism as Start screen) |
| `btn_nevermind` | 140×40, `TOP_MID Y+100` | capsule, hidden until confirm mode, `LV_COLOR_PRIMARY_DARK` |

**Encoder behavior:**
- Not in confirm mode: click on the (only) `btn_awake` → `enter_confirm_mode()`
  (relabel to "ARE YOU SURE?", set `LV_COLOR_ERROR` bg, reveal `btn_nevermind`,
  `focus_index = 0`). No rotate handling needed here — matches `screen_home.c`'s no-op
  rotate stub, since there's only one target.
- In confirm mode: rotate toggles `focus_index` between `btn_awake` (0, "ARE YOU SURE?")
  and `btn_nevermind` (1) — same two-way toggle as Intro. Click:
  `focus_index==0` → `sleep_store_end_session()` (computes `end_time - start_time`),
  `transition_to_summary()`; `focus_index==1` → `exit_confirm_mode()` (revert label/color,
  hide `btn_nevermind`, back to normal Active state).

### 4. `SLEEP_STATE_SUMMARY`

```
             You slept for 7h 12m              ← lbl_result, CENTER Y-30
                                                   montserrat_28

           That's close to your goal!          ← lbl_sub, CENTER Y+15
                                                   montserrat_14, LV_COLOR_TEXT_SECONDARY

              Press to continue...              ← hint, BOTTOM_MID Y-40
                                                   montserrat_14, LV_COLOR_TEXT_MUTED
```

No focusable buttons — matches the doc's "remains visible until any encoder input" spec.

**Encoder behavior:**
```c
if (data->enc_diff != 0 || (data->state == LV_INDEV_STATE_PR && data->enc_diff == 0)) {
    transition_to_start();   // both rotate and click dismiss
}
```

### 5. `SLEEP_STATE_WEEKLY`

```
              Your weekly average is           ← header, TOP_MID Y+35
                                                   montserrat_16

                    8 hrs                      ← metric, TOP_MID Y+65
                                                   montserrat_36 (largest font)

          Great consistency this week!         ← encouragement, TOP_MID Y+110
                                                   montserrat_14

              ┌──────────────────┐
              │  ▂ ▅ ▃ ▇ ▅ ▃ ▅   │             ← lv_chart_t, CENTER Y+30
              └──────────────────┘               280×120, LV_CHART_TYPE_BAR, 7 points

                     Back                      ← btn_back, BOTTOM_MID Y-20
                                                   110×40 capsule
```

Matches the reference LVGL code sample almost exactly (it's already correct — no
corrections needed there).

**Encoder behavior:** single focusable target (`btn_back`), no rotate handling needed
(same no-op-rotate shape as `screen_home.c`). Click → `transition_to_intro()`.

---

## New storage: `utils/sleep_store.c/.h`

Breathing's `session_store` is in-memory only (`static int`, no persistence) per
`Breathing-Implementation-Plan.md`. Sleep can't get away with that — a 7-day weekly average
needs to survive reboots, and if the device restarts mid-log, the start timestamp needs to
still be there. Recommend NVS-backed from day one rather than starting in-memory and
migrating later:

```c
// sleep_store.h
void     sleep_store_init(void);
void     sleep_store_start_session(void);            // writes time(NULL) to NVS immediately
uint32_t sleep_store_end_session(void);               // reads stored start, computes
                                                        // (now - start) in minutes, appends
                                                        // to 7-day history, returns duration
float    sleep_store_get_weekly_avg_hours(void);
void     sleep_store_get_last_7_days(uint16_t out_minutes[7]);
```

`sleep_store_start_session()` writing immediately (rather than buffering the timestamp only
in RAM) means a crash or reset mid-log doesn't silently lose the in-progress session — worth
testing explicitly (kill the simulator mid-`SLEEP_STATE_ACTIVE`, relaunch, confirm the start
timestamp is still there before you click "ARE YOU SURE?").

**Day-bucketing rule** (`Sleeping-App-Implementation-Plan.md`'s "noon-to-noon" spec): a
session that starts at 23:30 and ends at 07:00 should be attributed to whichever calendar
day the *start* timestamp falls on, using a noon boundary rather than midnight — implement
this as one helper, `day_bucket_for_timestamp()`, inside `sleep_store.c`, so the boundary
logic lives in exactly one place instead of being re-derived by the UI layer or the chart
code.

---

## Wiring changes required

1. **`ui_manager.h`** — add `SCREEN_SLEEP` to the `screen_id_t` enum.
2. **`ui_manager.c`** — add `#include "screens/screen_sleep.h"`, register
   `screens[SCREEN_SLEEP] = screen_sleep_create()` and
   `screen_event_handlers[SCREEN_SLEEP] = screen_sleep_encoder_event` in
   `ui_manager_init()`, next to the existing Breathing/Backgrounds registrations.
3. **`screen_menu.c`** — add a `{ LV_SYMBOL_..., "Sleep", SCREEN_SLEEP }` entry to
   `menu_items[]`. (Also worth doing while you're in there: this feature isn't yet in
   `UI-Feature-List.md`'s feature matrix or `Display-Interaction-Guide.md`'s menu order —
   worth updating those two docs alongside the code so they don't drift again.)
4. **Both `CMakeLists.txt`** — add `screen_sleep.c` and `sleep_store.c` to `SRCS` /
   `studybud_screens`.

---

## Implementation order

1. `utils/sleep_store.h` + `.c` — storage layer first, independent of any UI, easy to
   unit-test standalone (start a session, end it, confirm duration math and day-bucketing)
2. `screens/screen_sleep.h` — two-function declarations matching `screen_breathing.h`'s shape
3. `screens/screen_sleep.c`, built incrementally:
   - Step 1: `SLEEP_STATE_INTRO` — two-button focus toggle (copy `animate_style` /
     `update_focus_styles` shape from `screen_breathing.c`)
   - Step 2: `SLEEP_STATE_START` — live clock (copy `update_time_cb` timer pattern from
     `screen_home.c`), Start/Back focus toggle
   - Step 3: `SLEEP_STATE_ACTIVE` — pulsing badge (reuse `start_pulse_animation()` from
     `screen_idle_background.c`), confirm sub-mode
   - Step 4: `SLEEP_STATE_SUMMARY` — duration display, any-input dismiss
   - Step 5: `SLEEP_STATE_WEEKLY` — `lv_chart_t` with 7-day data from `sleep_store`
4. Wire into `ui_manager.c` + `screen_menu.c` (section above)
5. Update both `CMakeLists.txt`
6. Build simulator, test the full loop: Intro → Start → Active → Confirm → Summary → back to
   Start, and Intro → Weekly → back to Intro
7. Reboot-mid-session test (see storage section above)

---

## Open questions worth resolving before you start coding

- **No explicit Home shortcut anywhere in this flow** — see the navigation map note above.
  Worth deciding whether the global long-press-to-menu is sufficient, or whether Summary
  should get its own Home button like Breathing's Completion screen has.
- **Does a global long-press during `SLEEP_STATE_ACTIVE` interrupt an in-progress log?**
  The 800ms long-press-to-menu gesture is handled entirely by `ui_manager.c` and fires
  regardless of which screen is active — it doesn't stop the session (the timestamp is
  already safely in NVS), but the user would land on the Menu screen mid-log rather than
  staying on the pulsing badge. Confirm that's acceptable, since there's currently no way
  for a screen to opt out of the global long-press gesture.
- **Is Weekly Review reachable only from Intro, or also directly from the menu wheel?**
  This roadmap only wires it as an Intro sub-branch, matching the doc's nav diagram — if
  you want one-tap access to the chart without detouring through Intro, it needs its own
  `screen_id_t` and menu entry rather than being a sub-state of `screen_sleep.c`.