#include "points_store.h"
#include <string.h>
#include <stdio.h>
#include <time.h>

#if defined(ESP_PLATFORM)
#include "esp_log.h"
static const char *POINTS_TAG = "Points";
#endif

static points_state_t s_state;

static bool time_is_set(void)
{
    time_t now;
    time(&now);
    struct tm t;
    localtime_r(&now, &t);
    return t.tm_year >= 100;
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

static void yesterday_key(char *out, size_t len)
{
    time_t now;
    time(&now);
    struct tm t;
    localtime_r(&now, &t);
    t.tm_mday -= 1;
    mktime(&t);
    key_for_date(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday, out, len);
}

static bool key_is_today(const char *key)
{
    if (!key[0]) return false;
    char tk[16];
    today_key(tk, sizeof(tk));
    return strcmp(key, tk) == 0;
}

static bool key_is_yesterday(const char *key)
{
    if (!key[0]) return false;
    char yk[16];
    yesterday_key(yk, sizeof(yk));
    return strcmp(key, yk) == 0;
}

static void set_key(char *dst, size_t dst_len, const char *src)
{
    strncpy(dst, src, dst_len - 1);
    dst[dst_len - 1] = '\0';
}

void points_store_init(void)
{
    memset(&s_state, 0, sizeof(s_state));
    s_state.bedtime_hour = 23;
    s_state.bedtime_min = 30;

    points_store_set_goal(0, "Drink 8 glasses of water", GOAL_METRIC_WATER, 8);
    points_store_set_goal(1, "Complete a focus session", GOAL_METRIC_FOCUS, 1);
    points_store_set_goal(2, "Do 3 breathing exercises", GOAL_METRIC_BREATHING, 3);

    points_store_rollover_if_new_day();
}

points_state_t *points_store_get_state(void)
{
    return &s_state;
}

/* ------------------------------------------------------------------ */
/* Day rollover                                                        */
/* ------------------------------------------------------------------ */

static void decay_streaks(void)
{
    for (int i = 0; i < STREAK_COUNT; i++) {
        streak_t *s = &s_state.streaks[i];
        if (s->last_active[0] && !key_is_today(s->last_active) && !key_is_yesterday(s->last_active)) {
            s->streak = 0;
            s->last_active[0] = '\0';
        }
    }
}

void points_store_rollover_if_new_day(void)
{
    if (!time_is_set()) return;

    char tk[16];
    today_key(tk, sizeof(tk));
    if (strcmp(tk, s_state.day_key) == 0) return;

    if (s_state.day_key[0]) decay_streaks();

    set_key(s_state.day_key, sizeof(s_state.day_key), tk);

    s_state.today_points = 0;
    s_state.water_today = 0;
    s_state.focus_today = 0;
    s_state.breathing_today = 0;
    s_state.todos_done_today = 0;
    s_state.moves_today = 0;
    s_state.water_bonus_claimed = false;
    s_state.bedtime_bonus_claimed = false;
    s_state.all_goals_bonus_claimed = false;

    for (int i = 0; i < MAX_DAILY_GOALS; i++) {
        s_state.goals[i].done = false;
    }
}

/* ------------------------------------------------------------------ */
/* Streak / multiplier                                                 */
/* ------------------------------------------------------------------ */

static void bump_streak(streak_activity_t act)
{
    if (act < 0 || act >= STREAK_COUNT) return;
    streak_t *s = &s_state.streaks[act];
    if (key_is_today(s->last_active)) return;

    if (key_is_yesterday(s->last_active)) {
        s->streak++;
    } else {
        s->streak = 1;
    }
    set_key(s->last_active, sizeof(s->last_active), s_state.day_key);
}

static int mult_bp(int streak)
{
    int bp = 100 + 10 * (streak - 1);
    if (bp < 100) bp = 100;
    if (bp > 200) bp = 200;
    return bp;
}

/* ------------------------------------------------------------------ */
/* History                                                             */
/* ------------------------------------------------------------------ */

#if defined(ESP_PLATFORM)
static const char *reason_name(int reason)
{
    switch (reason) {
        case POINT_REASON_TODO:        return "todo";
        case POINT_REASON_WATER:       return "water";
        case POINT_REASON_WATER_GOAL:  return "water_goal";
        case POINT_REASON_BREATHING:   return "breathing";
        case POINT_REASON_FOCUS:       return "focus";
        case POINT_REASON_BEDTIME:     return "bedtime";
        case POINT_REASON_DAILY_GOAL:  return "daily_goal";
        case POINT_REASON_ALL_GOALS:   return "all_goals";
        case POINT_REASON_ADMIN:       return "admin";
        case POINT_REASON_SLEEP:       return "sleep";
        case POINT_REASON_WATER_BREAK: return "water_break";
        case POINT_REASON_MOVE:        return "move";
        default:                       return "unknown";
    }
}
#endif

static void push_history(int amount, int reason, int detail)
{
    if (s_state.history_count < POINT_HISTORY_LEN) s_state.history_count++;
    memmove(&s_state.history[1], &s_state.history[0],
            sizeof(point_event_t) * (s_state.history_count - 1));

    point_event_t *ev = &s_state.history[0];
    ev->amount = amount;
    ev->reason = reason;
    ev->detail = detail;
    set_key(ev->day_key, sizeof(ev->day_key), s_state.day_key);
    ev->timestamp = (int64_t)time(NULL);

#if defined(ESP_PLATFORM)
    ESP_LOGI(POINTS_TAG, "%s: %+d points (detail=%d, total=%d, today=%d)",
             reason_name(reason), amount, detail, s_state.total_points, s_state.today_points);
#endif
}

/* ------------------------------------------------------------------ */
/* Award core                                                          */
/* ------------------------------------------------------------------ */

static int award_points(int base, int reason, int detail, streak_activity_t act)
{
    points_store_rollover_if_new_day();

    int pts = base;
    if (act >= 0 && act < STREAK_COUNT) {
        bump_streak(act);
        pts = (base * mult_bp(s_state.streaks[act].streak)) / 100;
    }
    if (pts < 0) pts = 0;

    s_state.total_points += pts;
    s_state.today_points += pts;
    push_history(pts, reason, detail);
    return pts;
}

static int deduct_points(int amount)
{
    if (amount <= 0) return 0;
    int deduct = amount;
    if (deduct > s_state.total_points) deduct = s_state.total_points;
    s_state.total_points -= deduct;
    if (deduct > s_state.today_points) s_state.today_points = 0;
    else s_state.today_points -= deduct;
    return deduct;
}

static int check_all_goals(void)
{
    if (s_state.all_goals_bonus_claimed) return 0;
    for (int i = 0; i < MAX_DAILY_GOALS; i++) {
        if (!s_state.goals[i].done) return 0;
    }
    s_state.all_goals_bonus_claimed = true;
    return award_points(POINTS_ALL_GOALS, POINT_REASON_ALL_GOALS, 0, STREAK_GOALS);
}

static int check_goal_links(int metric, int value)
{
    int total = 0;
    for (int i = 0; i < MAX_DAILY_GOALS; i++) {
        daily_goal_t *g = &s_state.goals[i];
        if (g->metric == metric && !g->done && g->target > 0 && value >= g->target) {
            g->done = true;
            total += award_points(POINTS_GOAL, POINT_REASON_DAILY_GOAL, i, STREAK_GOALS);
            total += check_all_goals();
        }
    }
    return total;
}

/* ------------------------------------------------------------------ */
/* Award entry points                                                  */
/* ------------------------------------------------------------------ */

int points_store_award_todo(void)
{
    points_store_rollover_if_new_day();
    s_state.todos_done_today++;
    int total = award_points(POINTS_TODO, POINT_REASON_TODO, s_state.todos_done_today, -1);
    total += check_goal_links(GOAL_METRIC_TODOS, s_state.todos_done_today);
    return total;
}

int points_store_award_water(int water_goal)
{
    points_store_rollover_if_new_day();
    s_state.water_today++;

    int total = award_points(POINTS_WATER, POINT_REASON_WATER, s_state.water_today, STREAK_WATER);

    if (!s_state.water_bonus_claimed && water_goal > 0 && s_state.water_today >= water_goal) {
        s_state.water_bonus_claimed = true;
        total += award_points(POINTS_WATER_GOAL, POINT_REASON_WATER_GOAL, water_goal, STREAK_WATER);
    }

    total += check_goal_links(GOAL_METRIC_WATER, s_state.water_today);
    return total;
}

int points_store_award_breathing(int cycles)
{
    points_store_rollover_if_new_day();
    if (cycles < 1) cycles = 1;
    if (cycles > 60) cycles = 60;

    s_state.breathing_today++;

    int64_t now = (int64_t)time(NULL);
    bool on_cooldown = (s_state.last_breathing_ts > 0 &&
                        (now - s_state.last_breathing_ts) < BREATHING_COOLDOWN_SEC);
    s_state.last_breathing_ts = now;

    int total;
    if (on_cooldown) {
        total = award_points(POINTS_BREATHING_REPEAT, POINT_REASON_BREATHING,
                             s_state.breathing_today, -1);
    } else {
        int base = POINTS_BREATHING_BASE + cycles * POINTS_BREATHING_PER_CYCLE;
        total = award_points(base, POINT_REASON_BREATHING, s_state.breathing_today, STREAK_BREATHING);
    }
    total += check_goal_links(GOAL_METRIC_BREATHING, s_state.breathing_today);
    return total;
}

int points_store_revoke_water(int count, int water_goal)
{
    points_store_rollover_if_new_day();
    if (count <= 0) return 0;

    s_state.water_today -= count;
    if (s_state.water_today < 0) s_state.water_today = 0;

    int revoked = deduct_points(POINTS_WATER * count);
    push_history(-revoked, POINT_REASON_WATER, -count);

    if (s_state.water_bonus_claimed && water_goal > 0 && s_state.water_today < water_goal) {
        s_state.water_bonus_claimed = false;
        int claw = deduct_points(POINTS_WATER_GOAL);
        push_history(-claw, POINT_REASON_WATER_GOAL, -water_goal);
        revoked += claw;
    }

    return -revoked;
}

int points_store_award_water_break(void)
{
    points_store_rollover_if_new_day();
    return award_points(POINTS_WATER_BREAK, POINT_REASON_WATER_BREAK, 0, STREAK_WATER);
}

int points_store_award_move(void)
{
    points_store_rollover_if_new_day();
    s_state.moves_today++;
    bump_streak(STREAK_MOVE);

    int pts = 0;
    if (s_state.moves_today <= POINTS_MOVE_DAILY_CAP) {
        pts = POINTS_MOVE;
        s_state.total_points += pts;
        s_state.today_points += pts;
        push_history(pts, POINT_REASON_MOVE, s_state.moves_today);
    }
    pts += check_goal_links(GOAL_METRIC_MOVE, s_state.moves_today);
    return pts;
}

int points_store_award_focus(void)
{
    points_store_rollover_if_new_day();
    s_state.focus_today++;
    int total = award_points(POINTS_FOCUS, POINT_REASON_FOCUS, s_state.focus_today, STREAK_FOCUS);
    total += check_goal_links(GOAL_METRIC_FOCUS, s_state.focus_today);
    return total;
}

int points_store_award_sleep_tracked(int duration_min)
{
    points_store_rollover_if_new_day();
    int base = POINTS_SLEEP_TRACKED;
    if (duration_min >= SLEEP_TIER_8H_MIN) base = POINTS_SLEEP_8H;
    else if (duration_min >= SLEEP_TIER_7H_MIN) base = POINTS_SLEEP_7H;
    else if (duration_min >= SLEEP_TIER_6H_MIN) base = POINTS_SLEEP_6H;
    else if (duration_min >= SLEEP_TIER_3H_MIN) base = POINTS_SLEEP_3H;
    return award_points(base, POINT_REASON_SLEEP, duration_min, STREAK_SLEEP);
}

int points_store_award_bedtime(int start_hour_min)
{
    points_store_rollover_if_new_day();
    if (s_state.bedtime_bonus_claimed) return 0;
    if (start_hour_min < 0) return 0;

    int limit = s_state.bedtime_hour * 60 + s_state.bedtime_min;
    if (start_hour_min >= limit) return 0;

    s_state.bedtime_bonus_claimed = true;
    return award_points(POINTS_BEDTIME, POINT_REASON_BEDTIME, start_hour_min, STREAK_SLEEP);
}

/* ------------------------------------------------------------------ */
/* Goals                                                               */
/* ------------------------------------------------------------------ */

bool points_store_set_goal(int index, const char *label, int metric, int target)
{
    if (index < 0 || index >= MAX_DAILY_GOALS) return false;

    daily_goal_t *g = &s_state.goals[index];
    if (label) {
        set_key(g->label, sizeof(g->label), label);
    }
    if (metric >= GOAL_METRIC_NONE && metric <= GOAL_METRIC_MOVE) {
        g->metric = metric;
    }
    if (target >= 0) {
        g->target = target;
    }
    return true;
}

int points_store_toggle_goal(int index, bool done)
{
    if (index < 0 || index >= MAX_DAILY_GOALS) return 0;

    daily_goal_t *g = &s_state.goals[index];
    if (g->done == done) return 0;

    points_store_rollover_if_new_day();
    g->done = done;

    if (done) {
        int total = award_points(POINTS_GOAL, POINT_REASON_DAILY_GOAL, index, STREAK_GOALS);
        total += check_all_goals();
        return total;
    }

    if (s_state.all_goals_bonus_claimed) {
        bool all_done = true;
        for (int i = 0; i < MAX_DAILY_GOALS; i++) {
            if (!s_state.goals[i].done) { all_done = false; break; }
        }
        if (!all_done) s_state.all_goals_bonus_claimed = false;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Bedtime                                                             */
/* ------------------------------------------------------------------ */

void points_store_get_bedtime(int *hour, int *min)
{
    if (hour) *hour = s_state.bedtime_hour;
    if (min) *min = s_state.bedtime_min;
}

void points_store_set_bedtime(int hour, int min)
{
    if (hour >= 0 && hour < 24) s_state.bedtime_hour = hour;
    if (min >= 0 && min < 60) s_state.bedtime_min = min;
}

/* ------------------------------------------------------------------ */
/* Level (placeholder curve — revisit when plant is built)             */
/* ------------------------------------------------------------------ */

#define LEVEL_BASE_XP      100
#define LEVEL_XP_INCREMENT 150

static void compute_level(int *level, int *into_level, int *threshold)
{
    int rem = s_state.total_points;
    int lv = 1;
    int need = LEVEL_BASE_XP;
    while (rem >= need) {
        rem -= need;
        lv++;
        need += LEVEL_XP_INCREMENT;
    }
    if (level) *level = lv;
    if (into_level) *into_level = rem;
    if (threshold) *threshold = need;
}

int points_store_get_level(void)
{
    int lv;
    compute_level(&lv, NULL, NULL);
    return lv;
}

int points_store_get_level_progress(void)
{
    int into;
    compute_level(NULL, &into, NULL);
    return into;
}

int points_store_get_level_threshold(void)
{
    int need;
    compute_level(NULL, NULL, &need);
    return need;
}

/* ------------------------------------------------------------------ */
/* Admin                                                               */
/* ------------------------------------------------------------------ */

int points_store_admin_add(int amount)
{
    points_store_rollover_if_new_day();
    if (amount <= 0) return 0;
    s_state.total_points += amount;
    s_state.today_points += amount;
    push_history(amount, POINT_REASON_ADMIN, amount);
    return amount;
}

int points_store_admin_sub(int amount)
{
    points_store_rollover_if_new_day();
    if (amount <= 0) return 0;
    int pts = amount;
    if (pts > s_state.total_points) pts = s_state.total_points;
    s_state.total_points -= pts;
    if (pts > s_state.today_points) s_state.today_points = 0;
    else s_state.today_points -= pts;
    push_history(-pts, POINT_REASON_ADMIN, -amount);
    return -pts;
}

void points_store_admin_reset(void)
{
    points_store_rollover_if_new_day();
    s_state.total_points = 0;
    s_state.today_points = 0;
    for (int i = 0; i < POINT_HISTORY_LEN; i++) {
        memset(&s_state.history[i], 0, sizeof(point_event_t));
    }
    s_state.history_count = 0;
    for (int i = 0; i < STREAK_COUNT; i++) {
        s_state.streaks[i].streak = 0;
        s_state.streaks[i].last_active[0] = '\0';
    }
}
