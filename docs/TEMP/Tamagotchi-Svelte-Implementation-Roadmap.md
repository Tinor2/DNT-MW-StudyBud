# Tamagotchi / Goals UI Implementation Roadmap

## Goal

Build the Svelte “Tamagotchi” experience as a polished, readable companion to the LVGL app. The first milestone should let the user:

- see their current seed balance and level,
- view and edit three daily goals,
- understand their streak progress,
- read a human-friendly history of how they earned seeds,
- and preview a placeholder plant area that can later grow as the level increases.

The implementation should stay tightly aligned with the existing ESP32/LVGL system so that the web app never becomes a second source of truth.

---

## 1. Product outcome and success definition

The feature is successful when a user can open the Tamagotchi tab and immediately understand:

1. how many seeds they have,
2. how many seeds they earned today,
3. what their current level is,
4. what their daily goals are,
5. how their streaks are progressing,
6. and where their seeds came from recently.

### Definition of done

- The Svelte app shows a live summary from the device.
- Users can edit the three daily goals.
- Completion toggles are reflected correctly.
- The activity log reads clearly without needing raw reason codes.
- The UI is readable on both desktop and mobile layouts.
- The design fits the existing StudyBud visual language.

---

## 2. Architecture decision: single source of truth

### Recommended model

Use the ESP32/LVGL app as the authoritative system for all points, goals, streaks, and history.

The Svelte app should not maintain its own independent reward logic. Instead it should:

1. connect to the existing WebSocket channel,
2. ask for a fresh snapshot with the existing points request,
3. receive broadcasts for point changes,
4. normalize the payload into a clean UI-facing state object,
5. render that state in the Tamagotchi tab.

### Why this is the right approach

- It keeps the firmware and web app in sync.
- It avoids duplicate logic for streak rules and reward calculations.
- It preserves the current persistence model already used by the LVGL app.
- It makes future LVGL and Svelte parity easier to maintain.

### Files that should participate

- [main/networking/app_state.c](main/networking/app_state.c) remains the source of truth for protocol output.
- [web-app/src/App.svelte](web-app/src/App.svelte) should be the integration point for incoming messages.
- [web-app/src/lib/stores/tamagotchi.js](web-app/src/lib/stores/tamagotchi.js) should hold the normalized UI state.
- [web-app/src/lib/components](web-app/src/lib/components) should contain the UI components.

---

## 3. Data contract and message handling plan

The current firmware already emits enough information for this UI. The web app should listen for the following message types:

### Required incoming message types

- `full_sync`
  - used to initialize all tab state.
  - should populate the tamagotchi store with points, goals, streaks, and history.

- `points_sync`
  - used for incremental updates after point changes.
  - should replace the relevant slice of the tamagotchi state.

- `points_earned`
  - used for lightweight feedback after a reward is granted.
  - should optionally trigger a small toast or subtle visual confirmation.

- `goal_update`
  - used when a goal is edited or toggled.
  - should cause the UI to re-read the latest state.

### Expected payload fields

The Svelte store should normalize the following fields:

- `total`
- `today`
- `day`
- `level`
- `level_progress`
- `level_threshold`
- `water_today`
- `water_goal`
- `water_bonus`
- `bedtime_bonus`
- `bedtime`
- `goals[]`
- `streaks[]`
- `history[]`

### Suggested normalization layer

Instead of letting components read the raw websocket payload directly, the store should expose a shaped object like this:

```ts
{
  total: 0,
  today: 0,
  level: 1,
  levelProgress: 0,
  levelThreshold: 100,
  goals: [],
  streaks: [],
  history: []
}
```

### Optional protocol improvement

If the firmware later adds human-readable reason labels, the UI will become cleaner. For now, the web app can map reason codes locally into labels such as:

- water glass,
- bedtime bonus,
- breathing session,
- focus session,
- daily goal,
- streak reward,
- admin adjustment.

---

## 4. Svelte-side state model

The web app should use a dedicated store rather than scattering state across multiple components.

### Proposed store structure

Create a store in [web-app/src/lib/stores/tamagotchi.js](web-app/src/lib/stores/tamagotchi.js) with the following responsibilities:

- accept incoming payloads from the websocket layer,
- expose a normalized state object,
- provide helper functions for:
  - `setPointsState`,
  - `applyPointsUpdate`,
  - `setGoalDraft`,
  - `saveGoalEdit`,
  - `toggleGoalCompletion`,
  - `setBedtimeSettings`,
  - `setActiveSubtab`.

### Suggested internal state shape

```ts
{
  total: number,
  today: number,
  day: string,
  level: number,
  levelProgress: number,
  levelThreshold: number,
  waterToday: number,
  waterGoal: number,
  waterBonusClaimed: boolean,
  bedtimeBonusClaimed: boolean,
  bedtime: { hour: number, min: number },
  goals: Array<{
    index: number,
    label: string,
    metric: number,
    target: number,
    done: boolean
  }>,
  streaks: Array<{
    activity: string,
    days: number,
    multiplier: number
  }>,
  history: Array<{
    amount: number,
    reason: number,
    detail: number,
    day: string,
    ts: number
  }>
}
```

### Local persistence strategy

The store should keep UI-only draft state in localStorage, such as:

- the last active sub-tab,
- unsaved edits while a modal is open,
- the last selected goal metric,
- optional collapsed sections.

This should never override the device’s authoritative state.

---

## 5. UI architecture and screen structure

### Main layout

Add a new top-level tab called “Tamagotchi”.

The page should have two sub-tabs:

1. Plant
   - placeholder for now,
   - reserved for future plant artwork and growth-stage visuals.

2. Goals & Seeds
   - the primary feature for this milestone.

### Suggested component hierarchy

Create the following components in [web-app/src/lib/components](web-app/src/lib/components):

- `TamagotchiPage.svelte`
  - container for the page and its sub-tabs.

- `SeedsSummaryCard.svelte`
  - shows total seeds, today’s seeds, level, and progress bar.

- `DailyGoalsPanel.svelte`
  - renders the three daily goals.

- `GoalEditorSheet.svelte`
  - handles editing a single goal.

- `StreaksPanel.svelte`
  - displays streak chips and multipliers.

- `ActivityLogPanel.svelte`
  - shows recent seed-earning events in a readable list.

- `PlantPreview.svelte`
  - placeholder for future growth animation and plant art.

### Page sections

#### Top section

A summary card with:

- total seeds,
- seeds earned today,
- current level,
- progress to next level,
- a small helper line such as “Keep going to grow your plant”.

#### Middle section

Three goal cards arranged in a responsive grid. Each card should display:

- the goal title,
- the metric chip,
- the current target,
- completion state,
- and an edit action.

#### Bottom section

A combined area for:

- streak chips,
- the last 10–20 reward events,
- and a small empty-state message if the history is empty.

---

## 6. UI behavior and interaction design

### Goal editing flow

When the user taps a goal card:

1. open a compact editor sheet or modal,
2. show the current label and target,
3. let the user edit the label,
4. allow them to choose a metric,
5. let them set a target value,
6. let them toggle done/undone,
7. save the change back through the existing websocket command.

### Empty states

The page should never feel blank:

- if no goals exist, show a helpful message: “Add your first daily goal”.
- if no history exists, show: “No seed activity yet — complete a habit to start earning”.
- if no streak data exists, show a neutral message rather than an empty panel.

### Readability rules

- use concise labels,
- use plain-language reward descriptions,
- avoid showing raw numeric reason codes unless in a debug mode,
- keep the layout calm and not overloaded.

---

## 7. Styling plan

### Visual direction

The Tamagotchi tab should feel like a natural continuation of the existing StudyBud UI.

Use the existing palette and token system from [web-app/src/style.css](web-app/src/style.css) rather than introducing a new visual language.

### Styling rules

- use the current green family for primary actions,
- keep spacing generous,
- use rounded cards and subtle shadows,
- use color sparingly to call attention to the current status,
- ensure the layout remains readable in both light and dark mode.

### Responsive behavior

- single column on small screens,
- two-column cards on larger screens,
- keep the summary card prominent,
- let the history section scroll independently if needed.

---

## 8. Implementation phases

### Phase 1 — websocket + store integration

#### Scope

- add the tamagotchi store,
- connect it to the existing websocket message processing in [web-app/src/App.svelte](web-app/src/App.svelte),
- request points data on connect,
- normalize the incoming points payload into UI state.

#### Files

- [web-app/src/App.svelte](web-app/src/App.svelte)
- [web-app/src/lib/stores/tamagotchi.js](web-app/src/lib/stores/tamagotchi.js)

