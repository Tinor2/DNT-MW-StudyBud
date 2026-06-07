# Portable Development Constraints

Reference checklist for building the desktop displayio watch emulator so it ports to the **Adafruit Qualia ESP32-S3** and **TL021WVC02 (2.1" 480×480 round)** without UI redesign. Observe these throughout implementation.

Related: [06_desktop_displayio_emulator_roadmap.md](06_desktop_displayio_emulator_roadmap.md)

---

## 1. Layer boundaries (non-negotiable)

| Layer | Path | May import | Must not import |
|-------|------|------------|-----------------|
| **Shared UI + logic** | `shared/` | `displayio`, `vectorio`, `adafruit_bitmap_font`, other **CircuitPython-safe** libs used on device | `host/`, `device/`, `pygame`, `blinka` board shims, `tkinter`, desktop-only stdlib |
| **Desktop host** | `host/` | `shared/`, `pygame`, Blinka display setup, OS `time` | Device-only: `board`, `dotclockframebuffer`, `busio` for Qualia init |
| **Device platform** | `device/` | `shared/`, `board`, `busio`, `dotclockframebuffer`, `framebufferio`, touch/encoder libs | `pygame`, Blinka |

**Rules:**

- All screens, navigation, and app behavior live in **`shared/`** only.
- **`navigation.py`** consumes only `InputEvent` — never reads hardware or pygame directly.
- Swapping desktop → Qualia means replacing **`host/`** with **`device/`**, not rewriting `shared/apps/*`.

---

## 2. Portable UI contract (allowed in `shared/`)

Use only these display APIs unless a constraint in §6 forces an exception (document the exception in code review).

### displayio

| API / pattern | Allowed | Notes |
|---------------|---------|--------|
| `displayio.Group` | Yes | Keep trees shallow (§4) |
| `Group.x`, `Group.y` | Yes | Primary mechanism for slide animations |
| `display.root_group = …` | Yes | Set from `shared/`; `display` passed in or imported via thin `platform` |
| `displayio.Bitmap` + `Palette` | Yes | Small palettes; RGB565 |
| `displayio.TileGrid` | Yes | For icons and static images |
| `displayio.OnDiskBitmap` | Yes | **BMP only** on device; same format on desktop |
| `displayio.ColorConverter` | Yes | When needed for RGB565 |
| Full-screen `Bitmap(width, height, 65535)` | **No** | Memory risk on ESP32-S3 heap |
| Alpha / transparency-heavy compositing | **No** in v1 | Unreliable or expensive on device |
| `FourWire` / raw SPI display drivers | **No** in `shared/` | Qualia uses `FramebufferDisplay` in `device/` only |

### vectorio

| API / pattern | Allowed | Notes |
|---------------|---------|--------|
| `vectorio.Circle` | Yes | Todo checkmarks, dots |
| `vectorio.Rectangle` | Yes | List row highlights |
| `vectorio.Polygon` | Caution | OK if simple; prefer rect/circle for v1 |
| Shapes inside `displayio.Group` | Yes | **Required pattern** — vectorio is not a separate UI stack |

**Rule:** vectorio and displayio are **complementary**. Never implement a screen only in vectorio without a `displayio.Group` root.

### Text

| API / pattern | Allowed | Notes |
|---------------|---------|--------|
| `adafruit_bitmap_font` + BDF | Yes | **Canonical** for device; use same files on desktop |
| `bitmap_label.Label` | Yes | Preferred for dynamic text (clock, timer, counter) |
| `adafruit_display_text` | Avoid in v1 | Pick one text stack; default is `bitmap_label` |
| Runtime emoji / TTF in Labels | **No** | Use pre-rendered BMP icons (§5) |

### Time and paths (abstracted)

| Concern | Rule |
|---------|------|
| Clock / timer | Use `shared/platform.py`: `now()`, `monotonic()` — host uses CPython `time`; device uses `time` and/or `rtc` |
| Asset paths | Use `asset_path("icons/todo.bmp")` — never hardcode OS paths like `./assets` vs `/assets` in app modules |

---

## 3. Forbidden in `shared/` (causes Qualia redesign)

Do **not** use these in any file under `shared/`:

- `pygame`, `tkinter`, PIL/Pillow
- Blinka-only or host-only `displayio` extensions not in [CircuitPython displayio docs](https://docs.circuitpython.org/en/latest/shared-bindings/displayio/)
- Manual pygame blit of UI (HostDisplay fallback drawing **outside** displayio)
- CPython-only modules: `pathlib` is OK if CP-compatible usage; avoid `subprocess`, `socket` unless later feature needs WiFi in `device/`
- HTML/CSS concepts implemented only on host (gradients, `transition`, web fonts)
- Live emoji or Unicode icon rendering as a substitute for assets
- Per-frame allocation of large `Bitmap` / `Group` trees (create once, update in place)

---

## 4. Memory and performance (Qualia ESP32-S3)

| Constraint | Guideline |
|------------|-----------|
| Group depth | ≤ ~3 levels for a screen; flatten where possible |
| Palettes | Reuse one palette per theme where possible; avoid unique 65535-color buffers |
| View changes | Prefer swap `root_group` or toggle `Group.hidden` (if used) over rebuild entire tree |
| GC | On device main loop: `gc.collect()` after heavy view changes (in `device/main.py`, not scattered in every label update) |
| Animation length | 10–15 frames max for horizontal slide; target ~30 FPS full refresh |
| Animation style | Stepped `Group.x` increments — no CSS easing, no pygame transforms in `shared/` |
| Refresh | Match device: don’t assume 60 FPS; design UI readable at ~30 FPS |
| Libraries on device | Prefer `.mpy` from matching bundle; sync CP + bundle versions ([02](02_circuitpython_setup.md), [05](05_arduino_and_troubleshooting.md)) |

---

## 5. Assets and fonts

| Constraint | Detail |
|------------|--------|
| Resolution | Logical layout **480×480**; design for round safe area (~24 px inset from edges) |
| Icons | BMP (or generated once into `Bitmap` at startup on desktop only if same memory profile on device) |
| Fonts | BDF on disk; same paths on CIRCUITPY root as in repo `assets/fonts/` |
| Images on device | **BMP only** on `CIRCUITPY` ([03](03_display_setup.md)) |
| Colors | RGB565 palettes; match HTML simulator colors as hex → palette entries |
| Repo layout | `assets/icons/`, `assets/fonts/` — copied verbatim to board |

---

## 6. Input abstraction

All navigation and apps depend on this shape only:

```python
# shared/input_types.py (conceptual)
class InputEvent:
    dial_delta: int       # -1, 0, or +1 per tick
    button_pressed: bool  # True on rising edge only
```

| Constraint | Detail |
|------------|--------|
| No pygame in `shared/` | Host maps slider/keys → `InputEvent` |
| No `board` in `shared/` | Device maps encoder / UP-DN / optional touch → `InputEvent` |
| Thresholds | `DIAL_SWITCH` and library scroll logic live in `navigation.py` only |
| Touch | Optional later in `device/` only; must still emit `InputEvent` or a documented extension |

---

## 7. Navigation and app behavior

| Constraint | Detail |
|------------|--------|
| States | `watch` \| `library` \| `app` — single state machine in `navigation.py` |
| Parity target | Match `DESKTOP-EMULATOR-LAUNCH/watch_app_simulator.html` flows |
| App registry | Data-only in `apps_registry.py` (name, subtitle, color, view id) |
| Per-app logic | In `shared/apps/<name>.py`; no hardware imports |
| Button actions | Timer start/stop, counter increment, default back — same rules as HTML |

---

## 8. Display setup (platform-specific only)

| Platform | Location | Responsibility |
|----------|----------|----------------|
| Desktop | `host/display_setup.py` | Blinka + pygame `DISPLAY` **or** real displayio shim; 480×480 |
| Device | `device/qualia_display.py` | `release_displays()`, IO expander init, `DotClockFramebuffer`, `FramebufferDisplay`, TL021WVC02 timings |
| Circular mask | `host/circular_mask.py` **only** | Visual clip for round bezel; **not** used on device |
| `release_displays()` | Device: **required** at boot; Host: no-op or shim |

**Never** put init sequences, `board.TFT_PINS`, or `dotclockframebuffer` in `shared/`.

---

## 9. Desktop host requirements

| Constraint | Detail |
|------------|--------|
| Primary path | Real **displayio** on desktop (Blinka shim), not pygame-drawn UI |
| HostDisplay fallback | If used, **must not** implement app UI — spike/blank only; do not build apps on fallback |
| Window | Pygame presents framebuffer; circular mask is post-process only |
| Debug UI | Sliders, badges, buttons under the watch — **`host/` only**, never in `shared/` |
| Python version | 3.10+ for dev; `shared/` syntax must be CircuitPython-compatible (no walrus-required tricks, no 3.12-only features if CP lags) |

---

## 10. Device deployment (Qualia)

| Constraint | Detail |
|------------|--------|
| Entry | `code.py` on `CIRCUITPY` imports `device/main.py` or inlines thin boot |
| CircuitPython | Use **unstable/dev** build required for Qualia ([02](02_circuitpython_setup.md)) |
| Deploy set | `device/`, `shared/`, `assets/` → board; **not** `host/` or `requirements.txt` |
| `lib/` | Install via `circup` — versions match UF2 version |
| I2C | Display init uses I2C first; deinit/reinit per [03](03_display_setup.md) before touch |
| Touch (optional) | CST826 @ `0x15` — `device/` only, library `adafruit_cst8xx` |

---

## 11. Testing gates (before merging features)

### Desktop

- [ ] Runs without ESP32 connected
- [ ] All `shared/` imports pass without `host`/`pygame`
- [ ] Manual checklist in roadmap §Testing (clock, dial, library, each app)

### Device (smoke, early and often)

- [ ] Rainbow UF2 or minimal display init shows pixels ([05](05_arduino_and_troubleshooting.md))
- [ ] One `shared` screen (e.g. library) on hardware before all five apps
- [ ] `import gc; gc.mem_free()` acceptable after loading heaviest screen
- [ ] No `ImportError` / `Incompatible .mpy file` on boot

### Portability review (per PR / feature)

- [ ] New files: correct layer (`shared` / `host` / `device`)?
- [ ] New imports in `shared/`: CP-safe only?
- [ ] New graphics: BMP/BDF/vectorio/displayio subset only?
- [ ] Animation: `Group.x` steps, not host-only effects?

---

## 12. Version and compatibility

| Item | Constraint |
|------|------------|
| CircuitPython ↔ bundle | Same major/minor; see [02](02_circuitpython_setup.md) |
| Display | TL021WVC02 — 480×480, init sequence from [03](03_display_setup.md) |
| Rev B expander | `i2c_address = 0x38` if needed — `device/qualia_display.py` only |
| BLE | Not used in CircuitPython on S3 — do not plan BLE in `shared/` |

---

## 13. Feature scope (v1 — do not expand without device plan)

| In scope | Out of scope for v1 |
|----------|---------------------|
| Watch face, app library, 5 HTML apps | WiFi / Adafruit IO |
| Dial + button navigation | BLE |
| Short slide or instant view change | Long CSS-style animations |
| BMP icons + BDF fonts | PNG/JPEG on device without conversion |
| Optional touch later via `device/` | Touch-driven UI required in `shared/` |

---

## 14. Quick reference — “Is this allowed in `shared/`?”

```
YES  → displayio.Group, TileGrid, Bitmap, Palette, bitmap_label.Label
YES  → vectorio.Circle, vectorio.Rectangle inside Groups
YES  → Group.x / Group.y animation (short)
YES  → platform.now(), platform.asset_path()
YES  → navigation + InputEvent only for input

NO   → pygame, Blinka board, dotclockframebuffer, busio for TFT init
NO   → emoji fonts, huge bitmaps, deep group trees
NO   → host debug widgets, HTML/CSS-only effects
NO   → redesign-prone: one-off desktop drawing APIs
```

---

## 15. When in doubt

1. Put it in **`shared/`** only if CircuitPython on Qualia can run it without pygame.
2. If it touches **wiring, I2C, init bytes, or encoder** → **`device/`**.
3. If it touches **slider, window, or keyboard** → **`host/`**.
4. If unsure whether Blinka supports an API → **assume device is truth**; test on board early or check [CircuitPython displayio bindings](https://docs.circuitpython.org/en/latest/shared-bindings/displayio/).

---

*Last updated: aligned with roadmap v1 and Qualia / TL021WVC02 docs 01–06.*
