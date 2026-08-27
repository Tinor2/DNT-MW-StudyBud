import { writable } from 'svelte/store';

const STORAGE_KEY = 'studybud_theme';

function loadTheme() {
  try {
    const saved = localStorage.getItem(STORAGE_KEY);
    if (saved === 'light' || saved === 'dark') return saved;
    return window.matchMedia('(prefers-color-scheme: dark)').matches ? 'dark' : 'light';
  } catch {
    return 'light';
  }
}

function saveTheme(mode) {
  try {
    localStorage.setItem(STORAGE_KEY, mode);
  } catch {}
}

function applyTheme(mode) {
  document.documentElement.setAttribute('data-theme', mode);
}

function createThemeStore() {
  const { subscribe, set, update } = writable(loadTheme());

  let current = loadTheme();
  applyTheme(current);

  return {
    subscribe,
    toggle() {
      update(mode => {
        const next = mode === 'light' ? 'dark' : 'light';
        current = next;
        applyTheme(next);
        saveTheme(next);
        return next;
      });
    },
    set(mode) {
      current = mode;
      applyTheme(mode);
      saveTheme(mode);
      set(mode);
    }
  };
}

export const theme = createThemeStore();

theme.subscribe(mode => {
  if (typeof document !== 'undefined') {
    applyTheme(mode);
  }
});
