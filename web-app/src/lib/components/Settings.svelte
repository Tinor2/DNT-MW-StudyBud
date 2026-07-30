<script>
  import { theme } from '../stores/theme.js';
  import { notifications } from '../stores/notifications.js';
  import { ws } from '../stores/websocket.js';

  export let connected = false;
  export let settings = { brightness: 50, volume: 50, idle_timeout: 30 };

  let currentTheme = 'light';
  theme.subscribe(v => currentTheme = v);

  let devMode = window.location.hostname === 'localhost';
  let host = devMode ? '' : window.location.host;

  function send(type, data = {}) {
    ws.send({ type, ...data });
  }

  function applyDevice() {
    send('settings_update', {
      brightness: settings.brightness,
      volume: settings.volume,
      idle_timeout: settings.idle_timeout
    });
    notifications.add('settings_update', 'settings', 'Device settings applied');
  }

  function exportData() {
    const data = {};
    for (let i = 0; i < localStorage.length; i++) {
      const key = localStorage.key(i);
      if (key.startsWith('studybud_')) {
        try {
          data[key] = JSON.parse(localStorage.getItem(key));
        } catch {
          data[key] = localStorage.getItem(key);
        }
      }
    }
    const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `studybud_export_${new Date().toISOString().slice(0, 10)}.json`;
    a.click();
    URL.revokeObjectURL(url);
    notifications.add('export', 'settings', 'Data exported successfully');
  }

  function clearData() {
    if (confirm('Are you sure? This will clear all cached data.')) {
      const keys = [];
      for (let i = 0; i < localStorage.length; i++) {
        const key = localStorage.key(i);
        if (key.startsWith('studybud_')) keys.push(key);
      }
      keys.forEach(k => localStorage.removeItem(k));
      notifications.add('system', 'system', 'All cached data cleared');
    }
  }

  let showExportModal = false;
</script>

<div class="settings">
  <h2>Settings</h2>

  <!-- Device -->
  <section class="settings-group">
    <h3>Device Controls</h3>
    <div class="setting-row">
      <!-- svelte-ignore a11y-label-has-associated-control -->
      <label>Brightness</label>
      <div class="slider-row">
        <input type="range" bind:value={settings.brightness} min="0" max="100" />
        <span class="value">{settings.brightness}</span>
      </div>
    </div>
    <div class="setting-row">
      <!-- svelte-ignore a11y-label-has-associated-control -->
      <label>Volume</label>
      <div class="slider-row">
        <input type="range" bind:value={settings.volume} min="0" max="100" />
        <span class="value">{settings.volume}</span>
      </div>
    </div>
    <div class="setting-row">
      <!-- svelte-ignore a11y-label-has-associated-control -->
      <label>Idle Timeout</label>
      <div class="slider-row">
        <input type="number" bind:value={settings.idle_timeout} min="5" max="300" />
        <span class="value">{settings.idle_timeout}s</span>
      </div>
    </div>
    <button class="btn btn-primary" on:click={applyDevice} disabled={!connected}>
      Apply to Device
    </button>
  </section>

  <!-- Appearance -->
  <section class="settings-group">
    <h3>Appearance</h3>
    <div class="setting-row">
      <!-- svelte-ignore a11y-label-has-associated-control -->
      <label>Theme</label>
      <div class="toggle-row">
        <button
          class="btn {currentTheme === 'light' ? 'btn-primary' : 'btn-ghost'}"
          on:click={() => theme.set('light')}>Light</button>
        <button
          class="btn {currentTheme === 'dark' ? 'btn-primary' : 'btn-ghost'}"
          on:click={() => theme.set('dark')}>Dark</button>
      </div>
    </div>
  </section>

  <!-- Data -->
  <section class="settings-group">
    <h3>Data</h3>
    <div class="button-row">
      <button class="btn btn-primary" on:click={exportData}>Export All Data</button>
      <button class="btn btn-ghost" on:click={clearData}>Clear Cache</button>
    </div>
  </section>

  <!-- Connection -->
  <section class="settings-group">
    <h3>Connection</h3>
    <div class="setting-row">
      <!-- svelte-ignore a11y-label-has-associated-control -->
      <label>Mode</label>
      <span class="mode-badge">{devMode ? 'Development (Vite proxy)' : 'Production'}</span>
    </div>
  </section>
</div>

<style>
  .settings {
    display: flex;
    flex-direction: column;
    gap: 1rem;
  }

  .settings h2 {
    font-size: 1.1rem;
    color: var(--color-text);
    margin-bottom: 0.25rem;
  }

  .settings-group {
    background: var(--color-bg-card);
    border: 1px solid var(--color-border);
    border-radius: var(--radius);
    padding: 1rem;
  }

  .settings-group h3 {
    font-size: 0.8rem;
    font-weight: 600;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--color-text-muted);
    margin-bottom: 0.75rem;
  }

  .setting-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 1rem;
    padding: 0.5rem 0;
  }

  .setting-row + .setting-row {
    border-top: 1px solid var(--color-border);
  }

  .setting-row label {
    font-size: 0.875rem;
    color: var(--color-text);
    min-width: 100px;
  }

  .slider-row {
    display: flex;
    align-items: center;
    gap: 0.75rem;
    flex: 1;
  }

  .slider-row input[type="range"] {
    flex: 1;
    padding: 0;
    border: none;
    background: transparent;
    accent-color: var(--color-primary);
  }

  .slider-row input[type="number"] {
    width: 70px;
  }

  .value {
    font-size: 0.75rem;
    color: var(--color-text-muted);
    min-width: 2.5rem;
    text-align: right;
  }

  .toggle-row {
    display: flex;
    gap: 0.5rem;
  }

  .toggle-row .btn {
    min-width: 70px;
    justify-content: center;
  }

  .button-row {
    display: flex;
    gap: 0.5rem;
    flex-wrap: wrap;
  }

  .mode-badge {
    font-size: 0.8rem;
    color: var(--color-text-secondary);
    background: var(--color-surface);
    padding: 0.25rem 0.75rem;
    border-radius: var(--radius-pill);
  }
</style>
