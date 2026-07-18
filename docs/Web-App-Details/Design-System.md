# StudyBud — Design System

## Product Identity

StudyBud is a **stress management companion**, not just a study tool. The design language reflects this: calm, friendly, and approachable — never clinical or overwhelming. The desk buddy lives on your desk and helps you manage stress through timers, breathing exercises, hydration tracking, and gentle reminders.

**Design pillars:**
1. **Calm** — Muted colors, smooth transitions, no jarring elements
2. **Friendly** — Slightly rounded shapes, warm tones, pixel art personality
3. **Readable** — Functional clarity on a 480×480 round display
4. **Consistent** — Same palette across web app and display, different levels of detail

---

## Design Philosophy

```
┌─────────────────────────────────────────────────────┐
│                   TWO LAYERS                         │
├─────────────────────────────────────────────────────┤
│                                                      │
│  FUNCTIONAL LAYER (Timer, Todos, Water, Breathing,   │
│  Settings, Sedentary, etc.)                          │
│  → Clean, modern, muted pastel                      │
│  → Optimized for readability on 480×480 round LCD    │
│  → Same palette on web app, more detailed layout     │
│                                                      │
│  PERSONALITY LAYER (Idle backgrounds, Tamagotchi,    │
│  pixel art icons, future mascot)                     │
│  → Pixel art style                                   │
│  → Appears during idle, menu navigation, and         │
│    specific features (Tamagotchi)                    │
│  → Gives the desk buddy character without cluttering │
│    the functional UI                                 │
│                                                      │
└─────────────────────────────────────────────────────┘
```

The pixel art isn't the theme — it's the **personality**. The functional UI stays clean and readable. The idle screen and specific features are where the desk buddy "comes alive."

---

## Color Palette System

### Architecture

Colors are defined in **two single files** — one for each UI. Changing the palette means editing one file:

| UI | File | Format |
|---|---|---|
| Web App | `src/lib/theme.css` | CSS custom properties (`--color-*`) |
| LVGL Display | `main/display/lv_theme.h` | `#define` constants (`LV_COLOR_*`) |

Both files use the same color names and values, just different syntax.

### Default Palette: Lavender & Sage

```css
/* src/lib/theme.css — Web App */

:root {
  /* === PRIMARY === */
  --color-primary:        #B8A9C9;  /* Soft lavender — main accent, active states */
  --color-primary-light:  #D4C8E2;  /* Lighter lavender — hover states, highlights */
  --color-primary-dark:   #8B7BA3;  /* Deeper lavender — pressed states, emphasis */

  /* === SECONDARY === */
  --color-secondary:      #A8C5A0;  /* Sage green — secondary accent, success states */
  --color-secondary-light:#C5DDBF;  /* Lighter sage — tags, badges */
  --color-secondary-dark: #7A9E72;  /* Deeper sage — active success */

  /* === NEUTRALS === */
  --color-bg:             #F5F0F7;  /* Warm off-white — main background */
  --color-bg-card:        #FFFFFF;  /* Pure white — cards, panels */
  --color-bg-elevated:    #EDE8F0;  /* Slightly darker — modals, dropdowns */
  --color-surface:        #E8E2EC;  /* Muted lavender-grey — input backgrounds */
  --color-border:         #D4CDE0;  /* Subtle border color */
  --color-border-light:   #E8E2EC;  /* Very subtle dividers */

  /* === TEXT === */
  --color-text:           #3D3547;  /* Dark purple-grey — primary text */
  --color-text-secondary: #7A6F8A;  /* Muted — secondary labels, placeholders */
  --color-text-muted:     #A99BB8;  /* Very muted — disabled, timestamps */
  --color-text-inverse:   #FFFFFF;  /* Text on dark/colored backgrounds */

  /* === STATUS === */
  --color-success:        #A8C5A0;  /* Same as secondary */
  --color-warning:        #E8C97A;  /* Warm amber — warnings, sedentary alerts */
  --color-error:          #D4847A;  /* Muted coral — errors, deletions */
  --color-info:           #8BB5C9;  /* Soft blue — informational */

  /* === FEATURE ACCENTS (optional — for feature-specific highlights) === */
  --color-timer:          #E8C97A;  /* Warm amber — timer phases */
  --color-timer-break:    #8BB5C9;  /* Soft blue — break phases */
  --color-water:          #8BB5C9;  /* Soft blue — water tracker */
  --color-breathing:      #B8A9C9;  /* Lavender — breathing exercises */
  --color-tamagotchi:     #A8C5A0;  /* Sage — tamagotchi feature */

  /* === SHADOWS === */
  --shadow-sm:   0 1px 3px rgba(61, 53, 71, 0.06);
  --shadow-md:   0 4px 12px rgba(61, 53, 71, 0.08);
  --shadow-lg:   0 8px 24px rgba(61, 53, 71, 0.12);

  /* === TRANSITIONS === */
  --transition-fast:   150ms ease;
  --transition-normal: 250ms ease;
  --transition-slow:   400ms ease;
}
```

