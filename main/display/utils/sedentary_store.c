#include "sedentary_store.h"
#include "points_store.h"
#include "sleep_store.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#if defined(ESP_PLATFORM)
#include "esp_log.h"
static const char *SED_TAG = "Sedentary";
#define SED_LOG(fmt, ...) ESP_LOGI(SED_TAG, fmt, ##__VA_ARGS__)
#else
#define SED_LOG(fmt, ...) ((void)0)
#endif

static sedentary_state_t s_state;

static bool time_is_set(void)
{
    time_t now;
    time(&now);
    struct tm t;
    localtime_r(&now, &t);
    return t.tm_year >= 100;
}

static int now_minutes(void)
{
    time_t now;
    time(&now);
    struct tm t;
    localtime_r(&now, &t);
    return t.tm_hour * 60 + t.tm_min;
}

static void key_for_date(int year, int mon, int day, char *out, size_t len)
{
    snprintf(out, len, "%04d-%02d-%02d", year, mon, day);
}

static void today_key(char *out, size_t len)
{
    time_t now;
    time(&now);
    struct tm t;
    localtime_r(&now, &t);
    key_for_date(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, out, len);
}

static void set_key(char *dst, size_t dst_len, const char *src)
{
    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

static bool in_quiet_window(void)
{
    if (s_state.quiet_start_min == s_state.quiet_end_min) return false;
    int now = now_minutes();
    if (s_state.quiet_start_min < s_state.quiet_end_min) {
        return now >= s_state.quiet_start_min && now < s_state.quiet_end_min;
    }
    return now >= s_state.quiet_start_min || now < s_state.quiet_end_min;
}

static void reset_run(void)
{
    s_state.running = false;
    s_state.manually_paused = false;
    s_state.remaining_sec = 0;
    s_state.end_epoch = 0;
    s_state.in_quiet = false;
    s_state.sleep_paused = false;
}

void sedentary_store_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.enabled = true;
    s_state.interval_min = SEDENTARY_DEFAULT_INTERVAL_MIN;
    s_state.quiet_start_min = 23 * 60;
    s_state.quiet_end_min = 7 * 60;
    s_state.ui_state = SEDENTARY_UI_IDLE;
    s_state.last_exercise_idx = -1;
    sedentary_store_set_exercises(
        (const char *[]){ "Walk", "Stretch", "Push-ups", "Squats" }, 4);
}

sedentary_state_t *sedentary_store_get_state(void)
{
    return &s_state;
}

/* ------------------------------------------------------------------ */
/* Config                                                              */
/* ------------------------------------------------------------------ */

void sedentary_store_set_enabled(bool enabled)
{
    s_state.enabled = enabled;
    if (!enabled) {
        reset_run();
        s_state.snoozing = false;
        s_state.snooze_until = 0;
        s_state.pending_alert = false;
        s_state.ui_state = SEDENTARY_UI_IDLE;
    }
}

void sedentary_store_set_interval(int minutes)
{
    if (minutes < 1) minutes = 1;
    if (minutes > 600) minutes = 600;
    s_state.interval_min = minutes;
}

void sedentary_store_set_quiet_hours(int start_min, int end_min)
{
    if (start_min < 0 || start_min > 1439) start_min = 23 * 60;
    if (end_min < 0 || end_min > 1439) end_min = 7 * 60;
    s_state.quiet_start_min = start_min;
    s_state.quiet_end_min = end_min;
}

void sedentary_store_set_exercises(const char *names[SEDENTARY_MAX_EXERCISES], int count)
{
    if (count < 0) count = 0;
    if (count > SEDENTARY_MAX_EXERCISES) count = SEDENTARY_MAX_EXERCISES;
    s_state.exercise_count = count;
    for (int i = 0; i < count; i++) {
        if (names[i]) set_key(s_state.exercises[i], SEDENTARY_EXERCISE_LEN, names[i]);
    }
    for (int i = count; i < SEDENTARY_MAX_EXERCISES; i++) {
        s_state.exercises[i][0] = '\0';
    }
}

bool sedentary_store_rename_exercise(int index, const char *name)
{
    if (index < 0 || index >= s_state.exercise_count) return false;
    if (!name || !name[0]) return false;
    set_key(s_state.exercises[index], SEDENTARY_EXERCISE_LEN, name);
    return true;
}

bool sedentary_store_add_exercise(const char *name)
{
    if (!name || !name[0]) return false;
    if (s_state.exercise_count >= SEDENTARY_MAX_EXERCISES) return false;
    set_key(s_state.exercises[s_state.exercise_count], SEDENTARY_EXERCISE_LEN, name);
    s_state.exercise_count++;
    return true;
}

