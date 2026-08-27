#include "../main/display/utils/points_store.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static time_t g_now = 1700000000;

time_t time(time_t *tp)
{
    if (tp) *tp = g_now;
    return g_now;
}

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        g_checks++;                                                            \
        if (!(cond)) {                                                         \
            g_failures++;                                                      \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
        }                                                                      \
    } while (0)

static void add_days(int days)
{
    g_now += (time_t)days * 86400;
    points_store_rollover_if_new_day();
}

static void advance_min(int minutes)
{
    g_now += (time_t)minutes * 60;
}

static void test_basics(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    CHECK(points_store_get_level() == 1);
    CHECK(points_store_award_todo() == POINTS_TODO);
    CHECK(ps->total_points == POINTS_TODO);
    CHECK(ps->today_points == POINTS_TODO);
    CHECK(ps->history_count == 1);
    CHECK(ps->history[0].reason == POINT_REASON_TODO);
    CHECK(ps->history[0].amount == POINTS_TODO);

    int w = points_store_award_water(8);
    CHECK(w == POINTS_WATER);
    CHECK(ps->water_today == 1);
    CHECK(ps->streaks[STREAK_WATER].streak == 1);
}

static void test_water_goal_bonus(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();
    int total = 0;
    for (int i = 0; i < 8; i++) {
        total += points_store_award_water(8);
    }
    CHECK(ps->water_bonus_claimed);
    CHECK(ps->water_today == 8);
    CHECK(total == 8 * POINTS_WATER + POINTS_WATER_GOAL);
    int before = ps->total_points;
    CHECK(points_store_award_water(8) == POINTS_WATER);
    CHECK(ps->total_points == before + POINTS_WATER);
}

static void test_streak_multiplier(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    points_store_award_breathing(10);
    CHECK(ps->streaks[STREAK_BREATHING].streak == 1);

    add_days(1);
    points_store_award_breathing(10);
    CHECK(ps->streaks[STREAK_BREATHING].streak == 2);

    advance_min(35);
    int pts = points_store_award_breathing(10);
    CHECK(pts == ((POINTS_BREATHING_BASE + 10 * POINTS_BREATHING_PER_CYCLE) * 110) / 100);

    add_days(2);
    points_store_award_breathing(10);
    CHECK(ps->streaks[STREAK_BREATHING].streak == 1);
}

static void test_breathing_cooldown(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    int first = points_store_award_breathing(10);
    CHECK(first == POINTS_BREATHING_BASE + 10 * POINTS_BREATHING_PER_CYCLE);

    int repeat = points_store_award_breathing(10);
    CHECK(repeat == POINTS_BREATHING_REPEAT);
    CHECK(ps->streaks[STREAK_BREATHING].streak == 1);

    advance_min(35);
    int later = points_store_award_breathing(10);
    CHECK(later == POINTS_BREATHING_BASE + 10 * POINTS_BREATHING_PER_CYCLE);
}

static void test_water_revoke(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    points_store_award_water(8);
    points_store_award_water(8);
    CHECK(ps->total_points == 2 * POINTS_WATER);
    CHECK(ps->water_today == 2);

    int r = points_store_revoke_water(1, 8);
    CHECK(r == -POINTS_WATER);
    CHECK(ps->total_points == POINTS_WATER);
    CHECK(ps->water_today == 1);

    points_store_revoke_water(5, 8);
    CHECK(ps->total_points == 0);
    CHECK(ps->water_today == 0);

    CHECK(points_store_revoke_water(0, 8) == 0);
}

