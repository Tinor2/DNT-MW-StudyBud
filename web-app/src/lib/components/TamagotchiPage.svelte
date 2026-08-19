<script>
  import { onMount } from 'svelte';
  import { tamagotchiStore } from '../stores/tamagotchi.js';
  import { goalsStore, updateGoal, toggleGoal, checkRollover } from '../stores/goals.js';
  import tamagotchiLogo from '../../assets/logos/tamagotchi.png';
  import timerLogo from '../../assets/logos/timer.png';
  import waterLogo from '../../assets/logos/water.png';
  import breathingLogo from '../../assets/logos/breathing.png';
  import sleepingLogo from '../../assets/logos/sleeping_logo.png';
  import InfoIcon from './InfoIcon.svelte';
  import PlantPreview from './PlantPreview.svelte';
  import * as insights from '../insights.js';

  export let send = () => {};

  const SUBTAB_KEY = 'studybud_tamagotchi_subtab';
  let subtab = 'plant';
  try {
    const saved = localStorage.getItem(SUBTAB_KEY);
    if (saved === 'plant' || saved === 'goals') subtab = saved;
  } catch {}

  function setSubtab(tab) {
    subtab = tab;
    try { localStorage.setItem(SUBTAB_KEY, tab); } catch {}
  }

  let selectedGoal = null;
  let draftLabel = '';
  let draftDone = false;
  let rowEls = {};
  let bursts = [];

  const FIREWORK_COLORS = ['#FF6B6B', '#FFD166', '#06D6A0', '#118AB2', '#EF476F', '#F78C6B', '#F8D26A', '#7BD389', '#FFFFFF'];

  $: state = $tamagotchiStore;
  $: goals = $goalsStore.goals;
  $: completedGoals = goals.filter(goal => goal.done).length;
  $: progressPercent = state.levelThreshold > 0 ? Math.min(100, Math.round((state.levelProgress / state.levelThreshold) * 100)) : 0;
  $: longestStreak = state.streaks.reduce((m, s) => Math.max(m, s.days || 0), 0);

  onMount(() => {
    checkRollover();
    const id = setInterval(() => checkRollover(), 60000);
    return () => clearInterval(id);
  });

  function openGoalEditor(goal) {
    selectedGoal = goal;
    draftLabel = goal.label;
    draftDone = goal.done;
  }

  function closeEditor() {
    selectedGoal = null;
  }

  function toggleGoalFromList(index, event) {
    const { points } = toggleGoal(index);
    if (points > 0) {
      spawnBurstForGoal(index, event);
      send('points_admin', { action: 'add', amount: points });
    }
  }

  function spawnBurstForGoal(index, event) {
    const rowEl = event?.currentTarget?.closest('.goal-row') || rowEls[index];
    const rect = rowEl?.getBoundingClientRect();
    const x = rect ? rect.left + rect.width / 2 : window.innerWidth / 2;
    const y = rect ? rect.top + rect.height / 2 : window.innerHeight / 2;
    spawnBurst(x, y);
  }

  function spawnBurst(x, y) {
    const id = Math.random().toString(36).slice(2);
    const count = 16 + Math.floor(Math.random() * 8);
    const particles = Array.from({ length: count }, () => {
      const angle = Math.random() * Math.PI * 2;
      const dist = 70 + Math.random() * 120;
      return {
        tx: Math.cos(angle) * dist,
        ty: Math.sin(angle) * dist,
        size: 5 + Math.random() * 7,
        color: FIREWORK_COLORS[Math.floor(Math.random() * FIREWORK_COLORS.length)],
        rot: Math.random() * 360,
        delay: Math.random() * 0.04,
        spark: Math.random() > 0.4,
      };
    });
    bursts = [...bursts, { id, x, y, particles }];
    setTimeout(() => {
      bursts = bursts.filter(b => b.id !== id);
    }, 1100);
  }

  function saveGoal() {
    if (!selectedGoal) return;
    const index = selectedGoal.index;
    const label = draftLabel.trim() || `Goal ${index + 1}`;

    updateGoal(index, { label });
    send('goal_set', { index, label });

    if (draftDone !== selectedGoal.done) {
      toggleGoalFromList(index, null);
    }
    closeEditor();
  }

  function describeEvent(event) {
    switch (event.reason) {
      case 1: return (event.amount || 0) < 0 ? 'Todo undone' : 'Todo completed';
      case 2: return (event.amount || 0) < 0 ? 'Water removed' : 'Water logged';
      case 3: return 'Water goal reached';
      case 4: return 'Breathing session';
      case 5: return 'Focus session';
      case 6: return 'Bedtime bonus';
      case 7: return 'Daily goal';
      case 8: return 'All goals bonus';
      case 9: return 'Admin adjustment';
      case 10: return 'Sleep tracked';
      case 11: return 'Water break bonus';
      default: return 'Reward';
    }
  }

  function detailText(event) {
    const detail = event.detail ?? 0;
    switch (event.reason) {
      case 2: {
        if (detail === 0) return '';
        const n = Math.abs(detail);
        return `${n} glass${n === 1 ? '' : 'es'} ${(event.amount || 0) < 0 ? 'removed' : 'logged'}`;
      }
      case 3:
        return detail > 0 ? `${detail} glasses` : '';
      case 4:
        return detail > 0 ? `${detail} cycles` : '';
      case 5:
        return detail > 0 ? 'Pomodoro' : '';
      case 6: {
        const total = ((detail % 1440) + 1440) % 1440;
        const hr = Math.floor(total / 60) % 24;
        const min = total % 60;
        return `${(hr % 12) || 12}:${String(min).padStart(2, '0')} ${hr >= 12 ? 'PM' : 'AM'}`;
      }
      case 10: {
        if (detail <= 0) return '';
        const h = Math.floor(detail / 60);
        const m = detail % 60;
        return h > 0 ? `${h}h ${m}m` : `${m}m`;
      }
      default:
        return '';
    }
  }

  function fmtAmount(amount) {
    if (!amount) return '+0';
    return amount > 0 ? `+${amount}` : `${amount}`;
  }

  function fmtClock(ts) {
    if (!ts) return '';
    const d = new Date(ts);
    const hr = d.getHours();
    return `${(hr % 12) || 12}:${String(d.getMinutes()).padStart(2, '0')} ${hr >= 12 ? 'PM' : 'AM'}`;
  }

  function dayLabel(event) {
    if (event.ts) {
      const d = new Date(event.ts);
      const todayStart = new Date(new Date().setHours(0, 0, 0, 0)).getTime();
      const dayStart = new Date(d.getFullYear(), d.getMonth(), d.getDate()).getTime();
      const diff = Math.round((todayStart - dayStart) / 86400000);
      const clock = fmtClock(event.ts);
      if (diff === 0) return `Today · ${clock}`;
      if (diff === 1) return `Yesterday · ${clock}`;
      return `${d.toLocaleDateString(undefined, { weekday: 'short', month: 'short', day: 'numeric' })} · ${clock}`;
    }
    const parts = (event.day || '').split('-').map(Number);
    if (parts.length === 3 && parts[0] && parts[1] && parts[2]) {
      const d = new Date(parts[0], parts[1] - 1, parts[2]);
      const todayStart = new Date(new Date().setHours(0, 0, 0, 0)).getTime();
      const dayStart = new Date(parts[0], parts[1] - 1, parts[2]).getTime();
      const diff = Math.round((todayStart - dayStart) / 86400000);
      if (diff === 0) return 'Today';
      if (diff === 1) return 'Yesterday';
      return d.toLocaleDateString(undefined, { weekday: 'short', month: 'short', day: 'numeric' });
    }
    return event.day || 'Today';
  }

  function eventSubtitle(event) {
    return [detailText(event), dayLabel(event)].filter(Boolean).join(' · ');
  }

  const STREAK_META = {
    focus: { label: 'Focus', logo: timerLogo, color: '#3B7D4B', hint: 'Complete one focus session each day to keep this streak alive.' },
    water: { label: 'Water', logo: waterLogo, color: '#118AB2', hint: 'Log at least one glass of water each day to keep this streak alive.' },
    breathing: { label: 'Breathing', logo: breathingLogo, color: '#B084CC', hint: 'Complete a breathing session each day to keep this streak alive.' },
    goals: { label: 'Daily goals', logo: tamagotchiLogo, color: '#E0A84C', hint: 'Finish at least one daily goal each day to keep this streak alive.' },
    sleep: { label: 'Sleep', logo: sleepingLogo, color: '#5C7AEA', hint: 'Track a sleep session each night to keep this streak alive.' },
  };

  function streakMeta(activity) {
    return STREAK_META[activity] || { label: activity, logo: null, color: 'var(--color-primary)', hint: '' };
  }

  const STREAK_MILESTONES = [3, 7, 14, 30];

  function streakMilestone(days) {
    for (const m of STREAK_MILESTONES) {
      if (days < m) return m;
    }
    return null;
  }

  function streakPct(days) {
    const m = streakMilestone(days);
    if (m === null) return 100;
    return Math.min(100, (days / m) * 100);
  }

  function streakMilestoneLabel(days) {
    const m = streakMilestone(days);
    if (m === null) return 'Max streak reached';
    if (days <= 0) return `Next milestone: ${m} days`;
    return `${days} of ${m} days to milestone`;
  }
