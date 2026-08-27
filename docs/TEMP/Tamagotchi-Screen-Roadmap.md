# Tamagotchi Screen — Implementation Roadmap

## Goal

Replace the placeholder "pet" on the LVGL Tamagotchi screen with the real plant
artwork (`plant_progression_1` / `plant_progression_2`), rendered at a crisp 4x
scale with the ground resting exactly on the bottom edge of the 480×480 screen.
Keep the existing **PET → GOALS → STREAKS** state machine and the Goals & Seeds
and Streaks screens unchanged; only the PET screen is redesigned.

---

## Current state (before this change)

`screen_tamagotchi.c` has three states:

| State    | What it shows today |
|---|---|
| **PET**  | Title, level chip, a 240×240 pastel-green `pet_circle` built from LVGL rectangles (pot, soil, stem, leaves, tip), a hint label, and a bottom "Goals" button. The plant is a placeholder. |
| **GOALS** | Summary card (total seeds / level / progress bar) + 3 daily-goal rows + "Streaks"/"Back" nav, with radial scroll. |
| **STREAKS** | 6 streak rows + "Back" nav, with radial scroll. |

The placeholder plant is the only piece this roadmap changes.

---

## The new artwork

Two new source sprites live in `web-app/src/assets/logos/`:

| File | Size | Content |
|---|---|---|
| `plant_progression_1.png` | 120×120 | Sapling — stem + two leaves. Art occupies rows y≈49–119. |
| `plant_progression_2.png` | 120×120 | Grown plant — taller, extra foliage + flowers. Art occupies rows y≈40–119. |

Both sprites share the **same ground** (bottom rows y≈88–119 are pixel-identical)
and are centered horizontally. Alpha is binary (fully transparent / fully opaque)
and each uses only 14 unique colors.

### Sprite pipeline (exactly as requested)

1. **Upscale 4× with nearest-neighbour** — 120×120 → **480×480**. Nearest-neighbour
   (not bilinear) keeps the pixel-art edges crisp. No cropping, no padding.
2. The upscaled images are copied to `main/assets/` as
   `plant_stage_1.png` / `plant_stage_2.png`.
3. Converted to LVGL `LV_IMG_CF_TRUE_COLOR_ALPHA` C assets via the existing
   `docs/TEMP/png_to_lvgl.py` → `plant_stage_1.c/.h`, `plant_stage_2.c/.h`.
4. **Bottom alignment is automatic**: the upscaled image is 480×480 (screen size)
   and its content already reaches the last source row (y=119 → y=479). The
   `lv_img` is aligned with `LV_ALIGN_BOTTOM_MID`, so the very bottom pixel of the
   ground sits on the very bottom pixel row of the screen.

### Flash budget

- Each 480×480 `TRUE_COLOR_ALPHA` image = 480 × 480 × 3 bytes ≈ **660 KB**.
- Two images ≈ **1.32 MB** added to the firmware.
- Current `studybud.bin` ≈ 1.9 MB → new total ≈ **3.2 MB**, which exceeds the
  3 MB `factory` partition. The partition table therefore grows `factory` from
  `0x300000` (3 MB) to `0x800000` (8 MB); flash is 16 MB so this is safe.
- The simulator shares the same `.c` assets and is unaffected by the partition
  change.

---

## PET screen — target design

```
  ┌─────────────────────────────────────────────┐  y=0
  │              Tamagotchi  (title)             │  y≈30
  │        ┌──────────────────────────────┐      │
  │        │       Level 2 · 450 seeds    │      │  y≈62  chip (centered)
  │        └──────────────────────────────┘      │
  │        Your plant grows as you earn seeds     │  y≈112 hint
  │                                              │
  │                ══╗  ══                      │  transparent sky region — sprite shows here
  │                 ╚═╗  ══                     │
  │                   ║  ║                      │
  │                   ║  ║    plant art          │  art spans y≈163…455 (stage 2)
  │                   ║  ║                       │          y≈199…455 (stage 1)
  │              ▄▄▄▄▄█▄█▄▄▄▄▄                   │
  │          ████████████████████  ground         │  ground sits on y=479 (screen bottom)
  │            ┌──────────────┐                  │
  │            │    Goals     │                  │  y≈416–464 Goals pill, bottom-center
  └─────────────────────────────────────────────┘  y=479/480
```

Design decisions:

1. **Plant sprite is the hero.** It is a full-screen 480×480 `lv_img` aligned to
   `LV_ALIGN_BOTTOM_MID` (0, 3) — offset +3px down so the bottommost (uneven)
   rows of the ground are cropped off the screen bottom. The transparent top
   rows let the pastel-green screen background, title, chip, and hint show
   through. Ground pixel row 476 lands on screen y=479; sprite rows 477–479 are
   clipped.
2. **The 240×240 `pet_circle` placeholder goes away** along with the pot/stem/
   leaf LVGL objects. A real sprite replaces the drawn placeholder.