#### Acceptance criteria

- the app loads the points summary without manual refresh,
- the UI updates when the device sends new points data,
- the store exposes a stable state object that components can consume.

### Phase 2 — summary and goal list UI

#### Scope

- build the summary card,
- render the three goal cards,
- show completion state visually,
- add simple empty-state messaging.

#### Files

- [web-app/src/lib/components/TamagotchiPage.svelte](web-app/src/lib/components)
- [web-app/src/lib/components/SeedsSummaryCard.svelte](web-app/src/lib/components)
- [web-app/src/lib/components/DailyGoalsPanel.svelte](web-app/src/lib/components)

#### Acceptance criteria

- users can see all three goals immediately,
- the UI communicates done vs not done clearly,
- the layout is responsive and clean.

### Phase 3 — goal editing and save flow

#### Scope

- add the goal editor sheet,
- allow editing labels and targets,
- allow toggling done/undone,
- send the correct message to the firmware.

#### Files

- [web-app/src/lib/components/GoalEditorSheet.svelte](web-app/src/lib/components)
- [web-app/src/lib/stores/tamagotchi.js](web-app/src/lib/stores/tamagotchi.js)

#### Acceptance criteria

- editing a goal updates the UI immediately,
- save operations send the right request to the device,
- the UI remains consistent after a successful change.

### Phase 4 — streaks and history feed

#### Scope

- render streak chips with the current multiplier,
- show the last reward events in a readable list,
- translate reward reasons into friendly labels.

#### Files

- [web-app/src/lib/components/StreaksPanel.svelte](web-app/src/lib/components)
- [web-app/src/lib/components/ActivityLogPanel.svelte](web-app/src/lib/components)

#### Acceptance criteria

- the streak section is understandable without technical knowledge,
- recent activity is readable and sorted newest-first,
- reward descriptions are friendly and not raw.

### Phase 5 — plant placeholder and polish

#### Scope

- add the placeholder plant preview area,
- connect its visual state to the current level,
- finish microcopy, spacing, and transitions.

#### Files

- [web-app/src/lib/components/PlantPreview.svelte](web-app/src/lib/components)
- [web-app/src/style.css](web-app/src/style.css)

#### Acceptance criteria

- the page contains a dedicated plant area,
- the plant preview feels intentionally placed for future growth work,
- the full page feels cohesive and polished.

---

## 9. Edge cases and robustness

The implementation should handle these cases cleanly:

- no points data yet,
- missing goal labels,
- missing streak data,
- very long goal labels,
- empty history,
- a device that reconnects after being offline,
- a user editing a goal while the device sends a live update.

### Expected behavior

- the UI should not crash if a field is missing,
- it should fall back to an empty or neutral placeholder,
- it should preserve local draft input while waiting for the next sync.

---

## 10. Testing plan

### Manual testing

- connect to the device and confirm the page loads correctly,
- edit each of the three goals,
- toggle completion on and off,
- verify that point totals update as expected,
- confirm the history panel remains readable after multiple events,
- verify the layout on a phone-sized viewport.

### Future automated checks

- unit tests for the store normalization functions,
- component tests for empty states and goal editing,
- a small regression test for the reason-label mapping.

---

## 11. Risks and open questions

### Risks

- the websocket payload shape may evolve slightly,
- reason codes may need a more expressive label map,
- the plant section may need artwork later and should stay flexible.

### Open questions

- should the goal editor be a modal or a slide-over panel?
- should streaks be displayed as chips or a compact grid?
- should the plant preview be static for now or animated later?

These can be decided during implementation without blocking the first milestone.

---

## 12. Recommended implementation sequence

The safest order is:

1. wire the store and websocket bridge,
2. render the summary and goals cards,
3. add goal editing,
4. add streak and history panels,
5. add the plant placeholder and polish.

This keeps the work incremental and makes each milestone demonstrable.

---

## 13. Final recommendation

Implement the feature as a thin web UI over the existing ESP32 points system rather than creating a parallel model. That keeps the product aligned with the current app architecture and makes future parity work much easier.

In practice, that means:

- ESP32/LVGL remains the source of truth,
- websocket remains the transport,
- Svelte store becomes the normalized view layer,
- localStorage is reserved for lightweight UI draft state only.
