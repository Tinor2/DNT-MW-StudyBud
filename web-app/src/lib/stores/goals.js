import { writable } from 'svelte/store';

const STORAGE_KEY = 'studybud_daily_goals';
export const MAX_DAILY_GOALS = 3;
export const POINTS_GOAL = 20;
export const POINTS_ALL_GOALS = 30;

function todayKey() {
  const d = new Date();
  const pad = n => String(n).padStart(2, '0');
  return `${d.getFullYear()}-${pad(d.getMonth() + 1)}-${pad(d.getDate())}`;
}

function seedGoals() {
  const today = todayKey();
  return [
    { index: 0, label: 'Drink 8 glasses of water', done: false, date: today, awarded: false },
    { index: 1, label: 'Complete a focus session', done: false, date: today, awarded: false },
    { index: 2, label: 'Do 3 breathing exercises', done: false, date: today, awarded: false },
  ];
}

function normalizeGoals(raw) {
  const goals = [];
  for (let i = 0; i < MAX_DAILY_GOALS; i++) {
    const g = Array.isArray(raw) ? raw[i] : null;
    goals.push({
      index: g?.index ?? i,
      label: g?.label || `Goal ${i + 1}`,
      done: !!g?.done,
      date: g?.date || todayKey(),
      awarded: !!g?.awarded,
    });
  }
  return goals;
}

function load() {
  let raw = null;
  try {
    const saved = localStorage.getItem(STORAGE_KEY);
    raw = saved ? JSON.parse(saved) : null;
  } catch {
    raw = null;
  }

  const today = todayKey();
  const goals = normalizeGoals(raw && Array.isArray(raw.goals) ? raw.goals : seedGoals());
  let allGoalsBonusClaimed = !!raw?.allGoalsBonusClaimed;
  let rolledOver = false;

  for (const g of goals) {
    if (g.date !== today) {
      g.date = today;
      g.done = false;
      g.awarded = false;
      rolledOver = true;
    }
  }
  if (rolledOver) allGoalsBonusClaimed = false;

  return { goals, allGoalsBonusClaimed };
}

function write(state) {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(state));
  } catch {
    /* storage unavailable */
  }
}

export const goalsStore = writable(load());

export function updateGoal(index, updates) {
  goalsStore.update(state => {
    const goals = state.goals.map(g => {
      if (g.index !== index) return g;
      return {
        ...g,
        label: updates.label !== undefined ? updates.label : g.label,
      };
    });
    const next = { ...state, goals };
    write(next);
    return next;
  });
}

export function toggleGoal(index) {
  let points = 0;
  goalsStore.update(state => {
    const today = todayKey();
    const goals = state.goals.map(g => {
      const goal = { ...g, date: today };
      if (goal.index !== index) return goal;

      goal.done = !goal.done;
      if (goal.done && !goal.awarded) {
        goal.awarded = true;
        points += POINTS_GOAL;
      }
      if (!goal.done) {
        goal.awarded = false;
      }
      return goal;
    });

    let allGoalsBonusClaimed = state.allGoalsBonusClaimed;
    const allDone = goals.every(g => g.done);
    if (allDone && !allGoalsBonusClaimed) {
      allGoalsBonusClaimed = true;
      points += POINTS_ALL_GOALS;
    } else if (!allDone && allGoalsBonusClaimed) {
      allGoalsBonusClaimed = false;
    }

    const next = { ...state, goals, allGoalsBonusClaimed };
    write(next);
    return next;
  });
  return { points };
}

export function checkRollover() {
  let reset = false;
  goalsStore.update(state => {
    const today = todayKey();
    let changed = false;
    const goals = state.goals.map(g => {
      if (g.date === today) return g;
      reset = true;
      changed = true;
      return { ...g, date: today, done: false, awarded: false };
    });
    let allGoalsBonusClaimed = state.allGoalsBonusClaimed;
    if (reset && allGoalsBonusClaimed) {
      allGoalsBonusClaimed = false;
      changed = true;
    }
    if (!changed) return state;
    const next = { ...state, goals, allGoalsBonusClaimed };
    write(next);
    return next;
  });
  return reset;
}

export function seedDemoGoals() {
  const today = todayKey();
  const goals = [
    { index: 0, label: 'Drink 8 glasses of water', done: true, date: today, awarded: true },
    { index: 1, label: 'Read for 30 minutes', done: false, date: today, awarded: false },
    { index: 2, label: '10 minutes of stretching', done: false, date: today, awarded: false },
  ];
  const next = { goals, allGoalsBonusClaimed: false };
  goalsStore.set(next);
  write(next);
}