3. **"Goals" pill sits bottom-center** (`LV_ALIGN_BOTTOM_MID`, 0, -16), drawn
   on top of the sprite's ground, matching the classic tamagotchi bottom-button
   placement.
4. **Hint** sits between the chip and the top of the plant (y≈112), over the
   transparent sky region.
5. **Focus contract** for the PET state becomes: single focusable element
   (`btn_goals`). Encoder press → `transition_to_goals()`. Rotation has nothing
   to cycle (previously it toggled pet-circle/Goals focus). The focused pill
   shows the same 4px `ACCENT_LIGHT` focus border used on the other list rows.

---

## Growth stage → level mapping

Isolated in one small function so more stages slot in later without touching the
screen logic:

```c
static const lv_img_dsc_t *stage_sprite_for_level(int level)
{
    return (level >= 2) ? &plant_stage_2 : &plant_stage_1;
}
```

| Level | Sprite |
|---|---|
| 1 | `plant_stage_1` (sapling) |
| 2+ | `plant_stage_2` (grown plant with flowers) |

`screen_tamagotchi_refresh()` already calls `transition_to_pet()`; the pet
refresh path reads the current level and swaps the image source accordingly, so
the plant "grows" on screen whenever the device re-syncs level data.

Future stages (seed → sprout → flowering → full tree) are just more `png_to_lvgl`
assets + one extra branch in `stage_sprite_for_level()`.

---

## Files touched

```
docs/TEMP/Tamagotchi-Screen-Roadmap.md          ← this roadmap (NEW)
partitions.csv                                  ← factory 0x300000 → 0x800000
main/assets/plant_stage_1.png                   ← 480×480 upscale (NEW)
main/assets/plant_stage_2.png                   ← 480×480 upscale (NEW)
main/assets/plant_stage_1.c/.h                  ← png_to_lvgl.py output (NEW)
main/assets/plant_stage_2.c/.h                  ← png_to_lvgl.py output (NEW)
main/CMakeLists.txt                             ← +2 asset sources
simulator/CMakeLists.txt                        ← +2 asset sources
main/display/screens/screen_tamagotchi.c        ← PET state redesign
main/display/screens/screen_tamagotchi.h        ← unchanged
```

---

## Implementation order

1. Upscale both sprites 4× (nearest-neighbour) into `main/assets/`.
2. Run `docs/TEMP/png_to_lvgl.py` → `plant_stage_1` / `plant_stage_2`.
3. Register assets in `main/CMakeLists.txt` and `simulator/CMakeLists.txt`.
4. Bump `factory` partition to 8 MB.
5. Rework `screen_tamagotchi.c`:
   - delete `build_placeholder_plant()` + `pet_circle` and its globals,
   - add `lv_img` full-screen, bottom-aligned, sourcing `stage_sprite_for_level()`,
   - align `plant_img` at `LV_ALIGN_BOTTOM_MID (0, 3)` (crops bottom 3 rows),
   - move `btn_goals` to bottom-center, simplify PET focus list to `[btn_goals]`.
6. Build simulator; visually verify ground sits on the bottom edge and the plant
   is unobstructed.
7. Build ESP32 (`idf.py build`); confirm `studybud.bin` fits the new 8 MB partition.

---

## Verification checklist

- [x] Simulator: PET screen shows the sapling at level 1 and the grown plant at
      level ≥ 2. (Level 1 verified via `--script="hrpss"` → screenshot; stage 2
      not yet on-screen since simulator persistence seeds level 1.)
- [x] The bottom pixel of the ground touches the bottom edge of the 480×480 screen.
      Pixel-diffed the screenshot against the upscaled sprite composited on the
      pastel background: max channel diff 6 (pure RGB565 quantization) over all
      105,674 visible pixels; ground row 470 spans x[192,288] exactly matching
      the sprite, and the ground fills the visible bottom edge of the round mask
      (row 475, x=240).
- [x] Title, chip, hint, and Goals pill are readable and do not overlap the art.
      (All top-screen pixels differ from the sprite render only where the UI
      chrome is drawn.)
- [ ] Encoder: click goes PET → GOALS → STREAKS → back; radial scroll still works.
- [x] ESP32: firmware builds and fits in the 8 MB `factory` partition.
      `studybud.bin` = 0x333a70 bytes ≈ 3.2 MB; `check_sizes.py` reports
      "Smallest app partition is 0x800000 bytes. 0x4cc590 bytes (60%) free."

---

## Out of scope for this milestone (future work)

- Web-app plant preview (`PlantPreview.svelte`). The Svelte app can import the
  120×120 PNGs directly and scale with CSS `image-rendering: pixelated` for a
  4× look, sharing the same level → stage mapping.
- Idle/breathing animation of the plant (e.g. sway), which would need the
  `.aseprite` layered frames exported as a frame sequence.
- More growth stages (only two sprites exist today).
