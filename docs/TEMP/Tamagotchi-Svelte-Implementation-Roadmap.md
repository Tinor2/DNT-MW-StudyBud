# Tamagotchi / Goals UI Implementation Roadmap

## Goal

Build the Svelte “Tamagotchi” experience so it shows:

- a seed/points summary,
- daily goal cards,
- streak chips,
- a human-friendly activity log,
- and a future plant preview area.

The implementation should use the ESP32/LVGL app as the source of truth, while keeping the Svelte UI responsive and easy to extend.

---

## 1. Data flow decision

### Short answer

No major protocol rewrite is needed.

The firmware already exposes the right data through the existing WebSocket flow:

- [main/networking/app_state.c](main/networking/app_state.c) already broadcasts `points_sync`
- [main/networking/app_state.c](main/networking/app_state.c) already includes a `points` block inside `full_sync`
- [main/networking/app_state.c](main/networking/app_state.c) already handles goal edits, bedtime settings, and admin actions

### What the Svelte app should do

The Svelte app should:

1. connect to the existing websocket stream,
2. request a fresh snapshot on connect with `get_points`,
3. listen for `points_sync` and `points_earned`,
4. normalize the payload into a single tamagotchi state object,
5. render that state in the UI.

### Recommended protocol tweak (optional but useful)

The current payload is sufficient, but the UI would be cleaner if the device also sends a small human-readable label for each history reason, for example:

- `reason_label: "Water glass"`
- `reason_label: "Sleep tracked"`

This is optional. The first implementation can map numeric reason codes locally in the web app.

---

## 2. New Svelte-side state structure

We do not need a new database on the device or a separate backend store. The web app should use a dedicated Svelte store plus local persistence for draft UI state.

### Suggested state shape

```ts
interface TamagotchiState {
  total: number;
  today: number;
  day: string;
  level: number;
  levelProgress: number;
  levelThreshold: number;

  waterToday: number;
  waterGoal: number;
  waterBonusClaimed: boolean;
  bedtimeBonusClaimed: boolean;
  bedtime: { hour: number; min: number };

  goals: Array<{
    index: number;
    label: string;
    metric: number;
    target: number;
    done: boolean;
  }>;

  streaks: Array<{
    activity: string;
    days: number;
    multiplier: number;
  }>;

  history: Array<{
    amount: number;
    reason: number;
    detail: number;
    day: string;
    ts: number;
  }>;
}
```

### Where this should live

Create a dedicated store such as:

- [web-app/src/lib/stores/tamagotchi.js](web-app/src/lib/stores/tamagotchi.js)

This store should:

- receive updates from websocket messages,
- expose helpers like `setPointsState`, `applyPointsUpdate`, `setGoalDraft`, `toggleGoal`, `setBedtime`,
- optionally persist draft goal text to localStorage so editing is not lost on refresh.

### What does not need a new database

- No new SD-card schema is required on the ESP32.
- No separate server-side database is required for the first pass.
- The LVGL app already persists points, goals, and history through the existing persistence flow.

---

## 3. UI architecture

### Main page structure

Add a new top-level tab on the Svelte app called “Tamagotchi”.

The page should contain two sub-tabs:

1. Plant
   - placeholder for now,
   - later will show a growing plant and level progression.

2. Goals & Seeds
   - the main feature for this milestone,
   - displays points, goals, streaks, and activity history.

### Suggested component split

Create these components under [web-app/src/lib/components](web-app/src/lib/components):

- `TamagotchiPage.svelte`
- `SeedsSummaryCard.svelte`
- `DailyGoalsPanel.svelte`
- `StreaksPanel.svelte`
- `ActivityLogPanel.svelte`
- `GoalEditorSheet.svelte`
- `PlantPreview.svelte` (placeholder for future work)

### UI layout

#### Top section

- large summary card with:
  - total seeds,
  - seeds earned today,
  - current level,
  - progress bar to next level.

#### Middle section

- daily goals grid with 3 cards,
- each card shows:
  - label,
  - metric chip,
  - target value,
  - done state,
  - edit button.

#### Bottom section

- streak chips row,
- then a scrollable activity log.

### Interaction model

- Clicking a goal card opens an editor.
- The editor should allow:
  - changing the goal text,
  - changing the metric,
  - changing the target,
  - toggling completion.
- The activity log should be read-only and sorted newest-first.

---

## 4. UI styling plan

### Visual direction

Use the app’s existing green palette and the same tab accent colors as the rest of the Svelte app.

### Design rules

- Use the existing theme tokens from [web-app/src/style.css](web-app/src/style.css)
- Keep the screen visually calm and readable, not overloaded
- Use cards with rounded corners, subtle shadows, and mild hover states
- Use clear iconography from the existing logo set in [web-app/src/assets/logos](web-app/src/assets/logos)

### Color behavior

The tamagotchi tab should inherit the same accent logic used by the other tabs, so the tab color can be driven from the current app theme or logo palette.

### Mobile-first behavior

- single-column layout on narrow screens,
- two-column layout on larger screens,
- keep the goals cards compact and touch-friendly.

---

## 5. Implementation order

### Phase 1 — state wiring

- add a tamagotchi store,
- connect it to websocket messages in [web-app/src/App.svelte](web-app/src/App.svelte),
- make sure `full_sync` and `points_sync` populate the new state,
- make sure `get_points` is requested on connect.

Acceptance criteria:

- the app shows total seeds, today’s seeds, and level immediately after connect,
- updates appear when the device broadcasts new points.

### Phase 2 — goals UI

- build the summary card,
- build the 3 goal cards,
- support editing labels/metrics/targets,
- support toggling completion.

Acceptance criteria:

- users can view and edit all three goals,
- completion updates reflect the current backend state.

### Phase 3 — streaks and history

- render streak chips using the `streaks` payload,
- render the history feed with readable labels,
- display a friendly explanation for each reward event.

Acceptance criteria:

- the UI shows today’s progress and streak summaries,
- history events are understandable without reading raw reason codes.

### Phase 4 — plant placeholder

- add the plant preview section,
- keep it visually separate so the later growth-stage work can slot in cleanly.

Acceptance criteria:

- the page has a dedicated plant area ready for later art and growth animation.

---

## 6. Recommended local UI persistence

The web app should keep its own local draft state for:

- in-progress goal edits,
- selected sub-tab,
- maybe collapsed/expanded sections.

This should be stored in `localStorage` and should not override the firmware’s authoritative state.

---

## 7. Final recommendation

Implement this as a thin Svelte view over the existing ESP32 points system rather than creating a second parallel state model.

That keeps the architecture simple:

- ESP32/LVGL = source of truth,
- websocket = transport,
- Svelte store = normalized UI state,
- localStorage = UI-only draft persistence.

This is the cleanest way to avoid duplicating logic and to keep the Svelte app aligned with the LVGL app.