static void test_water_goal_clawback(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    for (int i = 0; i < 8; i++) points_store_award_water(8);
    CHECK(ps->water_bonus_claimed);
    CHECK(ps->total_points == 8 * POINTS_WATER + POINTS_WATER_GOAL);

    int r = points_store_revoke_water(1, 8);
    CHECK(r == -(POINTS_WATER + POINTS_WATER_GOAL));
    CHECK(ps->water_bonus_claimed == false);
    CHECK(ps->water_today == 7);
    CHECK(ps->history[0].reason == POINT_REASON_WATER_GOAL);
    CHECK(ps->history[0].amount == -POINTS_WATER_GOAL);

    int w = points_store_award_water(8);
    CHECK(w == POINTS_WATER + POINTS_WATER_GOAL);
    CHECK(ps->water_bonus_claimed == true);
}

static void test_water_break_bonus(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    int b = points_store_award_water_break();
    CHECK(b == POINTS_WATER_BREAK);
    CHECK(ps->history[0].reason == POINT_REASON_WATER_BREAK);
    CHECK(ps->total_points == POINTS_WATER_BREAK);
}

static void test_sleep_tracked(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    int pts = points_store_award_sleep_tracked(30);
    CHECK(pts == POINTS_SLEEP_TRACKED);
    CHECK(ps->streaks[STREAK_SLEEP].streak == 1);
    CHECK(ps->history[0].reason == POINT_REASON_SLEEP);
}

static void test_sleep_tiers(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    CHECK(points_store_award_sleep_tracked(120) == POINTS_SLEEP_TRACKED);
    CHECK(points_store_award_sleep_tracked(180) == POINTS_SLEEP_3H);
    CHECK(points_store_award_sleep_tracked(360) == POINTS_SLEEP_6H);
    CHECK(points_store_award_sleep_tracked(420) == POINTS_SLEEP_7H);
    CHECK(points_store_award_sleep_tracked(480) == POINTS_SLEEP_8H);
    CHECK(points_store_award_sleep_tracked(600) == POINTS_SLEEP_8H);
    CHECK(ps->streaks[STREAK_SLEEP].streak == 1);
}

static void test_bedtime(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    int h, m;
    points_store_get_bedtime(&h, &m);
    CHECK(h == 23 && m == 30);

    int before = ps->total_points;
    CHECK(points_store_award_bedtime(22 * 60 + 15) == POINTS_BEDTIME);
    CHECK(ps->total_points == before + POINTS_BEDTIME);
    CHECK(ps->bedtime_bonus_claimed);

    CHECK(points_store_award_bedtime(20 * 60) == 0);

    points_store_init();
    CHECK(points_store_award_bedtime(23 * 60 + 45) == 0);
    CHECK(points_store_award_bedtime(-1) == 0);
}

static void test_goals(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    CHECK(points_store_set_goal(0, "Read", GOAL_METRIC_NONE, 0));
    CHECK(points_store_set_goal(1, "Water", GOAL_METRIC_WATER, 5));
    CHECK(points_store_set_goal(2, "Focus", GOAL_METRIC_FOCUS, 1));
    CHECK(!points_store_set_goal(5, "bad", GOAL_METRIC_NONE, 0));

    CHECK(strcmp(ps->goals[0].label, "Read") == 0);
    CHECK(ps->goals[1].metric == GOAL_METRIC_WATER);
    CHECK(ps->goals[1].target == 5);

    int pts = points_store_toggle_goal(0, true);
    CHECK(pts == POINTS_GOAL);

    for (int i = 0; i < 5; i++) points_store_award_water(8);
    CHECK(ps->goals[1].done);
    CHECK(ps->goals[2].done == false);

    points_store_award_focus();
    CHECK(ps->goals[2].done);
    CHECK(ps->all_goals_bonus_claimed);
}

static void test_all_goals_bonus_via_toggle(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    points_store_set_goal(0, "A", GOAL_METRIC_NONE, 0);
    points_store_set_goal(1, "B", GOAL_METRIC_NONE, 0);
    points_store_set_goal(2, "C", GOAL_METRIC_NONE, 0);

    points_store_toggle_goal(0, true);
    points_store_toggle_goal(1, true);
    int total = points_store_toggle_goal(2, true);
    CHECK(ps->all_goals_bonus_claimed);
    CHECK(total == POINTS_GOAL + POINTS_ALL_GOALS);

    points_store_toggle_goal(2, false);
    CHECK(ps->all_goals_bonus_claimed == false);

    int again = points_store_toggle_goal(2, true);
    CHECK(again == POINTS_GOAL + POINTS_ALL_GOALS);
}

