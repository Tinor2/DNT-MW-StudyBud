/* Data-aware, mental-health-oriented insight text shown in InfoIcon tooltips.
 * Each function returns an array of lines; the first line is a general
 * stress-management link, and later lines adapt to the user's real numbers. */

export function sleepInsight(avgHours) {
  const lines = [
    'Sleep is when your brain consolidates memory and resets stress hormones like cortisol.'
  ];
  if (avgHours > 0 && avgHours < 7) {
    lines.push(`You're averaging ${avgHours.toFixed(1)}h — below the 7–9h recommended range. Even 30 extra minutes measurably improves mood, patience, and focus.`);
  } else if (avgHours >= 7 && avgHours <= 9) {
    lines.push(`Your average of ${avgHours.toFixed(1)}h sits in the healthy range. Protecting the same bedtime keeps that benefit.`);
  } else if (avgHours > 9) {
    lines.push(`You're averaging ${avgHours.toFixed(1)}h — consistent, though very long nights can also disrupt your sleep rhythm.`);
  }
  return lines;
}

export function sleepGraphInsight() {
  return [
    'A regular bedtime and wake time anchor your circadian rhythm.',
    'Shifting your sleep window by even an hour between nights makes it harder to fall asleep and is linked to worse mood regulation.'
  ];
}

export function waterInsight(today, goal) {
  const lines = [
    'Even mild dehydration can show up as irritability, fatigue, and cloudy thinking.'
  ];
  if (goal > 0) {
    if (today >= goal) {
      lines.push(`You're at ${today}/${goal} today — well done. Steady hydration keeps energy and mood stable.`);
    } else {
      lines.push(`You're at ${today}/${goal} today. Spreading the remaining ${Math.max(0, goal - today)} glasses across the day beats a last-minute rush.`);
    }
  }
  return lines;
}

export function waterAnalysisInsight(avgGlasses, goalPct) {
  const lines = [
    'Your brain is ~75% water — staying hydrated supports focus, memory, and emotional stability.'
  ];
  if (goalPct > 0 && goalPct < 50) {
    lines.push(`You're meeting your goal on ${goalPct}% of active days. A glass of water first thing in the morning is an easy habit anchor.`);
  } else if (goalPct >= 50 && goalPct < 80) {
    lines.push(`You hit your goal on ${goalPct}% of active days — you're most of the way there. Linking water to an existing routine (after meals, on breaks) closes the gap.`);
  } else if (goalPct >= 80) {
    lines.push(`You're hitting your goal ${goalPct}% of the time — this habit is solidly formed.`);
  }
  return lines;
}

export function breathingInsight(avgMin, sessionsWeek) {
  const lines = [
    'Slow breathing switches on the parasympathetic nervous system — your body’s brake pedal for stress.'
  ];
  if (sessionsWeek > 0 && sessionsWeek < 3) {
    lines.push(`${sessionsWeek} session${sessionsWeek === 1 ? '' : 's'} this week. Two or three short sessions a week are enough to start lowering baseline anxiety.`);
  } else if (sessionsWeek >= 3) {
    lines.push(`${sessionsWeek} sessions this week — consistent practice is what trains a calmer baseline, not longer sessions.`);
  }
  if (avgMin > 0) {
    lines.push(`Your sessions average ${avgMin.toFixed(1)} min each — plenty to make a difference.`);
  }
  return lines;
}

/* Per-exercise guidance. Matches common pattern names; falls back to the
 * actual inhale/hold/exhale timing when the name isn't recognised. */
