<script>
  import { fade } from 'svelte/transition';
  import plantStage1 from '../../assets/logos/plant_progression_1.png';
  import plantStage2 from '../../assets/logos/plant_progression_2.png';
  import plantStage3 from '../../assets/logos/plant_3_sprite.png';
  import InfoIcon from './InfoIcon.svelte';
  import { tamPlantInsight } from '../insights.js';

  export let state;

  const STAGES = [
    { level: 1, name: 'Sapling', img: plantStage1 },
    { level: 2, name: 'Growing plant', img: plantStage2 },
    { level: 4, name: 'Flourishing plant', img: plantStage3 },
  ];

  $: stage = state.level >= 4 ? STAGES[2] : state.level >= 2 ? STAGES[1] : STAGES[0];
  $: progressPercent = state.levelThreshold > 0 ? Math.min(100, Math.round((state.levelProgress / state.levelThreshold) * 100)) : 0;
  $: nextLevel = (state.level || 1) + 1;
</script>

<div class="plant-preview">
  <div class="section-header">
    <h3>Your plant</h3>
    <InfoIcon float={false} lines={tamPlantInsight(state.level, state.today)} title="Growing with your habits" />
  </div>

  <div class="plant-card">
    <div class="plant-chip card">
      <span class="chip-level">Level {state.level}</span>
      <span class="chip-dot">·</span>
      <span class="chip-seeds">{state.total} seeds</span>
    </div>
    <p class="plant-hint">Your plant grows as you earn seeds</p>

    {#key stage.name}
      <img class="plant-img" src={stage.img} alt={stage.name} width="120" height="120" transition:fade={{ duration: 600 }} />
    {/key}

    <div class="plant-progress card">
      <div class="progress-head">
        <strong class="stage-name">{stage.name}</strong>
        <span class="progress-meta">{state.levelProgress}/{state.levelThreshold} → Level {nextLevel}</span>
      </div>
      <div class="progress-track">
        <div class="progress-fill" style={`width: ${progressPercent}%`}></div>
      </div>
    </div>
  </div>

  <p class="card-sub plant-note">A live preview of your plant on the device — it grows as you collect seeds.</p>
</div>

<style>
  .plant-preview {
    display: flex;
    flex-direction: column;
    gap: 0.75rem;
  }

  .section-header {
    display: flex;
    justify-content: space-between;
    align-items: center;
  }

  .section-header h3 {
    font-size: 1rem;
    color: var(--color-text);
  }

  .plant-card {
    position: relative;
    width: 100%;
    max-width: 520px;
    margin: 0 auto;
    aspect-ratio: 1 / 1;
    border-radius: 16px;
    overflow: hidden;
    background: linear-gradient(180deg, #EAF4EC 0%, #DCEBDD 55%, #C5E0CD 100%);
    box-shadow: var(--shadow-md);
  }

  .plant-img {
    position: absolute;
    left: 50%;
    bottom: 0;
    transform: translateX(-50%);
    width: min(480px, 100%);
    height: auto;
    image-rendering: pixelated;
    z-index: 1;
  }

  .plant-chip {
    position: absolute;
    top: 0.75rem;
    left: 50%;
    transform: translateX(-50%);
    z-index: 2;
    display: flex;
    align-items: center;
    gap: 0.4rem;
    padding: 0.35rem 1rem;
    border-radius: 999px;
    background: var(--color-bg-card);
    box-shadow: var(--shadow-sm);
    font-size: 0.85rem;
    font-weight: 600;
    color: var(--color-text);
    white-space: nowrap;
  }

  .chip-seeds {
    color: var(--color-primary);
  }

  .chip-dot {
    color: var(--color-text-muted);
  }

  .plant-hint {
    position: absolute;
    top: 3.5rem;
    left: 0;
    right: 0;
    z-index: 2;
    text-align: center;
    font-size: 0.8rem;
    color: var(--color-text-secondary);
  }

  .plant-progress {
    position: absolute;
    left: 50%;
    bottom: 0.9rem;
    transform: translateX(-50%);
    width: min(330px, calc(100% - 2rem));
    z-index: 2;
    display: flex;
    flex-direction: column;
    gap: 0.4rem;
    border-radius: 14px;
    box-shadow: var(--shadow-md);
  }

  .progress-head {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    gap: 0.5rem;
  }

  .stage-name {
    font-size: 0.85rem;
    color: var(--color-text);
  }

  .progress-meta {
    font-size: 0.7rem;
    color: var(--color-text-secondary);
    font-family: var(--font-mono);
  }

  .progress-track {
    height: 8px;
    border-radius: 999px;
    background: var(--color-surface);
    overflow: hidden;
  }

  .progress-fill {
    height: 100%;
    border-radius: inherit;
    background: linear-gradient(90deg, var(--color-primary), var(--color-info));
    transition: width 0.4s ease;
  }

  .plant-note {
    text-align: center;
  }
</style>
