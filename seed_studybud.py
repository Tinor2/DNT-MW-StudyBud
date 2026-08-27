#!/usr/bin/env python3
"""Generate a studybud.json seed for the ESP32 SD card.

The ESP32 is the source of truth: at boot it loads /sdcard/studybud.json
(persistence.c -> load_state) and broadcasts that state to the web app, which
overwrites any web-side seed within seconds. So a demo/mock data seed belongs
on the SD card, not in the web app.

Usage:
    python3 seed_studybud.py [output_path]

Default output: docs/TEMP/studybud_seed.json
Copy it to the SD card as: studybud.json  (path: /sdcard/studybud.json)

Important: re-run this script before each demo. The "day"/"last" fields must
equal today's date, otherwise the device's daily rollover resets today_points
and decays streaks to 0 on first broadcast.

Note: web-app daily goals are stored in the browser (localStorage) and are NOT
synced from the device yet, so the "goals" array here only affects the device's
own points state, not the web goals list.
"""

import json
import sys
import time
from datetime import date

TODAY = date.today().isoformat()

# --- Dynamic timestamps: offsets from now so the event log looks fresh ---
NOW = int(time.time())

# Reason codes (points_store.h): TODO=1 WATER=2 WATER_GOAL=3 BREATHING=4
# FOCUS=5 BEDTIME=6 DAILY_GOAL=7 ALL_GOALS=8 ADMIN=9 SLEEP=10 WATER_BREAK=11


def _ago(hours):
    """Return a Unix timestamp `hours` hours before now."""
    return NOW - int(hours * 3600)


def build_seed():
    todos = [
        {"i": 1, "t": "Complete the project proposal", "d": True, "p": 2, "o": 0, "a": True},
        {"i": 2, "t": "Finish the design review doc", "d": False, "p": 1, "o": 1, "a": False},
        {"i": 3, "t": "Call the dentist to schedule", "d": False, "p": 1, "o": 2, "a": False},
        {"i": 4, "t": "30 minutes of deep reading", "d": False, "p": 0, "o": 3, "a": False},
    ]

    presets = [
        {"i": 1, "n": "Deep Work", "f": 1500000, "b": 300000, "p": True},
        {"i": 2, "n": "Quick Sprint", "f": 600000, "b": 120000, "p": True},
        {"i": 3, "n": "Long Session", "f": 2700000, "b": 600000, "p": True},
    ]

    exercises = [
        {"i": 1, "n": "Calm Box", "in": 4000, "h": 4000, "ex": 4000, "h2": 4000},
        {"i": 2, "n": "4-7-8", "in": 4000, "h": 7000, "ex": 8000, "h2": 0},
    ]

    # Streak order matches points_store.h: focus, water, breathing, goals, sleep
    streaks = [
        {"streak": 7, "last": TODAY},   # focus
        {"streak": 12, "last": TODAY},  # water
        {"streak": 5, "last": TODAY},   # breathing
        {"streak": 3, "last": TODAY},   # goals
        {"streak": 9, "last": TODAY},   # sleep
    ]

    # History with dynamic timestamps so the event log shows realistic "2h ago" etc.
    history = [
        {"amount": 25, "reason": 5, "detail": 1, "day": TODAY, "ts": _ago(2)},
        {"amount": 20, "reason": 7, "detail": 0, "day": TODAY, "ts": _ago(3)},
        {"amount": 20, "reason": 7, "detail": 1, "day": TODAY, "ts": _ago(4)},
        {"amount": 10, "reason": 4, "detail": 6, "day": TODAY, "ts": _ago(5)},
        {"amount": 5, "reason": 2, "detail": 1, "day": TODAY, "ts": _ago(6)},
        {"amount": 50, "reason": 10, "detail": 480, "day": TODAY, "ts": _ago(8)},
        {"amount": 30, "reason": 6, "detail": 1390, "day": TODAY, "ts": _ago(10)},
        {"amount": 10, "reason": 11, "detail": 1, "day": TODAY, "ts": _ago(12)},
        {"amount": 20, "reason": 3, "detail": 8, "day": TODAY, "ts": _ago(16)},
        {"amount": 10, "reason": 1, "detail": 2, "day": TODAY, "ts": _ago(20)},
    ]

    seed = {
        "version": 2,
        "todo_count": len(todos),
        "next_todo_id": 5,
        "todos": todos,
        "preset_count": len(presets),
        "next_preset_id": 4,
        "active_preset_id": 0,
        "presets": presets,
        "exercise_count": len(exercises),
        "exercises": exercises,
        "water": {"glasses": 6, "goal": 8},
        "settings": {"brightness": 80, "volume": 40, "idle_timeout": 60},
        "sleep_history": [420, 450, 480, 360, 470, 440, 465],
        "breath_count": 27,
        "points": {
            "total": 1230,
            "today": 85,
            "day": TODAY,
            "bt": 0,
            "bedtime": {"hour": 23, "min": 30},
            "counts": {"water": 6, "focus": 2, "breathing": 3, "todos": 1},
            "bonus": {"water": True, "bedtime": False, "all_goals": False},
            "goals": [
                {"label": "Drink 8 glasses of water", "metric": 1, "target": 8, "done": True},
                {"label": "Read for 30 minutes", "metric": 0, "target": 1, "done": False},
                {"label": "10 minutes of stretching", "metric": 0, "target": 1, "done": False},
            ],
            "streaks": streaks,
            "history": history,
            "history_count": len(history),
        },
        "todo_id_cnt": 5,
    }
    return seed


def main():
    out_path = sys.argv[1] if len(sys.argv) > 1 else "docs/TEMP/studybud_seed.json"
    seed = build_seed()
    with open(out_path, "w") as f:
        json.dump(seed, f, indent=2)
        f.write("\n")
    print(f"Wrote seed to {out_path}")
    print(f"Copy it to the SD card as studybud.json (day/last = {TODAY})")
    print(f"Timestamps are relative to now ({NOW}) — re-run before each demo for fresh times.")


if __name__ == "__main__":
    main()
