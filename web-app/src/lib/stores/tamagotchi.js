import { writable } from 'svelte/store';

export function createInitialTamagotchiState() {
  return {
    total: 0,
    today: 0,
    day: '',
    level: 1,
    levelProgress: 0,
    levelThreshold: 100,
    waterToday: 0,
    waterGoal: 8,
    waterBonusClaimed: false,
    bedtimeBonusClaimed: false,
    bedtime: { hour: 23, min: 30 },
    goals: [],
    streaks: [],
    history: [],
  };
}

export const tamagotchiStore = writable(createInitialTamagotchiState());

export const goalMetricOptions = [
  { value: 0, label: 'Manual' },
  { value: 1, label: 'Water' },
  { value: 2, label: 'Focus' },
  { value: 3, label: 'Breathing' },
  { value: 4, label: 'Todos' },
];

export function metricLabel(metric) {
  const match = goalMetricOptions.find(option => option.value === metric);
  return match ? match.label : 'Manual';
}

export function normalizePointsPayload(payload = {}) {
  const goals = Array.isArray(payload.goals)
    ? payload.goals.map((goal, index) => ({
        index: goal.index ?? index,
        label: goal.label || `Goal ${index + 1}`,
        metric: goal.metric ?? 0,
        target: goal.target ?? 1,
        done: !!goal.done,
      }))
    : [];

  const streaks = Array.isArray(payload.streaks)
    ? payload.streaks.map(streak => ({
        activity: streak.activity || 'activity',
        days: streak.days ?? 0,
        multiplier: streak.multiplier ?? 100,
      }))
    : [];

  const history = Array.isArray(payload.history)
    ? payload.history.map(event => ({
        amount: event.amount ?? 0,
        reason: event.reason ?? 0,
        detail: event.detail ?? 0,
        day: event.day || '',
        ts: event.ts ?? Date.now(),
      }))
    : [];

  return {
    total: payload.total ?? 0,
    today: payload.today ?? 0,
    day: payload.day || '',
    level: payload.level ?? 1,
    levelProgress: payload.level_progress ?? 0,
    levelThreshold: payload.level_threshold ?? 100,
    waterToday: payload.water_today ?? 0,
    waterGoal: payload.water_goal ?? 8,
    waterBonusClaimed: !!payload.water_bonus,
    bedtimeBonusClaimed: !!payload.bedtime_bonus,
    bedtime: payload.bedtime
      ? { hour: payload.bedtime.hour ?? 23, min: payload.bedtime.min ?? 30 }
      : { hour: 23, min: 30 },
    goals,
    streaks,
    history,
  };
}

export function setPointsState(payload) {
  tamagotchiStore.set(normalizePointsPayload(payload));
}

export function updateGoalInStore(index, updates) {
  tamagotchiStore.update(state => ({
    ...state,
    goals: state.goals.map(goal => (goal.index === index ? { ...goal, ...updates } : goal)),
  }));
}

export function applyPointsEvent(event) {
  tamagotchiStore.update(state => ({
    ...state,
    total: event.total ?? state.total,
    today: event.today ?? state.today,
  }));
}
