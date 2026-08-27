<script>
  import { notifications } from '../stores/notifications.js';

  let list = [];
  notifications.subscribe(v => list = v);

  function icon(feature) {
    const icons = {
      timer: '⏱', todo: '✓', water: '💧', breathing: '🌬',
      sleep: '🌙', settings: '⚙', connection: '🔗', system: '⚡'
    };
    return icons[feature] || '•';
  }

  function dismiss(id) {
    notifications.dismiss(id);
  }
</script>

<div class="toast-container">
  {#each list as n (n.id)}
    <!-- svelte-ignore a11y-click-events-have-key-events a11y-no-static-element-interactions -->
    <div class="toast" style="animation: slideIn 0.2s ease" on:click={() => dismiss(n.id)}>
      <span class="toast-icon">{icon(n.feature)}</span>
      <span class="toast-message">{n.message}</span>
    </div>
  {/each}
</div>

<style>
  .toast-container {
    position: fixed;
    top: 0.75rem;
    right: 0.75rem;
    z-index: 1000;
    display: flex;
    flex-direction: column;
    gap: 0.5rem;
    pointer-events: none;
  }

  .toast {
    display: flex;
    align-items: center;
    gap: 0.5rem;
    padding: 0.6rem 1rem;
    background: var(--color-bg-card);
    border: 1px solid var(--color-border);
    border-radius: var(--radius);
    box-shadow: var(--shadow-lg);
    cursor: pointer;
    pointer-events: auto;
    max-width: 320px;
    font-size: 0.8rem;
    color: var(--color-text);
  }

  .toast-icon {
    font-size: 1.1rem;
    flex-shrink: 0;
  }

  .toast-message {
    flex: 1;
    line-height: 1.3;
  }
</style>