### LVGL Equivalent

```c
/* main/display/lv_theme.h — Display */

#ifndef LV_THEME_H
#define LV_THEME_H

#include "lvgl.h"

/* === PRIMARY === */
#define LV_COLOR_PRIMARY        lv_color_hex(0xB8A9C9)
#define LV_COLOR_PRIMARY_LIGHT  lv_color_hex(0xD4C8E2)
#define LV_COLOR_PRIMARY_DARK   lv_color_hex(0x8B7BA3)

/* === SECONDARY === */
#define LV_COLOR_SECONDARY      lv_color_hex(0xA8C5A0)
#define LV_COLOR_SECONDARY_LIGHT lv_color_hex(0xC5DDBF)
#define LV_COLOR_SECONDARY_DARK lv_color_hex(0x7A9E72)

/* === BACKGROUNDS === */
#define LV_COLOR_BG             lv_color_hex(0xF5F0F7)
#define LV_COLOR_BG_CARD        lv_color_hex(0xFFFFFF)
#define LV_COLOR_SURFACE        lv_color_hex(0xE8E2EC)

/* === TEXT === */
#define LV_COLOR_TEXT           lv_color_hex(0x3D3547)
#define LV_COLOR_TEXT_SECONDARY lv_color_hex(0x7A6F8A)
#define LV_COLOR_TEXT_MUTED     lv_color_hex(0xA99BB8)

/* === STATUS === */
#define LV_COLOR_SUCCESS        lv_color_hex(0xA8C5A0)
#define LV_COLOR_WARNING        lv_color_hex(0xE8C97A)
#define LV_COLOR_ERROR          lv_color_hex(0xD4847A)
#define LV_COLOR_INFO           lv_color_hex(0x8BB5C9)

/* === FEATURE ACCENTS === */
#define LV_COLOR_TIMER          lv_color_hex(0xE8C97A)
#define LV_COLOR_TIMER_BREAK    lv_color_hex(0x8BB5C9)
#define LV_COLOR_WATER          lv_color_hex(0x8BB5C9)
#define LV_COLOR_BREATHING      lv_color_hex(0xB8A9C9)

/* === OPACITY === */
#define LV_OPACITY_BG           LV_OPA_10    /* 10% — very subtle backgrounds */
#define LV_OPACITY_DISABLED     LV_OPA_40    /* 40% — disabled elements */
#define LV_OPACITY_MUTED        LV_OPA_60    /* 60% — completed/faded tasks */

#endif /* LV_THEME_H */
```

### How to Change the Palette

1. Open `src/lib/theme.css` (web app) or `main/display/lv_theme.h` (display)
2. Update the hex values
3. Both files use the same variable names — keep them in sync
4. Rebuild both UIs

The entire theme changes by editing ~30 color values in two files.

---

## Typography

### Font Selection

The user wants a clean, slightly rounded sans-serif — friendly without being childish. Recommended options:

| Font | Style | Best For | LVGL Support |
|---|---|---|---|
| **Nunito** | Rounded terminals, warm | Display + Web | Excellent via `lv_font_conv` |
| **Quicksand** | Geometric, rounded | Display + Web | Good via `lv_font_conv` |
| **Poppins** | Geometric, clean | Web primarily | Good via `lv_font_conv` |
| **Inter** | Neutral, highly legible | Web primarily | Good via `lv_font_conv` |

**Recommendation:** Use **Nunito** across both UIs. It's friendly, highly legible at small sizes, and has excellent LVGL compatibility.

### Font Integration

#### LVGL (Display)

LVGL supports custom TrueType fonts via the `lv_font_conv` tool:

```bash
# Install the converter
npm install -g lv_font_conv

# Generate font at different sizes
lv_font_conv --bpp 4 --size 12 --font Nunito-Regular.ttf -o font_12.c --format lvgl
lv_font_conv --bpp 4 --size 16 --font Nunito-Regular.ttf -o font_16.c --format lvgl
lv_font_conv --bpp 4 --size 20 --font Nunito-SemiBold.ttf -o font_20_bold.c --format lvgl
lv_font_conv --bpp 4 --size 28 --font Nunito-Bold.ttf -o font_28.c --format lvgl
lv_font_conv --bpp 4 --size 36 --font Nunito-Bold.ttf -o font_36.c --format lvgl
```

Include the generated `.c` files in your LVGL project and register them:

```c
#include "font_12.c"
#include "font_16.c"
#include "font_20_bold.c"
#include "font_28.c"
#include "font_36.c"

// In LVGL init:
lv_theme_t *theme = lv_theme_default_init(
    disp,
    LV_COLOR_PRIMARY,    // Lavender
    LV_COLOR_SECONDARY,  // Sage
    true,                // Dark mode? false for light
    &font_16             // Default font
);
```

#### Web App (Svelte)

Use Google Fonts via `@import` in `theme.css`:

```css
@import url('https://fonts.googleapis.com/css2?family=Nunito:wght@400;600;700&display=swap');

:root {
  --font-primary: 'Nunito', sans-serif;
  --font-mono: 'JetBrains Mono', monospace;  /* For timer digits */
}

body {
  font-family: var(--font-primary);
}
```

For offline/PWA capability, self-host the font files in `static/fonts/` and reference them with `@font-face`.

### Font Scale

| Context | LVGL Size | Web App Size | Weight |
|---|---|---|---|
| Display: Large digits (timer) | 36px | 48-64px | Bold |
| Display: Headings | 20px | 24px | SemiBold |
| Display: Body text | 16px | 16px | Regular |
| Display: Small labels | 12px | 12-14px | Regular |
| Web: Page titles | — | 28-32px | Bold |
| Web: Section headings | — | 20-24px | SemiBold |
| Web: Body text | — | 16px | Regular |
| Web: Captions | — | 12-14px | Regular |

---

## Icon System

### Menu Wheel Icons

The menu wheel uses **pixel art icons** — custom-made, 48×48 pixels each. This is where the personality layer shines.

**Icon specifications:**

| Property | Value |
|---|---|
| Canvas size | 48×48 pixels |
| Color depth | Indexed palette (16 colors max) |
| Format | PNG (for web), raw bitmap (for LVGL) |
| Style | Pixel art, 1-2px outlines, muted palette |
| Transparent background | Yes |

**Feature icons:**

| Feature | Icon Concept | Primary Color |
|---|---|---|
| Home | Small house / clock face | Lavender |
| Timer | Hourglass / stopwatch | Amber |
| Todos | Checklist / clipboard | Sage |
| Water | Water droplet / cup | Blue |
| Breathing | Lung / wind / circle | Lavender |
| Sedentary | Person sitting / chair | Amber |
| Tamagotchi | Small creature / egg | Sage |
| Idle | Landscape / image frame | Lavender |
| Settings | Gear / sliders | Muted grey |
| Notifications | Bell / inbox | Blue |

### Creating Pixel Art Icons

Tools for creating 48×48 pixel art:
- **Aseprite** — Industry standard, supports animation
- **Piskel** — Free, browser-based
- **Lospec Pixel Editor** — Free, online

Guidelines:
- Use the muted palette (lavender, sage, amber, blue) — not bright primaries
- Keep outlines to 1-2 pixels
- Test at actual display size (48×48 on 480×480 screen = ~10% of screen width)
- Ensure icons are recognizable at a glance

### Other Icons (Non-Menu)

For UI elements like checkboxes, arrows, close buttons, use LVGL's built-in icon font or simple geometric shapes drawn with LVGL's canvas. These don't need to be pixel art.

---

## Spacing & Layout

### Web App

Use a consistent spacing scale based on 4px increments:

```css
:root {
  --space-1:  4px;
  --space-2:  8px;
  --space-3:  12px;
  --space-4:  16px;
  --space-5:  20px;
  --space-6:  24px;
  --space-8:  32px;
  --space-10: 40px;
  --space-12: 48px;
  --space-16: 64px;

  --radius-sm: 6px;
  --radius-md: 10px;
  --radius-lg: 16px;
  --radius-full: 9999px;  /* Pills, circular buttons */
}
```

