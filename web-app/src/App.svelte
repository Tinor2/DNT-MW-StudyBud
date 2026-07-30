<script>
  import './style.css'
  import { ws } from './lib/stores/websocket.js';
  import { theme } from './lib/stores/theme.js';
  import { notifications } from './lib/stores/notifications.js';
  import NotificationToast from './lib/components/NotificationToast.svelte';
  import Settings from './lib/components/Settings.svelte';
  import EventLog from './lib/components/EventLog.svelte';

  const devMode = window.location.hostname === 'localhost';
  let host = devMode ? 'localhost:5173' : window.location.host;
  let connected = false;
  let statusText = 'disconnected';
  let messages = [];
  let currentTheme = 'light';
  let eventLog = loadEventLog();

  let activeTab = 'home';
  let showEventLog = false;

  theme.subscribe(v => currentTheme = v);

  function loadEventLog() {
    try {
      const saved = localStorage.getItem('studybud_event_log');
      return saved ? JSON.parse(saved) : [];
    } catch { return []; }
  }

  function saveEventLog() {
    try {
      localStorage.setItem('studybud_event_log', JSON.stringify(eventLog.slice(0, 500)));
    } catch {}
  }

  let timerState = { remaining_ms: 0, running: false, preset_id: 0, phase: 0 };
  let todos = [];
  let presets = [];
  let breathingState = { exercises: [], active: false, active_id: 0 };
  let waterState = { glasses: 0, goal: 8 };
  let settingsState = { brightness: 50, volume: 50, idle_timeout: 30 };

  ws.subscribe(state => {
    if (state.status === 'connected' && statusText !== 'connected') {
      send('full_sync');
      send('get_breathing');
    }
    connected = state.status === 'connected';
    statusText = state.status;
    messages = state.messages;
    processMessages(state.messages);
  });

  function processMessages(msgs) {
    msgs.forEach(msg => {
      if (msg.dir === 'in' && msg.data) {
        logEvent('in', msg.data.type, msg.data);
        switch (msg.data.type) {
          case 'full_sync':
            todos = msg.data.todos || [];
            presets = msg.data.presets || [];
            timerState = { ...timerState, ...msg.data.timer } || timerState;
            waterState = { glasses: msg.data.water?.glasses || 0, goal: msg.data.water?.goal || 8 };
            settingsState = { ...settingsState, ...msg.data.settings };
            breathingState = { exercises: msg.data.exercises || [], active: msg.data.active || false, active_id: msg.data.active_id || 0 };
            break;
          case 'timer_info': case 'timer_update': case 'timer_sync':
            timerState = { ...timerState, ...msg.data };
            break;
          case 'todos_info': case 'todos_sync': case 'todo_sync':
            todos = msg.data.tasks || [];
            break;
          case 'presets_info': case 'presets_sync':
            presets = msg.data.presets || [];
            break;
          case 'breathing_info': case 'breathing_sync':
            breathingState = { exercises: msg.data.exercises || [], active: msg.data.active || false, active_id: msg.data.active_id || 0 };
            break;
          case 'water_info': case 'water_sync': case 'water_update':
            waterState = { glasses: msg.data.glasses || 0, goal: msg.data.goal || 8 };
            break;
          case 'settings_info': case 'settings_sync':
            settingsState = { brightness: msg.data.brightness || 50, volume: msg.data.volume || 50, idle_timeout: msg.data.idle_timeout || 30 };
            break;
        }
      }
    });
  }

  function logEvent(dir, type, data) {
    eventLog = [{ timestamp: new Date().toISOString(), direction: dir, type, data }, ...eventLog.slice(0, 499)];
    saveEventLog();
    console.log(`[${dir.toUpperCase()}] ${type}:`, data);
  }

  function connect() {
    ws.connect(host, devMode);
    logEvent('out', 'connect', { host });
  }

  function disconnect() {
    ws.disconnect();
    logEvent('out', 'disconnect', {});
  }

  function send(type, data = {}) {
    const msg = { type, ...data };
    ws.send(msg);
    logEvent('out', type, msg);
  }

  function clearLog() {
    eventLog = [];
    ws.clearLog();
  }

  function formatMs(ms) {
    const mins = Math.floor(ms / 60000);
    const secs = Math.floor((ms % 60000) / 1000);
    return `${mins}:${secs.toString().padStart(2, '0')}`;
  }

  function formatTimeShort(ts) {
    const d = new Date(ts);
    return d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' });
  }

  function timeAgo(ts) {
    const diff = Date.now() - new Date(ts).getTime();
    const mins = Math.floor(diff / 60000);
    if (mins < 1) return 'just now';
    if (mins < 60) return `${mins}m ago`;
    const hrs = Math.floor(mins / 60);
    if (hrs < 24) return `${hrs}h ago`;
    return `${Math.floor(hrs / 24)}d ago`;
  }

  // Timer commands
  function timerStart() { send('timer_command', { action: 'start' }); }
  function timerPause() { send('timer_command', { action: 'pause' }); }
  function timerReset() { send('timer_command', { action: 'reset' }); }
  function timerSkip() { send('timer_command', { action: 'skip' }); }

  // Todo commands
  function todoAdd() {
    const text = prompt('Todo text:');
    if (text) { send('todo_add', { text, priority: 3 }); }
  }
  function todoToggle(id, done) { send('todo_update', { id, done: !done }); }
  function todoDelete(id) {
    if (confirm('Delete todo?')) { send('todo_delete', { id }); }
  }

  // Breathing commands
  function breathingStart() {
    const id = prompt('Exercise ID (1-5):', breathingState.active_id || 1);
    if (id) send('breathing_start', { exercise_id: parseInt(id) });
  }
  function breathingStop() { send('breathing_stop'); }

  // Water commands
  function waterAdd() { send('water_log', { action: 'add' }); }
  function waterRemove() { send('water_log', { action: 'remove' }); }
  function waterGoal() {
    const goal = prompt('Water goal (glasses):', waterState.goal);
    if (goal) send('water_goal', { goal: parseInt(goal) });
  }

  // Preset commands
  function presetSelect(id) { send('preset_select', { preset_id: id }); }
  function presetAdd() {
    const name = prompt('Preset name:');
    if (name) {
      send('preset_add', {
        name,
        focus_ms: parseInt(prompt('Focus duration (ms):', 25 * 60 * 1000)) || 25 * 60 * 1000,
        break_ms: parseInt(prompt('Break duration (ms):', 5 * 60 * 1000)) || 5 * 60 * 1000,
        is_pomodoro: confirm('Is this a Pomodoro preset?')
      });
    }
  }
  function presetDelete(id) {
    if (confirm('Delete preset?')) { send('preset_delete', { preset_id: id }); }
  }

  function priorityColor(p) {
    const colors = ['#C97A7A', '#E0A84C', '#7DBF9E', '#9B9FBA'];
    return colors[p] || '#9B9FBA';
  }

  const tabs = [
    { id: 'home', label: 'Home', icon: '⌂' },
    { id: 'timer', label: 'Timer', icon: '⏱' },
    { id: 'todos', label: 'Todos', icon: '✓' },
    { id: 'breathing', label: 'Breathing', icon: '🌬' },
    { id: 'sleep', label: 'Sleep', icon: '🌙' },
    { id: 'settings', label: 'Settings', icon: '⚙' },
  ];

  function statusDot() {
    if (connected) return '🟢';
    if (statusText.includes('connect') || statusText.includes('reconnect')) return '🟡';
    return '🔴';
  }