</script>

<div class="tamagotchi-page">
  <div class="page-header">
    <h2><img class="page-logo" src={tamagotchiLogo} alt="Tamagotchi" /> Tamagotchi</h2>
    <p class="card-sub">Your seeds, goals, streaks, and recent rewards live here.</p>
  </div>

  <div class="subtab-bar" role="tablist">
    <button
      class="subtab {subtab === 'plant' ? 'active' : ''}"
      role="tab"
      aria-selected={subtab === 'plant'}
      on:click={() => setSubtab('plant')}
    >
      🌱 Plant
    </button>
    <button
      class="subtab {subtab === 'goals' ? 'active' : ''}"
      role="tab"
      aria-selected={subtab === 'goals'}
      on:click={() => setSubtab('goals')}
    >
      Goals &amp; Seeds
    </button>
  </div>

  {#if subtab === 'plant'}
    <PlantPreview {state} />
  {:else}
  <section class="summary-card card">
    <div class="summary-copy">
      <div class="summary-head">
        <p class="eyebrow">Seeds overview</p>
        <InfoIcon float={false} lines={insights.tamSummaryInsight(state.level, state.today)} title="Why seeds matter" />
      </div>
      <h3>{state.total} seeds</h3>
      <p class="summary-meta">{state.today} earned today · Level {state.level}</p>
    </div>
    <div class="summary-progress">
      <div class="progress-labels">
        <span>Level {state.level}</span>
        <span>{state.levelProgress}/{state.levelThreshold}</span>
      </div>
      <div class="progress-track">
        <div class="progress-fill" style={`width: ${progressPercent}%`}></div>
      </div>
      <p class="card-sub">Keep going to grow your plant and unlock the next milestone.</p>
    </div>
  </section>

  <section class="goals-section">
    <div class="section-header">
      <h3>Today's goals</h3>
      <span class="header-right">
        <span class="goal-count">{completedGoals}/{goals.length} done</span>
        <InfoIcon float={false} lines={insights.tamGoalsInsight(completedGoals, goals.length)} title="Daily goals & wellbeing" />
      </span>
    </div>
    <div class="goal-list">
      {#each goals as goal}
        <div class={`goal-row card ${goal.done ? 'done' : ''}`} bind:this={rowEls[goal.index]}>
          <button
            class="goal-check"
            class:checked={goal.done}
            on:click={(e) => toggleGoalFromList(goal.index, e)}
            aria-label={goal.done ? `Mark "${goal.label}" not done` : `Mark "${goal.label}" done`}
          >
            {#if goal.done}<span class="check-mark">✓</span>{/if}
          </button>
          <button type="button" class="goal-body" on:click={() => openGoalEditor(goal)}>
            <span class="goal-text">{goal.label}</span>
            <span class="goal-hint">{goal.done ? 'Done for today' : 'Tap to edit'}</span>
          </button>
          <button class="btn-icon goal-edit" on:click={() => openGoalEditor(goal)} title="Edit goal">✎</button>
        </div>
      {/each}
      {#if goals.length === 0}
        <div class="empty-state card">
          <p>No goals yet.</p>
          <p class="card-sub">Add your first daily goal to get started.</p>
        </div>
      {/if}
    </div>
  </section>

  <section class="streaks-section">
    <div class="section-header">
      <h3>Streaks</h3>
      <InfoIcon float={false} lines={insights.tamStreaksInsight(longestStreak)} title="Why streaks matter" />
    </div>
    <div class="streak-list">
      {#each state.streaks as streak}
        <div class="streak-item card">
          <div class="streak-top">
            {#if streakMeta(streak.activity).logo}
              <img class="streak-logo" src={streakMeta(streak.activity).logo} alt={streakMeta(streak.activity).label} />
            {/if}
            <span class="streak-name">{streakMeta(streak.activity).label}</span>
            {#if streak.days > 0}
              <span class="streak-days">{streak.days} day{streak.days === 1 ? '' : 's'}</span>
            {:else}
              <span class="streak-days muted">Not started</span>
            {/if}
          </div>
          <span class="streak-hint">{streakMeta(streak.activity).hint}</span>
          <div class="streak-track">
            <div
              class="streak-fill"
              style={`width: {streakPct(streak.days)}%; background: {streakMeta(streak.activity).color};`}
            ></div>
          </div>
          <span class="streak-mult">{streakMilestoneLabel(streak.days)} · {streak.multiplier / 100}x seeds</span>
        </div>
      {/each}
    </div>
  </section>

  <section class="history-section">
    <div class="section-header">
      <h3>Recent activity</h3>
      <InfoIcon float={false} lines={insights.tamHistoryInsight()} title="Reflecting on progress" />
    </div>
    <div class="history-list">
      {#each state.history as event}
        <div class="history-item card">
          <div>
            <strong>{describeEvent(event)}</strong>
            <p class="card-sub">{eventSubtitle(event)}</p>
          </div>
          <span class={`history-amount ${event.amount >= 0 ? 'positive' : 'negative'}`}>{fmtAmount(event.amount)}</span>
        </div>
      {/each}
      {#if state.history.length === 0}
        <div class="empty-state card">
          <p>No seed activity yet.</p>
          <p class="card-sub">Complete a habit to start earning seeds.</p>
        </div>
      {/if}
    </div>
  </section>
  {/if}
</div>

{#if bursts.length > 0}
  <div class="fireworks-layer" aria-hidden="true">
    {#each bursts as burst}
      <div class="firework-burst" style={`left: {burst.x}px; top: {burst.y}px;`}>
        {#each burst.particles as p}
          <span
            class="firework-particle {p.spark ? 'spark' : ''}"
            style={`--tx: {p.tx}px; --ty: {p.ty}px; --rot: {p.rot}deg; width: {p.size}px; height: {p.size}px; background: {p.color}; animation-delay: {p.delay}s;`}
          ></span>
        {/each}
      </div>
    {/each}
  </div>
{/if}

{#if selectedGoal}
  <button
    type="button"
    class="modal-backdrop"
    aria-label="Close goal editor"
    on:click={closeEditor}
    on:keydown={(e) => {
      if (e.key === 'Enter' || e.key === ' ') closeEditor();
    }}
  ></button>
  <div class="modal-card card">
    <div class="modal-header">
      <h3>Edit goal</h3>
      <button class="btn-icon" on:click={closeEditor}>✕</button>
    </div>
    <label>
      <span>Goal</span>
      <input bind:value={draftLabel} placeholder="What do you want to accomplish today?" />
    </label>
    <label class="checkbox-row">
      <input type="checkbox" bind:checked={draftDone} />
      <span>Completed today</span>
    </label>
    <div class="modal-actions">
      <button class="btn btn-ghost" on:click={closeEditor}>Cancel</button>
      <button class="btn btn-primary" on:click={saveGoal}>Save</button>
    </div>
  </div>
{/if}

<style>
  .tamagotchi-page {
    display: flex;
    flex-direction: column;
    gap: 1rem;
  }

  .page-header h2 {
    margin-bottom: 0.25rem;
  }

  .subtab-bar {
    display: inline-flex;
    gap: 2px;
    align-self: flex-start;
    background: var(--color-surface);
    border-radius: 999px;
    padding: 3px;
  }

  .subtab {
    border: none;
    background: transparent;
    color: var(--color-text-secondary);
    font-size: 0.8rem;
    font-weight: 600;
    font-family: inherit;
    padding: 0.4rem 1rem;
    border-radius: 999px;
    cursor: pointer;
    transition: all 0.15s ease;
  }

  .subtab:hover {
    color: var(--color-text);
  }

  .subtab.active {
    background: var(--color-primary);
    color: #fff;
  }

  .summary-card {
    display: grid;
    gap: 1rem;
    align-items: center;
  }

  .eyebrow {
    font-size: 0.72rem;
    text-transform: uppercase;
    letter-spacing: 0.08em;
    color: var(--color-primary);
    font-weight: 600;
  }

  .summary-head,
  .header-right {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 0.6rem;
  }

  .summary-copy h3 {
    font-size: 1.75rem;
    margin-bottom: 0.25rem;
  }

  .summary-meta {
    color: var(--color-text-secondary);
  }

  .progress-labels {
    display: flex;
    justify-content: space-between;
    font-size: 0.85rem;
    color: var(--color-text-secondary);
    margin-bottom: 0.35rem;
  }

  .progress-track {
    width: 100%;
    height: 10px;
    border-radius: 999px;
    overflow: hidden;
    background: var(--color-surface);
  }

  .progress-fill {
    height: 100%;
    background: linear-gradient(90deg, var(--color-primary), var(--color-info));
    border-radius: inherit;
  }

  .goals-section, .streaks-section, .history-section {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
  }

  .section-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
  }

  .goal-count {
    font-size: 0.8rem;
    color: var(--color-text-secondary);
  }

  .goal-list {
    display: flex;
    flex-direction: column;
    gap: 0.9rem;
  }

  .goal-row {
    display: flex;
    align-items: center;
    gap: 1rem;
    padding: 1.25rem 1.25rem;
    border: 2px solid var(--color-border);
    box-shadow: 0 2px 8px rgba(0, 0, 0, 0.06);
    transition: border-color 0.2s ease, background 0.2s ease, transform 0.1s ease;
  }

  .goal-row:hover {
    border-color: var(--color-primary);
  }

  .goal-row.done {
    border-color: var(--color-tag-green);
    background: color-mix(in srgb, var(--color-tag-green) 8%, var(--color-bg-card));
  }

  .goal-check {
    width: 44px;
    height: 44px;
    flex-shrink: 0;
    border-radius: 999px;
    border: 3px solid var(--color-border);
    background: transparent;
    display: flex;
    align-items: center;
    justify-content: center;
    cursor: pointer;
    transition: all 0.2s ease;
  }

  .goal-check:hover {
    border-color: var(--color-primary);
  }

  .goal-check.checked {
    background: var(--color-tag-green);
    border-color: var(--color-tag-green);
  }

  .check-mark {
    color: white;
    font-size: 1.4rem;
    font-weight: 700;
    line-height: 1;
    animation: pop-in 0.2s ease;
  }

  @keyframes pop-in {
    from { transform: scale(0.4); opacity: 0; }
    to { transform: scale(1); opacity: 1; }
  }

  .goal-body {
    flex: 1;
    display: flex;
    flex-direction: column;
    gap: 0.2rem;
    min-width: 0;
    text-align: left;
    background: none;
    border: none;
    padding: 0;
    font: inherit;
    cursor: pointer;
  }

  .goal-text {
    font-size: 1.2rem;
    font-weight: 700;
    color: var(--color-text);
    line-height: 1.3;
    transition: color 0.2s ease;
  }

  .goal-row.done .goal-text {
    color: var(--color-text-secondary);
    text-decoration: line-through;
    text-decoration-thickness: 2px;
  }

  .goal-hint {
    font-size: 0.75rem;
    color: var(--color-text-secondary);
    text-transform: uppercase;
    letter-spacing: 0.06em;
  }

  .goal-edit {
    flex-shrink: 0;
    color: var(--color-text-secondary);
  }

  .streak-list {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
  }

  .streak-item {
    padding: 1rem 1.25rem;
    display: flex;
    flex-direction: column;
    gap: 0.6rem;
  }

  .streak-top {
    display: flex;
    align-items: center;
    gap: 0.75rem;
  }

  .streak-logo {
    width: 26px;
    height: 26px;
    object-fit: contain;
  }

  .streak-name {
    flex: 1;
    font-size: 1rem;
    font-weight: 700;
    color: var(--color-text);
  }

  .streak-hint {
    font-size: 0.72rem;
    color: var(--color-text-muted);
    line-height: 1.35;
  }

  .streak-days {
    font-size: 0.9rem;
    font-weight: 600;
    color: var(--color-text-secondary);
  }

  .streak-days.muted {
    color: var(--color-text-muted);
    font-weight: 400;
  }

  .streak-track {
    width: 100%;
    height: 10px;
    border-radius: 999px;
    background: var(--color-surface);
    overflow: hidden;
  }

  .streak-fill {
    height: 100%;
    border-radius: inherit;
    transition: width 0.4s ease;
  }

  .streak-mult {
    font-size: 0.75rem;
    color: var(--color-text-secondary);
    text-transform: uppercase;
    letter-spacing: 0.06em;
  }

  .history-list {
    display: flex;
    flex-direction: column;
    gap: 0.6rem;
  }

  .history-item {
    display: flex;
    justify-content: space-between;
    align-items: center;
    gap: 0.75rem;
  }

  .history-amount {
    font-weight: 600;
  }

  .history-amount.positive {
    color: var(--color-primary);
  }

  .history-amount.negative {
    color: var(--color-error);
  }

  .empty-state.compact {
    padding: 0.8rem 1rem;
  }

  .modal-backdrop {
    position: fixed;
    inset: 0;
    background: rgba(0, 0, 0, 0.3);
    z-index: 20;
    border: 0;
    padding: 0;
    cursor: pointer;
  }

  .modal-card {
    position: fixed;
    inset: auto 1rem 1rem 1rem;
    z-index: 21;
    max-width: 420px;
    margin: 0 auto;
    padding: 1rem;
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
  }

  .modal-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
  }

  .modal-card label {
    display: flex;
    flex-direction: column;
    gap: 0.25rem;
    font-size: 0.9rem;
    color: var(--color-text-secondary);
  }

  .checkbox-row {
    flex-direction: row;
    align-items: center;
    gap: 0.5rem;
  }

  .modal-actions {
    display: flex;
    justify-content: flex-end;
    gap: 0.5rem;
    margin-top: 0.25rem;
  }

  .fireworks-layer {
    position: fixed;
    inset: 0;
    pointer-events: none;
    z-index: 100;
  }

  .firework-burst {
    position: absolute;
    width: 0;
    height: 0;
  }

  .firework-particle {
    position: absolute;
    left: 0;
    top: 0;
    border-radius: 999px;
    animation: firework-pop 1s ease-out both;
  }

  .firework-particle.spark {
    border-radius: 2px;
  }

  @keyframes firework-pop {
    0% {
      transform: translate(0, 0) rotate(0deg) scale(1);
      opacity: 1;
    }
    70% {
      opacity: 1;
    }
    100% {
      transform: translate(var(--tx), var(--ty)) rotate(var(--rot)) scale(0.1);
      opacity: 0;
    }
  }

  @media (min-width: 700px) {
    .summary-card {
      grid-template-columns: 1.1fr 1fr;
    }
  }
</style>