### LVGL Display

On a 480×480 round display, the usable area is a circle with diameter 480px. The corners are clipped. Key layout rules:

- **Safe zone:** Keep content within a 400px diameter circle (40px margin from edges)
- **Center alignment:** Most content should be center-aligned on the round display
- **Vertical stacking:** Use vertical layouts with 8-16px spacing between elements
- **Font sizes:** Minimum 12px for readability on the physical display

---

## Animation Guidelines

### Display (LVGL)

| Transition | Duration | Easing |
|---|---|---|
| Screen change | 300ms | `LV_SCR_LOAD_ANIM_FADE_ON` |
| Menu wheel scroll | 200ms | Ease-out (fast start, gentle stop) |
| Radial list scroll | 200ms | Ease-out |
| Notification toast | 400ms fade in, 300ms fade out | Linear |
| Idle mode entry | 800ms | Slow fade |
| Timer progress | Continuous | Linear (smooth ring update) |

### Web App

| Transition | Duration | Easing |
|---|---|---|
| Page navigation | 250ms | `ease` |
| Modal open/close | 200ms | `ease-out` |
| Hover states | 150ms | `ease` |
| Button press | 100ms | `ease-in` |
| List item reorder | 300ms | `spring` (Svelte transition) |
| Completion animation | 400ms | `ease-out` (strike-through + fade) |

---

## Audio Guidelines

The speaker is a **PWM buzzer** (GPIO 20, LEDC-controlled). It can produce **frequency tones at different durations** — not pre-recorded audio or WAV files. All sounds are simple frequency patterns that are short, gentle, and non-jarring.

### Hardware Limitation

The PWM buzzer can only produce:
- A single frequency tone at a time (e.g., 800Hz, 1200Hz, 2000Hz)
- Duration control (50ms for clicks, 200ms for alerts, 500ms for chimes)
- Silence between notes

It **cannot** play: WAV files, MP3s, complex melodies, or multi-instrument sounds. All sound design must be achievable with simple frequency tones.

### Sound Events

| Event | Sound Description | Frequency Range | Duration |
|---|---|---|---|
| Timer complete | 3-note ascending chime | 800Hz → 1200Hz → 1600Hz | ~800ms total |
| Break ready | Single gentle tone | 1000Hz | ~300ms |
| Task complete | Soft click | 1500Hz | ~50ms |
| Water reminder | Two-tone water drop | 2000Hz → 1500Hz | ~300ms |
| Breathing cue | Soft tone (inhale/exhale) | 600Hz (inhale) / 400Hz (exhale) | Per cue |
| Sedentary alert | Double tap | 1200Hz | 2x 100ms |
| Menu navigation | Ultra-soft click | 2000Hz | ~30ms |
| Idle wake | Quick ascending tone | 800Hz → 1200Hz | ~200ms |

### Sound Design Principles

1. **Never startle** — All sounds should be easy to ignore
2. **Consistent volume** — No sudden loud sounds
3. **Distinct but subtle** — Each event has a unique sound, but they share a tonal family
4. **Respect focus** — During timer sessions, only play timer-complete sounds. Other sounds are suppressed or queued.
5. **Volume control** — User can adjust via settings (0-100%). Default to 40%.

---

## Responsive Behavior

### Web App Breakpoints

| Breakpoint | Layout |
|---|---|
| < 640px (mobile) | Single column, hamburger nav, full-width cards |
| 640-1024px (tablet) | Single column, collapsible sidebar |
| > 1024px (desktop) | Sidebar + content area |

### Display Constraints

The display is always 480×480. No responsive breakpoints — but the radial menu and circular layouts inherently handle the round form factor.

---

## Accessibility

### Color Contrast

All text/background combinations must meet **WCAG AA** (4.5:1 contrast ratio):

| Text Color | Background | Ratio | Pass? |
|---|---|---|---|
| `#3D3547` on `#F5F0F7` | Dark on light | 7.2:1 | Yes |
| `#7A6F8A` on `#F5F0F7` | Muted on light | 4.1:1 | Borderline — use for large text only |
| `#FFFFFF` on `#B8A9C9` | White on lavender | 4.6:1 | Yes |
| `#FFFFFF` on `#8B7BA3` | White on dark lavender | 6.8:1 | Yes |

### Minimum Touch Targets

- Web app: 44×44px minimum (WCAG)
- Display: Encoder-based navigation (no touch targets needed)