</script>

<NotificationToast />

<div class="app-layout">
  <!-- Header -->
  <header class="app-header">
    <div class="header-left">
      <h1 class="app-title">StudyBud</h1>
    </div>
    <div class="header-right">
      <button class="btn-icon" on:click={connected ? disconnect : connect} title={connected ? 'Disconnect' : 'Connect'}>
        {statusDot()}
      </button>
      <span class="status-badge {statusText}">{statusText}</span>
      <button class="btn-icon" on:click={showEventLog = !showEventLog} class:btn-active={showEventLog} title="Event Log">
        📋
      </button>
      <button class="btn-icon" on:click={() => theme.toggle()} title="Toggle theme">
        {currentTheme === 'light' ? '🌙' : '☀️'}
      </button>
    </div>
  </header>

  <!-- Tab Bar -->
  <nav class="tab-bar">
    {#each tabs as tab}
      <button
        class="tab {activeTab === tab.id ? 'active' : ''}"
        on:click={() => activeTab = tab.id}
      >
        <span class="tab-icon">{tab.icon}</span>
        <span class="tab-label">{tab.label}</span>
      </button>
    {/each}
  </nav>

  <!-- Main Content -->
  <main class="tab-content">
    <!-- Home -->
    {#if activeTab === 'home'}
      <div class="page home-page">
        <h2>Dashboard</h2>
        {#if !connected}
          <div class="connect-prompt card">
            <p>Connect to your StudyBud to see live data.</p>
            <button class="btn btn-primary" on:click={connect}>Connect</button>
          </div>
        {/if}
        <div class="dashboard-grid">
          <div class="dashboard-card card">
            <h3>✓ Todos</h3>
            <p class="card-value">{todos.filter(t => !t.done).length} active</p>
            <p class="card-sub">{todos.filter(t => t.done).length} completed</p>
            {#if todos.length === 0}
              <p class="card-empty">No todos</p>
            {/if}
          </div>
          <div class="dashboard-card card">
            <h3>💧 Water</h3>
            <p class="card-value">{waterState.glasses}/{waterState.goal}</p>
            <p class="card-sub">glasses today</p>
          </div>
          <div class="dashboard-card card">
            <h3>🌬 Breathing</h3>
            <p class="card-value">{breathingState.active ? 'Active' : 'Inactive'}</p>
            <p class="card-sub">{breathingState.exercises.length} exercises</p>
          </div>
          <div class="dashboard-card card">
            <h3>🌙 Sleep</h3>
            <p class="card-value">--</p>
            <p class="card-sub">no data yet</p>
          </div>
          <div class="dashboard-card card">
            <h3>🔗 Device</h3>
            <p class="card-value status-{statusText}">{statusText}</p>
            <p class="card-sub">{devMode ? 'Vite proxy' : 'ESP32'}</p>
          </div>
        </div>
      </div>

    <!-- Timer / Presets -->
    {:else if activeTab === 'timer'}
      <div class="page timer-page">
        <h2>Timer Presets</h2>
        <div class="timer-status card">
          <p>Timer: <strong>{formatMs(timerState.remaining_ms)}</strong>
            {timerState.running ? '▶ Running' : '⏸ Paused'}
            (Phase {timerState.phase})</p>
          <div class="button-row">
            <button class="btn btn-primary" on:click={timerStart} disabled={!connected}>Start</button>
            <button class="btn btn-ghost" on:click={timerPause} disabled={!connected}>Pause</button>
            <button class="btn btn-ghost" on:click={timerReset} disabled={!connected}>Reset</button>
            <button class="btn btn-ghost" on:click={timerSkip} disabled={!connected}>Skip</button>
          </div>
        </div>
        <div class="preset-grid">
          {#each presets as preset, i}
            <div class="preset-card card">
              <div class="preset-header">
                <span class="badge type-badge">
                  {preset.is_pomodoro ? 'Pomodoro' : 'Standard'}
                </span>
                <button class="btn-icon" on:click={() => presetDelete(preset.id)} title="Delete">✕</button>
              </div>
              <h3>{preset.name}</h3>
              <p class="preset-duration">{formatMs(preset.focus_ms || preset.duration_ms || 0)}</p>
              <button
                class="btn btn-primary preset-select-btn"
                on:click={() => presetSelect(preset.id)}
                disabled={!connected}
              >
                {preset.id === timerState.preset_id ? '● Active' : '○ Select'}
              </button>
            </div>
          {:else}
            <div class="empty-state card">
              <p>No presets yet.</p>
              <p class="card-sub">Add one from the connected ESP32.</p>
            </div>
          {/each}
          <button class="preset-card card add-card" on:click={presetAdd} disabled={!connected}>
            <span class="add-icon">+</span>
            <p>Add Preset</p>
          </button>
        </div>
      </div>

    <!-- Todos -->
    {:else if activeTab === 'todos'}
      <div class="page todos-page">
        <h2>Todos</h2>
        <button class="btn btn-primary add-todo-btn" on:click={todoAdd} disabled={!connected}>
          + Add Todo
        </button>
        <div class="todo-list">
          {#each todos.filter(t => !t.done) as todo}
            <div class="todo-item card">
              <button class="checkbox" on:click={() => todoToggle(todo.id, todo.done)}>
                {todo.done ? '✓' : '○'}
              </button>
              <span class="todo-text">{todo.text}</span>
              {#if todo.priority !== undefined}
                <span class="priority-dot" style="background: {priorityColor(todo.priority)}"></span>
              {/if}
              <button class="btn-icon todo-delete" on:click={() => todoDelete(todo.id)}>✕</button>
            </div>
          {/each}
          {#if todos.filter(t => t.done).length > 0}
            <div class="completed-section">
              <p class="completed-label">{todos.filter(t => t.done).length} completed</p>
              {#each todos.filter(t => t.done) as todo}
                <div class="todo-item card done">
                  <button class="checkbox" on:click={() => todoToggle(todo.id, todo.done)}>✓</button>
                  <span class="todo-text done">{todo.text}</span>
                  <button class="btn-icon todo-delete" on:click={() => todoDelete(todo.id)}>✕</button>
                </div>
              {/each}
            </div>
          {/if}
          {#if todos.length === 0}
            <div class="empty-state card">
              <p>No todos yet.</p>
              <p class="card-sub">Add one to get started!</p>
            </div>
          {/if}
        </div>
      </div>

    <!-- Breathing -->
    {:else if activeTab === 'breathing'}
      <div class="page breathing-page">
        <h2>Breathing</h2>
        <div class="breathing-status card">
          <p>Status: {breathingState.active ? '▶ Active' : '⏸ Inactive'}</p>
          <div class="button-row">
            <button class="btn btn-primary" on:click={breathingStart} disabled={!connected}>Start</button>
            <button class="btn btn-ghost" on:click={breathingStop} disabled={!connected}>Stop</button>
          </div>
        </div>
        <div class="exercise-grid">
          {#each breathingState.exercises as ex}
            <div class="exercise-card card">
              <h3>{ex.name}</h3>
              <p class="exercise-timing">
                {ex.inhale_ms / 1000}s in / {ex.hold_ms / 1000}s hold / {ex.exhale_ms / 1000}s out
                {#if ex.hold2_ms > 0}
                  / {ex.hold2_ms / 1000}s hold
                {/if}
              </p>
            </div>
          {:else}
            <div class="empty-state card">
              <p>No breathing exercises loaded.</p>
            </div>
          {/each}
        </div>
      </div>

    <!-- Sleep -->
    {:else if activeTab === 'sleep'}
      <div class="page sleep-page">
        <h2>Sleep</h2>
        <div class="empty-state card">
          <p>Sleep tracking coming soon.</p>
          <p class="card-sub">Data will sync from your StudyBud device.</p>
        </div>
      </div>

    <!-- Settings -->
    {:else if activeTab === 'settings'}
      <Settings {connected} settings={settingsState} />
    {/if}
  </main>

  <!-- Event Log Overlay -->
  {#if showEventLog}
    <!-- svelte-ignore a11y-click-events-have-key-events a11y-no-static-element-interactions -->
    <div class="overlay" on:click={showEventLog = false}></div>
    <div class="event-log-panel">
      <div class="panel-header">
        <h2>Event Log</h2>
        <button class="btn btn-ghost" on:click={() => showEventLog = false}>✕ Close</button>
      </div>
      <div class="panel-content">
        <EventLog {eventLog} on:clear={clearLog} />
      </div>
    </div>
  {/if}
</div>

<style>
  .app-layout {
    display: flex;
    flex-direction: column;
    height: 100vh;
    overflow: hidden;
  }

  /* Header */
  .app-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0.5rem 1rem;
    background: var(--color-bg-card);
    border-bottom: 1px solid var(--color-border);
    flex-shrink: 0;
  }

  .header-left {
    display: flex;
    align-items: center;
    gap: 0.75rem;
  }

  .app-title {
    font-size: 1rem;
    font-weight: 600;
    color: var(--color-primary);
  }

  .header-right {
    display: flex;
    align-items: center;
    gap: 0.25rem;
  }

  .status-badge {
    font-size: 0.65rem;
    text-transform: uppercase;
    color: var(--color-text-muted);
    display: none;
  }

  @media (min-width: 480px) {
    .status-badge { display: inline; }
  }

  /* Tab Bar */
  .tab-bar {
    display: flex;
    background: var(--color-bg-card);
    border-bottom: 1px solid var(--color-border);
    flex-shrink: 0;
    overflow-x: auto;
    -webkit-overflow-scrolling: touch;
  }

  .tab {
    display: flex;
    align-items: center;
    gap: 0.4rem;
    padding: 0.6rem 1rem;
    font-size: 0.8rem;
    color: var(--color-text-secondary);
    background: transparent;
    border: none;
    border-bottom: 2px solid transparent;
    cursor: pointer;
    white-space: nowrap;
    transition: all 0.15s ease;
  }

  .tab:hover {
    color: var(--color-text);
    background: var(--color-surface);
  }

  .tab.active {
    color: #fff;
    background: var(--color-primary);
    border-bottom-color: var(--color-primary-dark);
  }

  .tab-icon {
    font-size: 1rem;
  }

  .tab-label {
    display: none;
  }

  @media (min-width: 640px) {
    .tab { padding: 0.6rem 1.25rem; }
    .tab-label { display: inline; }
  }

  /* Main Content */
  .tab-content {
    flex: 1;
    overflow-y: auto;
    padding: 1rem;
  }

  .page h2 {
    font-size: 1.1rem;
    color: var(--color-text);
    margin-bottom: 1rem;
  }

  .button-row {
    display: flex;
    gap: 0.4rem;
    flex-wrap: wrap;
  }

  .empty-state {
    text-align: center;
    padding: 2rem 1rem;
    color: var(--color-text-muted);
  }

  .empty-state p { margin-bottom: 0.25rem; }

  .card-sub {
    font-size: 0.75rem;
    color: var(--color-text-muted);
  }

  /* Home Dashboard */
  .dashboard-grid {
    display: grid;
    grid-template-columns: 1fr;
    gap: 0.75rem;
  }

  @media (min-width: 500px) {
    .dashboard-grid { grid-template-columns: 1fr 1fr; }
  }

  .dashboard-card h3 {
    font-size: 0.8rem;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    color: var(--color-text-muted);
    margin-bottom: 0.4rem;
  }

  .card-value {
    font-size: 1.25rem;
    font-weight: 600;
    color: var(--color-text);
  }

  .card-empty {
    font-size: 0.8rem;
    color: var(--color-text-muted);
    font-style: italic;
  }

  .connect-prompt {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 1rem;
    margin-bottom: 0.75rem;
  }

  .connect-prompt p {
    font-size: 0.875rem;
    color: var(--color-text-secondary);
  }

  /* Timer Page */
  .timer-status {
    margin-bottom: 1rem;
  }

  .timer-status p {
    font-size: 0.875rem;
    color: var(--color-text);
    margin-bottom: 0.5rem;
  }

  .preset-grid {
    display: grid;
    grid-template-columns: 1fr;
    gap: 0.75rem;
  }

  @media (min-width: 500px) {
    .preset-grid { grid-template-columns: 1fr 1fr; }
  }
  @media (min-width: 768px) {
    .preset-grid { grid-template-columns: 1fr 1fr 1fr; }
  }

  .preset-card {
    display: flex;
    flex-direction: column;
    gap: 0.4rem;
  }

  .preset-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
  }

  .type-badge {
    background: var(--color-surface);
    color: var(--color-text-secondary);
    font-size: 0.7rem;
  }

  .preset-card h3 {
    font-size: 0.95rem;
    font-weight: 600;
    color: var(--color-text);
  }

  .preset-duration {
    font-size: 1.5rem;
    font-weight: 700;
    color: var(--color-timer);
    font-family: var(--font-mono);
  }

  .preset-select-btn {
    align-self: flex-start;
    margin-top: 0.25rem;
  }

  .add-card {
    display: flex;
    flex-direction: column;
    align-items: center;
    justify-content: center;
    min-height: 140px;
    cursor: pointer;
    border: 2px dashed var(--color-border);
    background: transparent;
  }

  .add-card:hover {
    border-color: var(--color-primary);
    background: var(--color-surface);
  }

  .add-icon {
    font-size: 2rem;
    color: var(--color-text-muted);
    margin-bottom: 0.25rem;
  }

  .add-card p {
    font-size: 0.875rem;
    color: var(--color-text-muted);
  }

  /* Todos Page */
  .add-todo-btn {
    margin-bottom: 0.75rem;
  }

  .todo-list {
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
  }

  .todo-item {
    display: flex;
    align-items: center;
    gap: 0.6rem;
    padding: 0.6rem 0.75rem;
  }

  .todo-item.done {
    opacity: 0.6;
  }

  .checkbox {
    width: 28px;
    height: 28px;
    display: flex;
    align-items: center;
    justify-content: center;
    border-radius: 50%;
    background: transparent;
    border: 2px solid var(--color-border);
    color: transparent;
    cursor: pointer;
    flex-shrink: 0;
    font-size: 0.8rem;
  }

  .todo-item.done .checkbox {
    background: var(--color-primary);
    border-color: var(--color-primary);
    color: #fff;
  }

  .todo-text {
    flex: 1;
    font-size: 0.875rem;
    color: var(--color-text);
  }

  .todo-text.done {
    text-decoration: line-through;
    color: var(--color-text-muted);
  }

  .priority-dot {
    width: 8px;
    height: 8px;
    border-radius: 50%;
    flex-shrink: 0;
  }

  .todo-delete {
    opacity: 0;
    transition: opacity 0.15s;
  }

  .todo-item:hover .todo-delete {
    opacity: 1;
  }

  .completed-section {
    margin-top: 0.5rem;
    padding-top: 0.5rem;
    border-top: 1px solid var(--color-border);
  }

  .completed-label {
    font-size: 0.75rem;
    color: var(--color-text-muted);
    margin-bottom: 0.5rem;
  }

  /* Breathing Page */
  .breathing-status {
    margin-bottom: 1rem;
  }

  .breathing-status p {
    font-size: 0.875rem;
    color: var(--color-text);
    margin-bottom: 0.5rem;
  }

  .exercise-grid {
    display: grid;
    grid-template-columns: 1fr;
    gap: 0.75rem;
  }

  @media (min-width: 500px) {
    .exercise-grid { grid-template-columns: 1fr 1fr; }
  }

  .exercise-card h3 {
    font-size: 0.95rem;
    font-weight: 600;
    color: var(--color-text);
    margin-bottom: 0.3rem;
  }

  .exercise-timing {
    font-size: 0.8rem;
    color: var(--color-text-secondary);
    font-family: var(--font-mono);
  }

  /* Overlay */
  .overlay {
    position: fixed;
    inset: 0;
    background: rgba(0,0,0,0.4);
    z-index: 100;
  }

  .event-log-panel {
    position: fixed;
    top: 0;
    right: 0;
    bottom: 0;
    width: 100%;
    max-width: 520px;
    background: var(--color-bg);
    z-index: 101;
    display: flex;
    flex-direction: column;
    box-shadow: var(--shadow-lg);
    animation: slideIn 0.2s ease;
  }

  .panel-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
    padding: 0.75rem 1rem;
    background: var(--color-bg-card);
    border-bottom: 1px solid var(--color-border);
    flex-shrink: 0;
  }

  .panel-header h2 {
    font-size: 1rem;
    color: var(--color-text);
    margin: 0;
  }

  .panel-content {
    flex: 1;
    overflow-y: auto;
    padding: 1rem;
  }
</style>

<script context="module">
</script>