bool sedentary_store_remove_exercise(int index)
{
    if (index < 0 || index >= s_state.exercise_count) return false;
    for (int i = index; i < s_state.exercise_count - 1; i++) {
        set_key(s_state.exercises[i], SEDENTARY_EXERCISE_LEN, s_state.exercises[i + 1]);
    }
    s_state.exercise_count--;
    s_state.exercises[s_state.exercise_count][0] = '\0';
    return true;
}

/* ------------------------------------------------------------------ */
/* Session                                                             */
/* ------------------------------------------------------------------ */

void sedentary_store_start(void)
{
    if (!s_state.enabled) return;
    time_t now;
    time(&now);

    s_state.total_sec = s_state.interval_min * 60;
    s_state.remaining_sec = s_state.total_sec;
    s_state.end_epoch = (int64_t)now + s_state.remaining_sec;
    s_state.running = true;
    s_state.manually_paused = false;
    s_state.snoozing = false;
    s_state.snooze_until = 0;
    s_state.in_quiet = false;
    s_state.sleep_paused = false;
    s_state.ui_state = SEDENTARY_UI_RUNNING;
    SED_LOG("Started %d min countdown", s_state.interval_min);
}

void sedentary_store_toggle_pause(void)
{
    if (!s_state.running) return;
    s_state.manually_paused = !s_state.manually_paused;
    if (!s_state.manually_paused) {
        time_t now;
        time(&now);
        s_state.end_epoch = (int64_t)now + s_state.remaining_sec;
    }
    SED_LOG("Manual pause %s", s_state.manually_paused ? "on" : "off");
}

void sedentary_store_snooze(void)
{
    time_t now;
    time(&now);
    s_state.snoozing = true;
    s_state.snooze_until = (int64_t)now + SEDENTARY_SNOOZE_MIN * 60;
    s_state.pending_alert = false;
    reset_run();
    s_state.ui_state = SEDENTARY_UI_IDLE;
    SED_LOG("Snoozed for %d min", SEDENTARY_SNOOZE_MIN);
}

void sedentary_store_enter_break(void)
{
    s_state.pending_alert = false;
    s_state.ui_state = SEDENTARY_UI_BREAK;
}

int sedentary_store_confirm_break(int exercise_idx)
{
    if (exercise_idx < 0 || exercise_idx >= s_state.exercise_count) return 0;

    if (s_state.breaks_today < SEDENTARY_DAILY_CAP) {
        s_state.breaks_today++;
    }
    s_state.last_exercise_idx = exercise_idx;

    int pts = points_store_award_move();
    if (s_state.rewarded_today < SEDENTARY_DAILY_CAP) {
        s_state.rewarded_today++;
    }
    s_state.ui_state = SEDENTARY_UI_SUMMARY;
    SED_LOG("Break confirmed (%d/%d, +%d seeds)", s_state.breaks_today, SEDENTARY_DAILY_CAP, pts);
    return pts;
}

void sedentary_store_acknowledge_summary(void)
{
    sedentary_store_start();
}

/* ------------------------------------------------------------------ */
/* Tick                                                                */
/* ------------------------------------------------------------------ */

void sedentary_store_tick(void)
{
    if (time_is_set()) {
        char tk[16];
        today_key(tk, sizeof(tk));
        if (strcmp(tk, s_state.day_key) != 0) {
            if (s_state.day_key[0]) {
                s_state.breaks_today = 0;
                s_state.rewarded_today = 0;
            }
            set_key(s_state.day_key, sizeof(s_state.day_key), tk);
        }
    }

    if (!s_state.enabled) {
        reset_run();
        return;
    }

    if (s_state.snoozing) {
        time_t now;
        time(&now);
        if ((int64_t)now >= s_state.snooze_until) {
            s_state.snoozing = false;
            s_state.pending_alert = true;
            SED_LOG("Snooze expired, raising alert");
        }
        return;
    }

    if (!s_state.running) return;

    s_state.in_quiet = in_quiet_window();
    s_state.sleep_paused = sleep_store_is_active();

    if (s_state.manually_paused || s_state.in_quiet || s_state.sleep_paused) {
        return;
    }

    time_t now;
    time(&now);
    int64_t remaining = s_state.end_epoch - (int64_t)now;
    if (remaining <= 0) {
        s_state.remaining_sec = 0;
        s_state.running = false;
        s_state.pending_alert = true;
        SED_LOG("Countdown finished, raising alert");
    } else {
        s_state.remaining_sec = (int)remaining;
    }
}

bool sedentary_store_consume_alert(void)
{
    bool alert = s_state.pending_alert;
    s_state.pending_alert = false;
    return alert;
}

int sedentary_store_get_remaining_sec(void)
{
    return s_state.remaining_sec;
}

int sedentary_store_get_total_sec(void)
{
    return s_state.total_sec;
}

bool sedentary_store_should_refresh(void)
{
    return s_state.ui_state == SEDENTARY_UI_RUNNING;
}
