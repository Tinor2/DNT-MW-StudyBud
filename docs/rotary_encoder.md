# Rotary Encoder — Hardware & Software Guide

## Overview

The StudyBud board uses a standard incremental quadrature rotary encoder with a push-button switch. The encoder provides two output signals (`CLK` and `DT`) plus a button signal (`SW`), connected to the ESP32-S3 as follows:

| Signal | GPIO | Function |
|--------|------|----------|
| CLK    | 19   | Quadrature signal A |
| DT     | 20   | Quadrature signal B |
| SW     | 0    | Push-button (active-low, internal pull-up) |

---

## How the Encoder Works Physically

### Detents and Steps

The encoder dial has approximately **20 detent positions** — these are the positions where you feel a small mechanical "click" as the knob settles into place. Each detent is called a **step**.

### 40 Unique States

Critically, the encoder's electrical signals change **twice per detent step**:

1. **On-step** — the encoder rests in a detent position
2. **In-between** — the encoder is between two detent positions

Since there are ~20 detent steps, and each step has 2 electrical states, the encoder produces approximately **40 unique states** per full rotation. The `CLK` and `DT` signals form a 2-bit Gray code that cycles through these 40 states sequentially.

### Quadrature Signals

The two output signals (`CLK` and `DT`) are 90° out of phase. In one direction (e.g. clockwise):

```
State:    00  01  11  10  00  01  11  10 ...
           ↑       ↑       ↑       ↑
         detent  between detent  between
```

Every transition from one state to the next (in either direction) generates one "count" from the PCNT hardware.

---

## The Jitter Problem

Because the encoder counts **every** state change — including the in-between states — rotating the encoder by one full detent step generates **two** count increments (one for entering the in-between state, one for arriving at the next detent).

This creates a practical problem: **accidental nudges**. Consider a user trying to rotate one full step clockwise:

```
1. CCW  (in-between)   ← accidental nudge backwards
2. CW   (on-step)      ← returns to original position
3. CW   (in-between)   ← continues forward
4. CW   (on-step)      ← arrives at next detent
```

Without filtering, this produces **4 logged rotations**: one CCW, one CW, one CW, one CW. Net result is one step forward, but the intermediate jitter would cause a UI (e.g. menu list) to briefly glitch up then down — a poor user experience.

### Summary of Jitter Counts

| Physical motion | PCNT count | Net position change |
|----------------|------------|---------------------|
| 1 full step CW | +2 | +1 step |
| 1 full step CCW | -2 | -1 step |
| Slight nudge CW then back | 0 (one +1, one -1) | 0 |
| Slight nudge CCW then back | 0 (one -1, one +1) | 0 |

---

## Software Implementation

### Hardware PCNT (Current Approach)

The ESP32-S3's **Pulse Counter (PCNT)** peripheral decodes the quadrature signals entirely in hardware. This is the recommended approach — it eliminates polling, contact bounce, and missed transitions.

**Key configuration** (`main/main.cpp`):

```cpp
// Create PCNT unit
pcnt_unit_config_t unit_cfg = {};
unit_cfg.low_limit  = -32768;
unit_cfg.high_limit =  32767;
unit_cfg.clk_src   = PCNT_CLK_SRC_DEFAULT;

// Glitch filter (rejects mechanical bounce, ~10 µs max on ESP32-S3)
pcnt_glitch_filter_config_t filter_cfg = {};
filter_cfg.max_glitch_ns = 10000;  // 10 µs

// Channel: CLK = edge signal, DT = level signal
// pos-edge + DT low  → INCREASE (CW)
// pos-edge + DT high → DECREASE (CCW, level INVERSE flips direction)
// neg-edge + DT high → INCREASE (CW, level INVERSE flips direction)
// neg-edge + DT low  → DECREASE (CCW)
pcnt_channel_set_edge_action(chan, INCREASE, DECREASE);
pcnt_channel_set_level_action(chan, INVERSE, KEEP);
```

The PCNT hardware counts on **every** state change (both on-step and in-between), giving 2 counts per detent step.

### Reading the Counter

```cpp
int count = 0;
pcnt_unit_get_count(pcnt_unit, &count);
```

The count value is signed — positive for CW, negative for CCW. It accumulates continuously, so you can detect direction by comparing successive reads.

---

## Filtering Strategies

### Option 1: Ignore In-Between States (Step-Only Counting)

Divide the PCNT count by 2 (integer division) to only register full detent steps:

```cpp
int raw = 0;
pcnt_unit_get_count(pcnt_unit, &raw);
int steps = raw / 2;  // Only full steps
```

This is the simplest approach and eliminates jitter entirely. The tradeoff is reduced resolution (20 steps/rotation instead of 40).

### Option 2: Hysteresis / Debounce Window

Track the last confirmed position and only update when the count moves a minimum distance away from it:

```cpp
static int confirmed_pos = 0;
int raw = 0;
pcnt_unit_get_count(pcnt_unit, &raw);

if (abs(raw - confirmed_pos) >= 2) {  // Require 2 counts to confirm
    confirmed_pos = raw;
    // Apply position change to UI
}
```

This is more flexible than dividing by 2 — it allows sub-step resolution if needed while still rejecting small jitters.

### Option 3: Track Everything, Filter at UI Layer

Pass the raw count to the UI layer and let the UI decide:

- **Menu navigation**: Use step-only (divide by 2) to prevent glitching
- **Volume control / scrolling**: Allow raw counts for smoother, faster response
- **Precise selection**: Could use hysteresis with a threshold of 1–2 counts

---

## Recommendations

1. **For UI navigation (menus, lists, settings)**: Use Option 1 (divide by 2). Users expect one click = one item movement. Jitter is unacceptable here.

2. **For analog-style controls (volume, brightness)**: Use the raw PCNT count for responsive, fluid feel. The occasional sub-pixel jitter is imperceptible.

3. **For anything in between**: Use Option 2 with a threshold of 2 counts.

> **Note**: Before implementing any of these filtering strategies, confirm with the team which approach best suits the use case. The raw PCNT count should always be available as a fallback.