export function exerciseTip(ex) {
  const name = (ex.name || '').toLowerCase();
  const inhale = (ex.inhale_ms || 0) / 1000;
  const hold = (ex.hold_ms || 0) / 1000;
  const exhale = (ex.exhale_ms || 0) / 1000;
  const hold2 = (ex.hold2_ms || 0) / 1000;

  const lines = [
    'This pattern controls your breath-to-heart rhythm, which directly calms your nervous system.'
  ];

  if (name.includes('4-7-8') || name.includes('478') || name.includes('relax')) {
    lines.push(`4-7-8 is a natural sleep aid — the long exhale activates the parasympathetic nervous system. Try it before bed or when you're wound up.`);
  } else if (name.includes('box')) {
    lines.push(`Box breathing is a focus favourite — equal in/out timing steadies heart rate, making it great mid-study when your attention is fraying.`);
  } else if (name.includes('sigh') || name.includes('physiological')) {
    lines.push(`The physiological sigh (double inhale, long exhale) is the fastest known way to drop acute stress in the moment — use it before an exam or hard conversation.`);
  } else if (name.includes('coherent')) {
    lines.push(`Coherent breathing (~5s in / 5s out) trains a steady resting heart rhythm — good as a daily baseline reset rather than an emergency fix.`);
  } else if (name.includes('alternate')) {
    lines.push(`Alternate-nostril breathing balances the two sides of your autonomic nervous system and is often used to settle a racing mind.`);
  } else if (name.includes('extended') || name.includes('long exhale') || name.includes('down')) {
    lines.push(`Lengthening the exhale relative to the inhale is the key to triggering the body's relax response — use this to wind down after stress.`);
  } else if (name.includes('wim') || name.includes('hold')) {
    lines.push(`Breath-hold styles are energising, not calming — save these for a boost, and skip them close to bedtime.`);
  } else if (exhale > inhale * 1.4) {
    lines.push(`Your exhale (${exhale}s) is much longer than your inhale (${inhale}s) — a long-exhale pattern that naturally calms the nervous system. Great before bed.`);
  } else if (exhale > 0 && exhale >= inhale) {
    lines.push(`An even or slightly extended exhale balances your heart rhythm — a good all-round stress regulator.`);
  } else {
    lines.push(`A quicker pattern like this is energising and good for re-centring during the day, but less ideal right before sleep.`);
  }
  return lines;
}

export function timerInsight() {
  return [
    'Scheduled breaks stop mental fatigue from piling up.',
    'Short, regular recovery periods improve attention span and lower frustration — the brain is not built for sustained focus without rest.'
  ];
}

export function timerAnalysisInsight(sessions, minutes) {
  const lines = [
    'Tracking your focus makes progress visible, which is its own stress-reliever.'
  ];
  if (sessions > 0 && minutes > 0) {
    lines.push(`${sessions} session${sessions === 1 ? '' : 's'} and ${minutes}m of focus tracked. Consistency beats long stretches — regular short focus blocks protect you from burnout.`);
  }
  return lines;
}

export function todosInsight() {
  return [
    'Unfinished tasks keep nagging your mind (the Zeigarnik effect).',
    'Writing tasks down and completing them one at a time reduces mental clutter and gives a steady sense of control.'
  ];
}

export function tamSummaryInsight(level, today) {
  const lines = [
    'Gamifying healthy habits turns effort into visible progress, which keeps motivation up when willpower dips.'
  ];
  if (today > 0) {
    lines.push(`You earned ${today} seeds today — every point is a small self-care win stacking toward a bigger one.`);
  }
  return lines;
}

export function tamGoalsInsight(done, total) {
  const lines = [
    'Achievable daily goals build self-efficacy — the belief that you can follow through.'
  ];
  if (total > 0) {
    lines.push(`You've completed ${done} of ${total} today. Each tick releases a small reward signal that buffers against stress.`);
  }
  return lines;
}

export function tamStreaksInsight(longest) {
  const lines = [
    'Repeating a habit daily makes it automatic, so it stops costing mental energy.'
  ];
  if (longest > 0) {
    lines.push(`Your longest active streak is ${longest} day${longest === 1 ? '' : 's'} — streaks build momentum, and momentum lowers the barrier to starting.`);
  } else {
    lines.push('Build a streak by repeating one habit daily — consistency matters more than size.');
  }
  return lines;
}

export function tamPlantInsight(level, today) {
  const lines = [
    'Your plant is a living record of your consistency — it grows as your healthy habits compound.'
  ];
  if (level >= 2) {
    lines.push(`You've reached Level ${level} — your plant is in full bloom. Keep earning seeds to unlock even more growth.`);
  } else {
    lines.push(`Level ${level} — a few more seeds will take your plant to its next stage. Every habit you finish feeds it.`);
  }
  if (today > 0) {
    lines.push(`You've earned ${today} seeds today — that growth is already showing.`);
  }
  return lines;
}

