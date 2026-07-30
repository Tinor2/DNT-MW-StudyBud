<script>
  import { createEventDispatcher } from 'svelte';

  export let eventLog = [];

  const dispatch = createEventDispatcher();

  let filter = 'all';
  let search = '';

  $: filtered = eventLog.filter(e => {
    if (filter !== 'all' && e.direction !== filter) return false;
    if (search && !JSON.stringify(e).toLowerCase().includes(search.toLowerCase())) return false;
    return true;
  });

  function formatTime(ts) {
    return new Date(ts).toLocaleString();
  }

  function clearLog() {
    dispatch('clear');
  }

  function exportLog() {
    const blob = new Blob([JSON.stringify(eventLog, null, 2)], { type: 'application/json' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `studybud_log_${new Date().toISOString().slice(0, 10)}.json`;
    a.click();
    URL.revokeObjectURL(url);
  }
</script>

<div class="event-log">
  <div class="log-header">
    <h2>Event Log <span class="count">({eventLog.length})</span></h2>
    <div class="header-actions">
      <button class="btn btn-ghost" on:click={exportLog}>Export</button>
      <button class="btn btn-ghost" on:click={clearLog}>Clear</button>
    </div>
  </div>

  <div class="log-controls">
    <div class="filter-row">
      <button
        class="filter-btn {filter === 'all' ? 'active' : ''}"
        on:click={() => filter = 'all'}>All</button>
      <button
        class="filter-btn {filter === 'in' ? 'active' : ''}"
        on:click={() => filter = 'in'}>Received</button>
      <button
        class="filter-btn {filter === 'out' ? 'active' : ''}"
        on:click={() => filter = 'out'}>Sent</button>
    </div>
    <input
      type="text"
      bind:value={search}
      placeholder="Search events..."
      class="search-input"
    />
  </div>

  <div class="log-entries">
    {#if filtered.length === 0}
      <div class="empty">
        <p>{eventLog.length === 0 ? 'No events yet.' : 'No matching events.'}</p>
      </div>
    {:else}
      {#each filtered as event, i}
        <div class="entry {event.direction}">
          <div class="entry-header">
            <span class="dir-badge {event.direction}">
              {event.direction === 'in' ? '←' : '→'}
            </span>
            <span class="entry-type">{event.type}</span>
            <span class="entry-time">{formatTime(event.timestamp)}</span>
          </div>
          <pre class="entry-data">{JSON.stringify(event.data, null, 2)}</pre>
        </div>
      {/each}
    {/if}
  </div>
</div>

<style>
  .event-log {
    display: flex;
    flex-direction: column;
    height: 100%;
    gap: 0.75rem;
  }

  .log-header {
    display: flex;
    align-items: center;
    justify-content: space-between;
  }

  .log-header h2 {
    font-size: 1.1rem;
    color: var(--color-text);
  }

  .count {
    font-weight: 400;
    color: var(--color-text-muted);
    font-size: 0.9rem;
  }

  .header-actions {
    display: flex;
    gap: 0.5rem;
  }

  .log-controls {
    display: flex;
    align-items: center;
    gap: 0.75rem;
    flex-wrap: wrap;
  }

  .filter-row {
    display: flex;
    gap: 0.25rem;
    background: var(--color-surface);
    border-radius: var(--radius-pill);
    padding: 2px;
  }

  .filter-btn {
    padding: 0.35rem 0.75rem;
    border-radius: var(--radius-pill);
    font-size: 0.8rem;
    background: transparent;
    color: var(--color-text-secondary);
    border: none;
    cursor: pointer;
  }

  .filter-btn.active {
    background: var(--color-bg-card);
    color: var(--color-text);
    box-shadow: var(--shadow-sm);
  }

  .search-input {
    flex: 1;
    min-width: 150px;
    padding: 0.4rem 0.75rem;
    font-size: 0.8rem;
  }

  .log-entries {
    flex: 1;
    overflow-y: auto;
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
  }

  .empty {
    display: flex;
    align-items: center;
    justify-content: center;
    padding: 2rem;
    color: var(--color-text-muted);
    font-size: 0.875rem;
  }

  .entry {
    background: var(--color-bg-card);
    border: 1px solid var(--color-border);
    border-radius: var(--radius);
    padding: 0.6rem;
  }

  .entry-header {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    margin-bottom: 0.4rem;
  }

  .dir-badge {
    display: inline-flex;
    align-items: center;
    justify-content: center;
    width: 24px;
    height: 24px;
    border-radius: 50%;
    font-size: 0.75rem;
    flex-shrink: 0;
  }

  .dir-badge.in {
    background: rgba(92, 207, 255, 0.15);
    color: #5CCFFF;
  }

  .dir-badge.out {
    background: rgba(143, 255, 143, 0.15);
    color: #8FFF8F;
  }

  .entry-type {
    font-size: 0.8rem;
    font-weight: 600;
    color: var(--color-warning);
    font-family: var(--font-mono);
  }

  .entry-time {
    font-size: 0.7rem;
    color: var(--color-text-muted);
    margin-left: auto;
  }

  .entry-data {
    font-size: 0.7rem;
    color: var(--color-text-secondary);
    background: var(--color-surface);
    padding: 0.4rem;
    border-radius: 4px;
    overflow-x: auto;
    font-family: var(--font-mono);
    line-height: 1.4;
    max-height: 200px;
    overflow-y: auto;
  }
</style>
