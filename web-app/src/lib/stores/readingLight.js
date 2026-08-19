import { writable } from 'svelte/store';

const STORAGE_KEY = 'studybud_reading_light';

function clamp(v) {
  if (Number.isNaN(v)) return 0;
  return Math.min(100, Math.max(0, v));
}

function loadStrength() {
  try {
    return clamp(parseInt(localStorage.getItem(STORAGE_KEY), 10));
  } catch {
    return 0;
  }
}

function applyStrength(v) {
  const el = document.documentElement;
  el.style.setProperty('--reading-light-opacity', String(v / 100));
  el.setAttribute('data-reading-light', v > 0 ? 'on' : 'off');
}

function createReadingLightStore() {
  const { subscribe, set } = writable(loadStrength());

  let current = loadStrength();
  if (typeof document !== 'undefined') applyStrength(current);

  return {
    subscribe,
    set(v) {
      current = clamp(v);
      try {
        localStorage.setItem(STORAGE_KEY, String(current));
      } catch {}
      if (typeof document !== 'undefined') applyStrength(current);
      set(current);
    }
  };
}

export const readingLight = createReadingLightStore();
