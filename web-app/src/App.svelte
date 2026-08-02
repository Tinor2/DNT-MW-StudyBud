<script>
  import { onDestroy } from 'svelte';
  import './style.css'
  import timerLogo from './assets/logos/timer.png';
  import todoLogo from './assets/logos/todo.png';
  import breathingLogo from './assets/logos/breathing.png';
  import waterLogo from './assets/logos/water.png';
  import sleepLogo from './assets/logos/sleeping_logo.png';
  import { ws } from './lib/stores/websocket.js';
  import { theme } from './lib/stores/theme.js';
  import { notifications } from './lib/stores/notifications.js';
  import NotificationToast from './lib/components/NotificationToast.svelte';
  import Settings from './lib/components/Settings.svelte';
  import EventLog from './lib/components/EventLog.svelte';

  const devMode = window.location.hostname === 'localhost';
  const DEBUG_LOG = new URLSearchParams(window.location.search).has('debug');
  let host = devMode ? 'localhost:5173' : window.location.host;
  let connected = false;
  let statusText = 'disconnected';
  let messages = [];
  let currentTheme = 'light';
  let eventLog = loadEventLog();
  let sleepLog = loadLog('studybud_sleep_log');
  let breathingLog = loadLog('studybud_breathing_log');
  let breathSessions = loadLog('studybud_breath_sessions');
  let waterLog = loadLog('studybud_water_log');
  let focusLog = loadLog('studybud_focus_log');
  let sleepState = 'idle';

  seedDemoSessions();
  seedDemoLogs();

  function seedDemoSessions() {
    try {
      if (localStorage.getItem('studybud_seeded_sessions_v1')) return;
    } catch { return; }
    if (breathSessions.length > 0) return;
    const names = ['Box Breathing', '4-7-8 Relaxation'];
    const cyclesPer = [8, 12, 6, 15, 10, 9, 14, 7];
    const seeds = [];
    for (let i = 0; i < 8; i++) {
      const d = new Date();
      d.setDate(d.getDate() - (7 - i));
      const exId = (i % 2) + 1;
      const cycles = cyclesPer[i];
      seeds.push({
        ts: d.getTime(),
        exercise_id: exId,
        exercise_name: names[i % 2],
        cycles,
        duration_sec: cycles * 4 * 4
      });
    }
    breathSessions = seeds;
    saveLog('studybud_breath_sessions', seeds);
    try { localStorage.setItem('studybud_seeded_sessions_v1', '1'); } catch {}
  }

  function seedDemoLogs() {
    if (sleepLog.length > 0 || breathingLog.length > 0 || waterLog.length > 0) return;
    try {
      if (localStorage.getItem('studybud_seeded_v1')) return;
    } catch { return; }
    const sleepSeed = [400, 430, 480, 360, 470];
    const sleepTimes = [1380, 1410, 1350, 0, 1390];
    const breathSeed = [8, 12, 6, 15, 10];
    const waterSeed = [3, 5, 4, 7, 6];
    const sleepNext = [];
    const breathNext = [];
    const waterNext = [];
    for (let i = 5; i >= 1; i--) {
      const d = new Date();
      d.setDate(d.getDate() - i);
      const date = dateKey(d);
      sleepNext.push({ date, minutes: sleepSeed[5 - i], start_hour_min: sleepTimes[5 - i] });
      breathNext.push({ date, cycles: breathSeed[5 - i] });
      waterNext.push({ date, glasses: waterSeed[5 - i] });
    }
    sleepLog = sleepNext;
    breathingLog = breathNext;
    waterLog = waterNext;
    saveLog('studybud_sleep_log', sleepNext);
    saveLog('studybud_breathing_log', breathNext);
    saveLog('studybud_water_log', waterNext);
    try { localStorage.setItem('studybud_seeded_v1', '1'); } catch {}
  }

  const SYNC_INTERVAL_MS = 10000;
  const syncTimer = setInterval(() => {
    if (connected) {
      send('full_sync');
      send('get_breathing');
      send('get_sleep');
      send('get_water');
      send('get_timer');
    }
  }, SYNC_INTERVAL_MS);

  onDestroy(() => clearInterval(syncTimer));

  let activeTab = 'home';
  let showEventLog = false;
  let breathingRangeValue = 'week';
  let sleepRangeValue = 'week';
  let waterRangeValue = 'week';

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

  const LOG_LIMIT = 120;

  function loadLog(key) {
    try {
      const saved = localStorage.getItem(key);
      return saved ? JSON.parse(saved) : [];
    } catch { return []; }
  }

  function saveLog(key, log) {
    try { localStorage.setItem(key, JSON.stringify(log.slice(0, LOG_LIMIT))); } catch {}
  }

  function dateKey(d) {
    return `${d.getFullYear()}-${String(d.getMonth() + 1).padStart(2, '0')}-${String(d.getDate()).padStart(2, '0')}`;
  }

  function todayKey() { return dateKey(new Date()); }

  function addSleepEntry(minutes, startHourMin) {
    const date = todayKey();
    const idx = sleepLog.findIndex(e => e.date === date);
    const next = sleepLog.slice();
    const entry = {
      ...(idx >= 0 ? next[idx] : { date }),
      minutes: (idx >= 0 ? next[idx].minutes || 0 : 0) + minutes
    };
    if (startHourMin !== undefined && startHourMin >= 0) entry.start_hour_min = startHourMin;
    if (idx >= 0) next[idx] = entry;
    else next.unshift(entry);
    sleepLog = next;
    saveLog('studybud_sleep_log', sleepLog);
  }

  function addBreathEntry(cycles) {
    const date = todayKey();
    const idx = breathingLog.findIndex(e => e.date === date);
    const next = breathingLog.slice();
    if (idx >= 0) next[idx] = { ...next[idx], cycles: (next[idx].cycles || 0) + cycles };
    else next.unshift({ date, cycles });
    breathingLog = next;
    saveLog('studybud_breathing_log', breathingLog);
  }

  function addBreathSession(cycles, exerciseId) {
    if (!cycles) return;
    const ex = breathingState.exercises.find(e => e.id === exerciseId);
    const secPerCycle = (ex ? (ex.inhale_ms + ex.hold_ms + ex.exhale_ms + ex.hold2_ms) : 16000) / 1000;
    const entry = {
      ts: Date.now(),
      exercise_id: exerciseId || 0,
      exercise_name: ex ? ex.name : `Exercise #${exerciseId}`,
      cycles,
      duration_sec: Math.round(cycles * secPerCycle)
    };
    breathSessions = [entry, ...breathSessions.slice(0, 499)];
    saveLog('studybud_breath_sessions', breathSessions);
  }

  function fmtClock(mins) {
    if (mins < 0) return '—';
    const h = Math.floor(mins / 60) % 24;
    const m = Math.floor(mins % 60);
    return `${String(h).padStart(2, '0')}:${String(m).padStart(2, '0')}`;
  }

  function addWaterEntry(delta) {
    const date = todayKey();
    const idx = waterLog.findIndex(e => e.date === date);
    const next = waterLog.slice();
    const cur = idx >= 0 ? (next[idx].glasses || 0) : 0;
    const val = Math.max(0, cur + delta);
    if (idx >= 0) next[idx] = { ...next[idx], glasses: val };
    else next.unshift({ date, glasses: val });
    waterLog = next;
    saveLog('studybud_water_log', waterLog);
  }

  function backfillWater(glassesToday) {
    const n = glassesToday || 0;
    if (n <= 0) return;
    const date = todayKey();
    const idx = waterLog.findIndex(e => e.date === date);
    const next = waterLog.slice();
    if (idx >= 0) {
      if ((next[idx].glasses || 0) < n) next[idx] = { ...next[idx], glasses: n };
    } else {
      next.unshift({ date, glasses: n });
    }
    waterLog = next;
    saveLog('studybud_water_log', waterLog);
  }

  function backfillSleep(history) {
    if (!Array.isArray(history) || history.length === 0) return;
    const n = Math.min(history.length, 7);
    let next = sleepLog.slice();
    for (let i = 0; i < n; i++) {
      const minutes = history[i] || 0;
      if (minutes <= 0) continue;
      const d = new Date();
      d.setDate(d.getDate() - (n - 1 - i));
      const date = dateKey(d);
      const idx = next.findIndex(e => e.date === date);
      if (idx >= 0) next[idx] = { ...next[idx], minutes: Math.max(next[idx].minutes || 0, minutes) };
      else next.push({ date, minutes });
    }
    sleepLog = next;
    saveLog('studybud_sleep_log', sleepLog);
  }

  function last7DayKeys() {
    const keys = [];
    for (let i = 6; i >= 0; i--) {
      const d = new Date();
      d.setDate(d.getDate() - i);
      keys.push(dateKey(d));
    }
    return keys;
  }

  let range = 'week';

  function dayKeys(n) {
    const keys = [];
    for (let i = n - 1; i >= 0; i--) {
      const d = new Date();
      d.setDate(d.getDate() - i);
      keys.push(dateKey(d));
    }
    return keys;
  }

  function rangeDays(r) {
    if (r === '3d') return 3;
    if (r === 'month') return 30;
    return 7;
  }

  function breathingRange(r) { range = r; if (connected) send('get_breathing'); }
  function sleepRange(r) { range = r; if (connected) send('get_sleep'); }
  function waterRange(r) { range = r; if (connected) send('get_water'); }

  function backfillBreathing(sessionsToday) {
    const n = sessionsToday || 0;
    if (n <= 0) return;
    const date = todayKey();
    const idx = breathingLog.findIndex(e => e.date === date);
    const next = breathingLog.slice();
    if (idx >= 0) {
      if ((next[idx].cycles || 0) < n) next[idx] = { ...next[idx], cycles: n };
    } else {
      next.unshift({ date, cycles: n });
    }
    breathingLog = next;
    saveLog('studybud_breathing_log', breathingLog);
  }

  function addFocusEntry(e) {
    if (!e) return;
    const entry = {
      ts: Date.now(),
      preset_name: e.preset_name || 'unknown',
      is_pomodoro: !!e.is_pomodoro,
      phase: e.phase,
      phase_name: e.phase_name || 'session',
      duration_sec: e.duration_sec || 0
    };
    focusLog = [entry, ...focusLog.slice(0, 499)];
    saveLog('studybud_focus_log', focusLog);
  }

  function isSameDay(ts, offsetDays) {
    const d = new Date(ts);
    d.setHours(0, 0, 0, 0);
    const today = new Date();
    today.setHours(0, 0, 0, 0);
    today.setDate(today.getDate() + offsetDays);
    return d.getTime() === today.getTime();
  }

  function dayLabel(key) {
    const d = new Date(key + 'T12:00:00');
    if (range === 'month') return String(d.getDate());
    return ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'][d.getDay()];
  }

  $: focusTodayEntries = focusLog.filter(e => e.phase === 0 && isSameDay(e.ts, 0));
  $: focusSessionsToday = focusTodayEntries.length;
  $: focusMinutesToday = Math.round(focusTodayEntries.reduce((s, e) => s + (e.duration_sec || 0), 0) / 60);
  $: pomodorosThisWeek = focusLog.filter(e => e.phase === 0 && e.is_pomodoro && e.ts >= Date.now() - 7 * 86400000).length;
  $: breaksToday = focusLog.filter(e => e.phase !== 0 && isSameDay(e.ts, 0)).length;
  $: breathingData = dayKeys(rangeDays(range)).map(k => ({
    date: k,
    label: dayLabel(k),
    cycles: (breathingLog.find(x => x.date === k) || {}).cycles || 0
  }));
  $: sleepData = dayKeys(rangeDays(range)).map(k => {
    const entry = sleepLog.find(x => x.date === k) || {};
    return {
      date: k,
      label: dayLabel(k),
      hours: (entry.minutes || 0) / 60,
      start_hour_min: entry.start_hour_min
    };
  });
  $: waterData = dayKeys(rangeDays(range)).map(k => ({
    date: k,
    label: dayLabel(k),
    glasses: (waterLog.find(x => x.date === k) || {}).glasses || 0
  }));
  $: weeklySleepAvg = (() => {
    const nights = sleepData.filter(b => b.hours > 0);
    return nights.length ? nights.reduce((s, b) => s + b.hours, 0) / nights.length : 0;
  })();
  $: avgBedtimeMin = (() => {
    const mins = sleepData.filter(b => b.start_hour_min !== undefined && b.start_hour_min >= 0).map(b => b.start_hour_min);
    if (!mins.length) return -1;
    const ref = 180;
    let s = 0;
    for (const m of mins) s += (m - ref + 1440) % 1440;
    return ((s / mins.length) + ref) % 1440;
  })();
  $: avgWakeMin = avgBedtimeMin >= 0 && weeklySleepAvg > 0 ? (avgBedtimeMin + weeklySleepAvg * 60) % 1440 : -1;
  $: favExercise = (() => {
    const counts = {};
    for (const s of breathSessions) {
      const key = s.exercise_name || `Exercise #${s.exercise_id}`;
      counts[key] = (counts[key] || 0) + 1;
    }
    let best = null, bestC = 0;
    for (const k in counts) {
      if (counts[k] > bestC) { bestC = counts[k]; best = k; }
    }
    return best ? { name: best, count: bestC } : null;
  })();
  $: totalBreathSec = breathSessions.reduce((s, e) => s + (e.duration_sec || 0), 0);
  $: avgBreathMin = breathSessions.length ? totalBreathSec / breathSessions.length / 60 : 0;
  $: breathSessionsWeek = breathSessions.filter(s => s.ts >= Date.now() - 7 * 86400000).length;

  let timerState = { remaining_ms: 0, running: false, preset_id: 0, phase: 0, phase_name: 'session', is_pomodoro: false, total_ms: 0, phase_complete: false };
  let todos = [];
  let presets = [];
  let breathingState = { exercises: [], active: false, active_id: 0, sessions_today: 0 };
  let waterState = { glasses: 0, goal: 8 };
  let settingsState = { brightness: 50, volume: 50, idle_timeout: 30 };

  let processedSeq = 0;
  let syncRequested = false;

  function requestSync() {
    send('full_sync');
    send('get_breathing');
    send('get_sleep');
    send('get_water');
    send('get_timer');
  }

  ws.subscribe(state => {
    if (state.status === 'connected' && !syncRequested) {
      syncRequested = true;
      requestSync();
    }
    if (state.status !== 'connected') syncRequested = false;
    connected = state.status === 'connected';
    statusText = state.status;
    messages = state.messages;
    if (state.seq > processedSeq) {
      processMessages(state.messages.slice(-(state.seq - processedSeq)));
    }
    processedSeq = state.seq;
  });

  function normalizeExercises(exercises) {
    return (exercises || []).map(ex => ({
      ...ex,
      inhale_ms: ex.inhale_ms ?? ex.inhale ?? 0,
      hold_ms: ex.hold_ms ?? ex.hold ?? 0,
      exhale_ms: ex.exhale_ms ?? ex.exhale ?? 0,
      hold2_ms: ex.hold2_ms ?? ex.hold2 ?? 0,
    }));
  }

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
            if (Array.isArray(msg.data.exercises)) {
              breathingState = { exercises: normalizeExercises(msg.data.exercises), active: msg.data.active || false, active_id: msg.data.active_id || 0, sessions_today: msg.data.sessions_today || 0 };
            }
            break;
          case 'timer_info': case 'timer_update': case 'timer_sync':
            timerState = { ...timerState, ...msg.data };
            break;
          case 'timer_session_complete':
            addFocusEntry(msg.data);
            break;
          case 'todos_info': case 'todos_sync': case 'todo_sync':
            todos = msg.data.tasks || [];
            break;
          case 'todo_toggled':
            todos = todos.map(t => t.id === (msg.data.id ?? -1)
              ? { ...t, done: msg.data.done }
              : t);
            break;
          case 'presets_info': case 'presets_sync':
            presets = msg.data.presets || [];
            break;
          case 'breathing_info': case 'breathing_sync':
            breathingState = { exercises: normalizeExercises(msg.data.exercises), active: msg.data.active || false, active_id: msg.data.active_id || 0, sessions_today: msg.data.sessions_today || 0 };
            backfillBreathing(msg.data.sessions_today);
            break;
          case 'breathing_selected':
            breathingState = { ...breathingState, active_id: msg.data.active_id ?? breathingState.active_id };
            break;
          case 'water_info': case 'water_sync': case 'water_update':
            waterState = { glasses: msg.data.glasses || 0, goal: msg.data.goal || 8 };
            backfillWater(msg.data.glasses);
            break;
          case 'sleep_state':
            sleepState = msg.data.state || 'idle';
            break;
          case 'sleep_session':
            addSleepEntry(msg.data.duration_min || 0, msg.data.start_hour_min);
            break;
          case 'sleep_info':
            backfillSleep(msg.data.history);
            break;
          case 'breathing_complete':
            addBreathEntry(msg.data.cycles || 0);
            addBreathSession(msg.data.cycles || 0, msg.data.exercise_id);
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
    if (DEBUG_LOG) console.log(`[${dir.toUpperCase()}] ${type}:`, data);
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

  // Timer commands (monitor only - control happens on the device)
  function timerInfo() { send('timer_info'); }

  // Todo commands
  function todoAdd() {
    const text = prompt('Todo text:');
    if (text) { send('todo_add', { text, priority: 3 }); }
  }
  function todoToggle(id, done) { send('todo_update', { id, done: !done }); }
  function todoDelete(id) {
    if (confirm('Delete todo?')) { send('todo_delete', { id }); }
  }

  // Breathing commands (select preset only - start/stop happens on the device)
  function breathingSelect(id) { send('breathing_select', { exercise_id: id }); }
  function breathingEdit(ex) {
    const name = prompt('Exercise name:', ex.name);
    if (name === null) return;
    const ask = (label, curSec) => {
      const s = prompt(label, String(curSec));
      if (s === null) return null;
      const n = parseFloat(s);
      return isNaN(n) ? curSec : n;
    };
    const inhale = ask('Inhale (seconds):', ex.inhale_ms / 1000);
    const hold = ask('Hold after inhale (seconds, 0 = none):', ex.hold_ms / 1000);
    const exhale = ask('Exhale (seconds):', ex.exhale_ms / 1000);
    const hold2 = ask('Hold after exhale (seconds, 0 = none):', ex.hold2_ms / 1000);
    send('breathing_update', {
      exercise_id: ex.id,
      name: name || ex.name,
      inhale: Math.round((inhale ?? ex.inhale_ms / 1000) * 1000),
      hold: Math.round((hold ?? ex.hold_ms / 1000) * 1000),
      exhale: Math.round((exhale ?? ex.exhale_ms / 1000) * 1000),
      hold2: Math.round((hold2 ?? ex.hold2_ms / 1000) * 1000)
    });
  }

  // Water commands
  function waterAdd() {
    addWaterEntry(1);
    send('water_log', { action: 'add' });
  }
  function waterRemove() {
    addWaterEntry(-1);
    send('water_log', { action: 'remove' });
  }
  function waterGoalSet(n) {
    const g = Math.max(1, Math.min(20, Math.round(n) || waterState.goal));
    send('water_goal', { goal: g });
  }

  function waterColor(progress) {
    const p = Math.max(0, Math.min(1, progress));
    const hue = 160;
    const sat = Math.round(10 + 45 * p);
    const light = Math.round(82 - 32 * p);
    return `hsl(${hue}, ${sat}%, ${light}%)`;
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
    { id: 'home', label: 'Home', icon: '⌂', logo: null },
    { id: 'timer', label: 'Timer', icon: '⏱', logo: timerLogo },
    { id: 'todos', label: 'Todos', icon: '✓', logo: todoLogo },
    { id: 'breathing', label: 'Breathing', icon: '🌬', logo: breathingLogo },
    { id: 'water', label: 'Water', icon: '💧', logo: waterLogo },
    { id: 'sleep', label: 'Sleep', icon: '🌙', logo: sleepLogo },
    { id: 'settings', label: 'Settings', icon: '⚙', logo: null },
  ];

  $: statusDot = connected ? '🟢' : (statusText.includes('connect') || statusText.includes('reconnect')) ? '🟡' : '🔴';
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
        {statusDot}
      </button>
      <span class="status-badge {statusText}">{statusText}</span>
      <button class="btn-icon" on:click={() => showEventLog = !showEventLog} class:btn-active={showEventLog} title="Event Log">
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
        <span class="tab-icon">
          {#if tab.logo}
            <img src={tab.logo} alt={tab.label} />
          {:else}
            {tab.icon}
          {/if}
        </span>
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
            <h3><img class="card-logo" src={todoLogo} alt="Todos" /> Todos</h3>
            <p class="card-value">{todos.filter(t => !t.done).length} active</p>
            <p class="card-sub">{todos.filter(t => t.done).length} completed</p>
            {#if todos.length === 0}
              <p class="card-empty">No todos</p>
            {/if}
          </div>
          <div class="dashboard-card card">
            <h3><img class="card-logo" src={waterLogo} alt="Water" /> Water</h3>
            <p class="card-value">{waterState.glasses}/{waterState.goal}</p>
            <p class="card-sub">glasses today</p>
          </div>
          <div class="dashboard-card card">
            <h3><img class="card-logo" src={breathingLogo} alt="Breathing" /> Breathing</h3>
            <p class="card-value">{breathingState.active ? 'Active' : 'Inactive'}</p>
            <p class="card-sub">{breathingState.exercises.length} exercises</p>
          </div>
          <div class="dashboard-card card">
            <h3><img class="card-logo" src={sleepLogo} alt="Sleep" /> Sleep</h3>
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
        <h2><img class="page-logo" src={timerLogo} alt="Timer" /> Timer Presets</h2>
        <div class="timer-status card">
          <p>
            Timer: <strong>{formatMs(timerState.remaining_ms)}</strong>
            {#if timerState.running}
              <span class="badge type-badge running">▶ Running</span>
            {:else if timerState.phase_complete}
              <span class="badge type-badge">⏸ Finished — press on device</span>
            {:else}
              <span class="badge type-badge">⏸ Paused</span>
            {/if}
          </p>
          <p>
            <span class="badge type-badge {timerState.is_pomodoro ? 'pomodoro' : ''}">
              {timerState.is_pomodoro ? 'Pomodoro' : 'Standard'}
            </span>
            <span class="badge type-badge">Phase: {timerState.phase_name || timerState.phase}</span>
            <span class="card-sub">Preset #{timerState.preset_id || '—'}</span>
          </p>
          <p class="card-sub">Timer control is device-only — the web app monitors live state.</p>
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
              <p class="preset-duration">
                {formatMs(preset.focus ?? preset.focus_ms ?? 0)}
                <span class="preset-break">/ {formatMs(preset.break_duration ?? preset.break_ms ?? 0)}</span>
              </p>
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

        <div class="card pomodoro-analytics">
          <h3>Pomodoro Analytics</h3>
          <div class="analytics-row">
            <div class="analytics-stat">
              <span class="analytics-value">{focusSessionsToday}</span>
              <span class="analytics-label">Focus sessions today</span>
            </div>
            <div class="analytics-stat">
              <span class="analytics-value">{focusMinutesToday}m</span>
              <span class="analytics-label">Focus time today</span>
            </div>
            <div class="analytics-stat">
              <span class="analytics-value">{pomodorosThisWeek}</span>
              <span class="analytics-label">Pomodoros this week</span>
            </div>
            <div class="analytics-stat">
              <span class="analytics-value">{breaksToday}</span>
              <span class="analytics-label">Breaks today</span>
            </div>
          </div>
          <p class="card-sub">Tracked from completed timer sessions reported by the device.</p>
        </div>
      </div>

    <!-- Todos -->
    {:else if activeTab === 'todos'}
      <div class="page todos-page">
        <h2><img class="page-logo" src={todoLogo} alt="Todos" /> Todos</h2>
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
        <h2><img class="page-logo" src={breathingLogo} alt="Breathing" /> Breathing</h2>
        <div class="breathing-status card">
          <p>Status: {breathingState.active ? '▶ Active' : '⏸ Inactive'}</p>
          <p class="card-sub">Sessions today: {breathingState.sessions_today}</p>
          <p class="card-sub">Breathing control is device-only — tap an exercise to select it. The selected pattern's timing drives the device's breathing animation.</p>
        </div>
        <div class="card analysis-card">
          <h3>Breathing Analysis</h3>
          <div class="analytics-row">
            <div class="analytics-stat">
              <span class="analytics-value">{favExercise ? favExercise.name : 'No data yet'}</span>
              <span class="analytics-label">Favourite breathing exercise</span>
            </div>
            <div class="analytics-stat">
              <span class="analytics-value">{favExercise ? favExercise.count : '—'}</span>
              <span class="analytics-label">Sessions using it</span>
            </div>
            <div class="analytics-stat">
              <span class="analytics-value">{avgBreathMin ? avgBreathMin.toFixed(1) + 'm' : '—'}</span>
              <span class="analytics-label">Avg breathing time / session</span>
            </div>
            <div class="analytics-stat">
              <span class="analytics-value">{breathSessionsWeek}</span>
              <span class="analytics-label">Breathing sessions this week</span>
            </div>
          </div>
          <p class="card-sub">Tracked from completed breathing sessions reported by the device.</p>
        </div>

        <div class="exercise-grid">
          {#each breathingState.exercises as ex}
            <div class="exercise-card card {ex.id === breathingState.active_id ? 'selected' : ''}">
              <div class="preset-header">
                <span class="badge type-badge">#{ex.id}</span>
                <button class="btn-icon" on:click={() => breathingEdit(ex)} disabled={!connected} title="Edit timing">✎</button>
              </div>
              <h3>{ex.name}</h3>
              <p class="exercise-timing">
                {ex.inhale_ms / 1000}s in / {ex.hold_ms / 1000}s hold / {ex.exhale_ms / 1000}s out
                {#if ex.hold2_ms > 0}
                  / {ex.hold2_ms / 1000}s hold
                {/if}
              </p>
              <button
                class="btn btn-primary preset-select-btn"
                on:click={() => breathingSelect(ex.id)}
                disabled={!connected}
              >
                {ex.id === breathingState.active_id ? '● Selected' : '○ Select'}
              </button>
            </div>
          {:else}
            <div class="empty-state card">
              <p>No breathing exercises loaded.</p>
            </div>
          {/each}
        </div>

        

        <div class="card">
          <div class="card-row">
            <h3>Breathing Cycles</h3>
            <div class="range-switch">
              <button class:active={range === '3d'} on:click={() => breathingRange('3d')}>3D</button>
              <button class:active={range === 'week'} on:click={() => breathingRange('week')}>Week</button>
              <button class:active={range === 'month'} on:click={() => breathingRange('month')}>Month</button>
            </div>
          </div>
          <div class="bar-chart" class:month={range === 'month'}>
            {#each breathingData as day}
              <div class="bar-col">
                <div class="bar-track">
                  <div class="bar breathing-bar" style="height: {Math.min(100, (day.cycles / 20) * 100)}%"></div>
                </div>
                <span class="bar-label">{day.label}</span>
                <span class="bar-value">{day.cycles}</span>
              </div>
            {/each}
          </div>
          <p class="card-sub">Completed breathing cycles per day (logged to this browser).</p>
        </div>

        

        <div class="info-box">
          <span class="info-icon">💡</span>
          <p>Slow, rhythmic breathing activates your parasympathetic nervous system, lowering heart rate and cortisol. A few minutes of deep breathing between study sessions is one of the fastest ways to reduce stress and sharpen focus.</p>
        </div>
      </div>

    <!-- Water -->
    {:else if activeTab === 'water'}
      <div class="page water-page">
        <h2><img class="page-logo" src={waterLogo} alt="Water" /> Water</h2>

        <div class="card">
          <div class="card-row">
            <div>
              <h3>Today</h3>
              <p class="water-total">{waterState.glasses}<span class="water-goal"> / {waterState.goal}</span></p>
              <p class="card-sub">glasses of water today</p>
            </div>
            <div class="button-row">
              <button class="btn btn-icon-lg" on:click={waterRemove} disabled={!connected || waterState.glasses <= 0} title="Remove a glass">−</button>
              <button class="btn btn-icon-lg" on:click={waterAdd} disabled={!connected} title="Add a glass">+</button>
            </div>
          </div>
          {#if connected}
            <div class="water-progress">
              <div class="water-progress-fill" style="width: {Math.min(100, (waterState.glasses / (waterState.goal || 8)) * 100)}%; background: {waterColor(waterState.glasses / (waterState.goal || 8))}"></div>
            </div>
          {/if}
        </div>

        <div class="card">
          <div class="card-row">
            <h3>Daily Goal</h3>
            <div class="goal-stepper">
              <button class="btn-icon" on:click={() => waterGoalSet(waterState.goal - 1)} disabled={!connected || waterState.goal <= 1}>−</button>
              <input
                class="goal-input"
                type="number"
                min="1"
                max="20"
                value={waterState.goal}
                on:change={(e) => waterGoalSet(parseInt(e.target.value, 10))}
                disabled={!connected}
              />
              <button class="btn-icon" on:click={() => waterGoalSet(waterState.goal + 1)} disabled={!connected}>+</button>
            </div>
          </div>
          <p class="card-sub">Goal is stored on the device and synced back to this page.</p>
        </div>

        <div class="card">
          <div class="card-row">
            <h3>Glasses per Day</h3>
            <div class="range-switch">
              <button class:active={range === '3d'} on:click={() => waterRange('3d')}>3D</button>
              <button class:active={range === 'week'} on:click={() => waterRange('week')}>Week</button>
              <button class:active={range === 'month'} on:click={() => waterRange('month')}>Month</button>
            </div>
          </div>
          <div class="bar-chart" class:month={range === 'month'}>
            {#each waterData as day}
              <div class="bar-col">
                <div class="bar-track">
                  <div class="bar water-bar" style="height: {Math.min(100, (day.glasses / (waterState.goal || 8)) * 100)}%; background: {waterColor(day.glasses / (waterState.goal || 8))}"></div>
                </div>
                <span class="bar-label">{day.label}</span>
                <span class="bar-value">{day.glasses}</span>
              </div>
            {/each}
          </div>
          <p class="card-sub">Glasses logged per day (tracked in this browser).</p>
        </div>
      </div>

    <!-- Sleep -->
    {:else if activeTab === 'sleep'}
      <div class="page sleep-page">
        <h2><img class="page-logo" src={sleepLogo} alt="Sleep" />Sleep</h2>

        <div class="card">
          <div class="card-row">
            <div>
              <h3>Status</h3>
              <p class="card-sub">Sleep control is device-only — this panel monitors the session.</p>
            </div>
            <span class="sleep-status {sleepState}">{sleepState}</span>
          </div>
        </div>

        <div class="card">
          <div class="card-row">
            <h3>Nights</h3>
            <div class="range-switch">
              <button class:active={range === '3d'} on:click={() => sleepRange('3d')}>3D</button>
              <button class:active={range === 'week'} on:click={() => sleepRange('week')}>Week</button>
              <button class:active={range === 'month'} on:click={() => sleepRange('month')}>Month</button>
            </div>
          </div>
          <div class="bar-chart" class:month={range === 'month'}>
            {#each sleepData as night}
              <div class="bar-col">
                <div class="bar-track">
                  <div class="bar sleep-bar" style="height: {Math.min(100, (night.hours / 12) * 100)}%"></div>
                </div>
                <span class="bar-label">{night.label}</span>
                <span class="bar-value">{night.hours.toFixed(1)}h</span>
              </div>
            {/each}
          </div>
          <p class="card-sub">Average: {weeklySleepAvg.toFixed(1)}h / night</p>
        </div>

        <div class="card analysis-card">
          <h3>Sleep Analysis</h3>
          <div class="analytics-row">
            <div class="analytics-stat">
              <span class="analytics-value">{weeklySleepAvg ? weeklySleepAvg.toFixed(1) + 'h' : '—'}</span>
              <span class="analytics-label">Average sleep duration</span>
            </div>
            <div class="analytics-stat">
              <span class="analytics-value">{fmtClock(avgBedtimeMin)}</span>
              <span class="analytics-label">Average bedtime</span>
            </div>
            <div class="analytics-stat">
              <span class="analytics-value">{fmtClock(avgWakeMin)}</span>
              <span class="analytics-label">Average wake time</span>
            </div>
          </div>
          <p class="card-sub">Averages over the nights in the selected range.</p>
        </div>

        <div class="info-box">
          <span class="info-icon">💡</span>
          <p>Adults need roughly 7–9 hours of sleep a night. Going to bed and waking up at consistent times trains your circadian rhythm, which improves sleep quality, memory, and mood.</p>
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
    <div class="overlay" on:click={() => showEventLog = false}></div>
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

  .tab-icon img {
    display: block;
    width: 18px;
    height: 18px;
    object-fit: contain;
  }

  .page-logo {
    width: 22px;
    height: 22px;
    object-fit: contain;
    vertical-align: -4px;
    margin-right: 0.35rem;
  }

  .card-logo {
    width: 14px;
    height: 14px;
    object-fit: contain;
    vertical-align: -2px;
    margin-right: 0.25rem;
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

  /* Stacked cards: consistent gaps between sections */
  .breathing-page,
  .sleep-page,
  .water-page {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
  }

  .breathing-page > h2,
  .sleep-page > h2,
  .water-page > h2 {
    margin-bottom: 0;
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

  .preset-break {
    font-size: 1rem;
    color: var(--color-text-muted);
    font-weight: 500;
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

  .exercise-card {
    display: flex;
    flex-direction: column;
    gap: 0.4rem;
    cursor: pointer;
    text-align: left;
    font: inherit;
    color: inherit;
  }

  .exercise-card.selected {
    border-color: var(--color-primary);
    background: var(--color-surface);
  }

  .exercise-timing {
    font-size: 0.8rem;
    color: var(--color-text-secondary);
    font-family: var(--font-mono);
  }

  /* Sleep page */
  .card-row {
    display: flex;
    align-items: center;
    justify-content: space-between;
    gap: 1rem;
  }

  /* Water page */
  .water-total {
    font-size: 2rem;
    font-weight: 700;
    color: var(--color-water);
    font-family: var(--font-mono);
  }

  .water-goal {
    font-size: 1rem;
    color: var(--color-text-muted);
    font-weight: 500;
  }

  .water-progress {
    height: 10px;
    background: var(--color-surface);
    border-radius: 999px;
    overflow: hidden;
    margin-top: 0.75rem;
  }

  .water-progress-fill {
    height: 100%;
    background: var(--color-water);
    border-radius: 999px;
    transition: width 0.3s ease, background 0.3s ease;
  }

  .goal-stepper {
    display: inline-flex;
    align-items: center;
    gap: 0.35rem;
  }

  .goal-input {
    width: 56px;
    padding: 0.35rem 0.4rem;
    text-align: center;
    font-family: var(--font-mono);
    font-size: 0.9rem;
    color: var(--color-text);
    background: var(--color-surface);
    border: 1px solid var(--color-border);
    border-radius: 8px;
  }

  .btn-icon-lg {
    font-size: 1.25rem;
    width: 44px;
    height: 44px;
    display: inline-flex;
    align-items: center;
    justify-content: center;
  }

  /* Range switch */
  .range-switch {
    display: inline-flex;
    gap: 2px;
    background: var(--color-surface);
    border-radius: 999px;
    padding: 2px;
  }

  .range-switch button {
    border: none;
    background: transparent;
    color: var(--color-text-muted);
    font-size: 0.7rem;
    padding: 0.25rem 0.6rem;
    border-radius: 999px;
    cursor: pointer;
    font-family: var(--font-mono);
  }

  .range-switch button.active {
    background: var(--color-primary);
    color: #fff;
  }

  /* Pomodoro analytics */
  .pomodoro-analytics {
    margin-top: 1rem;
  }

  .info-box {
    display: flex;
    align-items: flex-start;
    gap: 0.6rem;
    padding: 0.75rem;
    background: var(--color-surface);
    border: 1px solid var(--color-border);
    border-radius: 8px;
    font-size: 0.8rem;
    line-height: 1.4;
    color: var(--color-text-secondary);
  }

  .info-box p {
    margin: 0;
  }

  .info-icon {
    font-size: 1rem;
    line-height: 1.2;
    flex-shrink: 0;
  }

  .analytics-row {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 0.75rem;
    margin-top: 0.5rem;
  }

  @media (min-width: 640px) {
    .analytics-row { grid-template-columns: repeat(4, 1fr); }
  }

  .analytics-stat {
    display: flex;
    flex-direction: column;
    gap: 0.15rem;
    padding: 0.6rem;
    background: var(--color-surface);
    border-radius: 8px;
  }

  .analytics-value {
    font-size: 1.25rem;
    font-weight: 600;
    color: var(--color-timer);
    font-family: var(--font-mono);
  }

  .analytics-label {
    font-size: 0.7rem;
    color: var(--color-text-muted);
  }

  .badge.running {
    background: var(--color-success, #5E9F72);
    color: #fff;
  }

  .badge.pomodoro {
    background: var(--color-primary);
    color: #fff;
  }

  .sleep-status {
    font-family: var(--font-mono);
    font-size: 0.75rem;
    text-transform: uppercase;
    letter-spacing: 0.05em;
    padding: 0.25rem 0.6rem;
    border-radius: 999px;
    border: 1px solid var(--color-border);
    color: var(--color-text-muted);
  }

  .sleep-status.active {
    color: var(--color-primary);
    border-color: var(--color-primary);
  }

  /* Bar charts */
  .bar-chart {
    display: flex;
    align-items: flex-end;
    gap: 0.5rem;
    height: 160px;
    padding-top: 0.5rem;
  }

  .bar-chart.month {
    gap: 2px;
  }

  .bar-chart.month .bar-label,
  .bar-chart.month .bar-value {
    font-size: 0.6rem;
  }

  .bar-col {
    flex: 1;
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 0.25rem;
    height: 100%;
  }

  .bar-track {
    flex: 1;
    width: 100%;
    max-width: 48px;
    display: flex;
    align-items: flex-end;
    background: var(--color-border);
    border-radius: 4px;
    overflow: hidden;
  }

  .bar {
    width: 100%;
    background: var(--color-primary);
    border-radius: 4px 4px 0 0;
    min-height: 2px;
  }

  .bar.breathing-bar {
    background: var(--color-primary);
  }

  .bar.sleep-bar {
    background: var(--color-timer, #E0A84C);
  }

  .bar.water-bar {
    background: var(--color-water, #5FAF8B);
  }

  .bar-label {
    font-size: 0.7rem;
    color: var(--color-text-muted);
  }

  .bar-value {
    font-size: 0.7rem;
    font-family: var(--font-mono);
    color: var(--color-text-secondary);
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