export function tamHistoryInsight() {
  return [
    'Reflecting on what you’ve earned and accomplished lifts mood and self-esteem.',
    'Reviewing your reward history helps you notice which habits are working, so you can double down on the ones that feel good.'
  ];
}

function fmtClock(mins) {
  if (mins < 0) return '—';
  const m = ((Math.round(mins) % 1440) + 1440) % 1440;
  const h = Math.floor(m / 60) % 24;
  const min = Math.floor(m % 60);
  return `${String(h).padStart(2, '0')}:${String(min).padStart(2, '0')}`;
}

/* Verdict-style evaluations for the analysis cards. Each returns a grade
 * ('good' | 'ok' | 'poor' | 'none'), a short title, and how to improve. */
export function sleepEvaluation(avgHours, avgBedtimeMin, avgWakeMin) {
  if (!avgHours || avgHours <= 0) {
    return { grade: 'none', title: 'No sleep data yet', text: 'Finish a sleep session and come back — you’ll get a verdict on both duration and timing.' };
  }
  let grade, title, text;
  if (avgHours >= 7) {
    grade = 'good';
    title = 'On track';
    text = `You average ${avgHours.toFixed(1)}h a night, inside the recommended 7–9h range. Protect this window and your recovery stays solid.`;
  } else if (avgHours >= 6) {
    grade = 'ok';
    title = 'Close, but slightly short';
    text = `You average ${avgHours.toFixed(1)}h — just under the 7–9h target. Adding ~30 minutes is the highest-impact change you can make.`;
  } else {
    grade = 'poor';
    title = 'Short on sleep';
    text = `At ${avgHours.toFixed(1)}h a night you’re below the healthy range, which raises stress and dulls focus. Prioritise a 7h+ window, even if it means shifting other activities.`;
  }
  if (avgBedtimeMin >= 0) {
    const wakeHint = avgWakeMin >= 0 ? ` and waking at ${fmtClock(avgWakeMin)}` : '';
    if (avgBedtimeMin >= 60) {
      text += ` Your average bedtime of ${fmtClock(avgBedtimeMin)}${wakeHint} is late — pull it earlier in 15-minute steps to make waking up easier.`;
    } else {
      text += ` Your bedtime of ${fmtClock(avgBedtimeMin)}${wakeHint} is reasonable — the key is keeping it consistent every night.`;
    }
  }
  return { grade, title, text };
}

export function breathingEvaluation(sessionsWeek, avgMin) {
  if (!sessionsWeek || sessionsWeek <= 0) {
    return { grade: 'none', title: 'No breathing sessions yet', text: 'Try one 2-minute session today — even a single session measurably lowers stress reactivity.' };
  }
  let grade, title;
  if (sessionsWeek >= 3) {
    grade = 'good';
    title = 'Great consistency';
  } else if (sessionsWeek >= 2) {
    grade = 'ok';
    title = 'Getting there';
  } else {
    grade = 'poor';
    title = 'Just starting';
  }
  let text = `${sessionsWeek} session${sessionsWeek === 1 ? '' : 's'} this week. Two to three short sessions a week are enough to start lowering baseline anxiety.`;
  if (avgMin > 0) {
    if (avgMin >= 2) {
      text += ` Your sessions average ${avgMin.toFixed(1)} min — plenty. Keep the frequency up rather than the length.`;
    } else {
      text += ` Sessions around ${avgMin.toFixed(1)} min are fine — aim for at least 2 minutes for a measurable effect.`;
    }
  }
  return { grade, title, text };
}

export function waterEvaluation(avgGlasses, goalPct) {
  if (!avgGlasses || avgGlasses <= 0) {
    return { grade: 'none', title: 'No water data yet', text: 'Log a few glasses and this card will judge how your hydration is looking.' };
  }
  let grade, title;
  if (goalPct >= 80) {
    grade = 'good';
    title = 'Hydration is solid';
  } else if (goalPct >= 50) {
    grade = 'ok';
    title = 'Mostly hitting the mark';
  } else {
    grade = 'poor';
    title = 'Hydration needs attention';
  }
  let text = `You average ${avgGlasses.toFixed(1)} glasses/day and meet your goal on ${goalPct}% of active days.`;
  if (goalPct < 50) {
    text += ' Pair a glass with an existing routine (after meals, on breaks) so it happens automatically.';
  }
  return { grade, title, text };
}
