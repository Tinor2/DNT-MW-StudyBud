<script>
  export let lines = [];
  export let title = '';
  export let float = true;
</script>

<span class="info-wrap {float ? 'float' : ''}" role="button" tabindex="0" aria-label="More information">
  <span class="info-dot">ⓘ</span>
  <span class="info-tip">
    {#if title}<strong>{title}</strong>{/if}
    {#each lines as line}<p>{line}</p>{/each}
  </span>
</span>

<style>
  .info-wrap {
    position: relative;
    display: inline-flex;
    cursor: help;
    z-index: 100;
  }

  .info-wrap.float {
    position: absolute;
    top: 0.4rem;
    right: 0.4rem;
  }

  .info-dot {
    font-size: 0.95rem;
    line-height: 1;
    color: var(--color-text-muted);
    opacity: 0.85;
    padding: 2px;
  }

  .info-wrap:hover .info-dot,
  .info-wrap:focus .info-dot {
    color: var(--color-primary);
    opacity: 1;
  }

  .info-tip {
    display: none;
    position: absolute;
    top: 1.5rem;
    right: 0;
    z-index: 200;
    width: max-content;
    max-width: 260px;
    background: var(--color-bg-card);
    border: 1px solid var(--color-border);
    border-radius: 8px;
    box-shadow: var(--shadow-lg);
    padding: 0.6rem 0.75rem;
    font-size: 0.75rem;
    line-height: 1.45;
    color: var(--color-text-secondary);
    text-align: left;
    font-weight: 400;
  }

  .info-wrap:hover .info-tip,
  .info-wrap:focus .info-tip {
    display: block;
  }

  .info-tip p {
    margin: 0 0 0.4rem 0;
  }

  .info-tip p:last-child {
    margin-bottom: 0;
  }

  .info-tip strong {
    display: block;
    color: var(--color-text);
    margin-bottom: 0.25rem;
  }
</style>