static void test_admin(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    CHECK(points_store_admin_add(50) == 50);
    CHECK(ps->total_points == 50);
    CHECK(ps->history[0].reason == POINT_REASON_ADMIN);

    CHECK(points_store_admin_sub(20) == -20);
    CHECK(ps->total_points == 30);

    points_store_admin_reset();
    CHECK(ps->total_points == 0);
    CHECK(ps->history_count == 0);
}

static void test_level(void)
{
    points_store_init();
    for (int i = 0; i < 11; i++) points_store_award_todo();
    CHECK(points_store_get_level() == 2);
    CHECK(points_store_get_level_progress() >= 0);
    CHECK(points_store_get_level_threshold() > 0);
}

static void test_history_ring(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();
    for (int i = 0; i < POINT_HISTORY_LEN + 10; i++) {
        points_store_award_todo();
    }
    CHECK(ps->history_count == POINT_HISTORY_LEN);
}

static void test_todo_done_single_count(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();
    points_store_award_todo();
    points_store_award_todo();
    CHECK(ps->todos_done_today == 2);
    CHECK(ps->total_points == 2 * POINTS_TODO);
}

static void test_move_award(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    int total = 0;
    for (int i = 0; i < POINTS_MOVE_DAILY_CAP; i++) {
        total += points_store_award_move();
    }
    CHECK(total == POINTS_MOVE_DAILY_CAP * POINTS_MOVE);
    CHECK(ps->moves_today == POINTS_MOVE_DAILY_CAP);
    CHECK(ps->streaks[STREAK_MOVE].streak == 1);
    CHECK(ps->total_points == POINTS_MOVE_DAILY_CAP * POINTS_MOVE);
    CHECK(ps->history[0].reason == POINT_REASON_MOVE);
    CHECK(ps->history[0].amount == POINTS_MOVE);

    int capped = points_store_award_move();
    CHECK(capped == 0);
    CHECK(ps->moves_today == POINTS_MOVE_DAILY_CAP + 1);
    CHECK(ps->total_points == POINTS_MOVE_DAILY_CAP * POINTS_MOVE);

    add_days(1);
    int next = points_store_award_move();
    CHECK(next == POINTS_MOVE);
    CHECK(ps->moves_today == 1);
    CHECK(ps->streaks[STREAK_MOVE].streak == 2);
}

static void test_move_goal_link(void)
{
    points_store_init();
    points_state_t *ps = points_store_get_state();

    CHECK(points_store_set_goal(0, "Move", GOAL_METRIC_MOVE, 3));
    CHECK(ps->goals[0].metric == GOAL_METRIC_MOVE);

    points_store_award_move();
    points_store_award_move();
    CHECK(ps->goals[0].done == false);

    int pts = points_store_award_move();
    CHECK(pts == POINTS_MOVE + POINTS_GOAL);
    CHECK(ps->goals[0].done);
}

int main(void)
{
    test_basics();
    test_water_goal_bonus();
    test_streak_multiplier();
    test_breathing_cooldown();
    test_bedtime();
    test_goals();
    test_all_goals_bonus_via_toggle();
    test_admin();
    test_level();
    test_history_ring();
    test_todo_done_single_count();
    test_water_revoke();
    test_water_goal_clawback();
    test_water_break_bonus();
    test_sleep_tracked();
    test_sleep_tiers();
    test_move_award();
    test_move_goal_link();

    if (g_failures == 0) {
        printf("All %d checks passed\n", g_checks);
        return 0;
    }
    printf("%d/%d checks FAILED\n", g_failures, g_checks);
    return 1;
}
