#include "sedentary_store.h"
#include "points_store.h"
#include "sleep_store.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static time_t g_now = 1700000000;

time_t time(time_t *tp)
{
    if (tp) *tp = g_now;
    return g_now;
}

static time_t make_time(int year, int mon, int day, int h, int min)
{
    struct tm t;
    memset(&t, 0, sizeof(t));
    t.tm_year = year - 1900;
    t.tm_mon = mon - 1;
    t.tm_mday = day;
    t.tm_hour = h;
    t.tm_min = min;
    return mktime(&t);
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

int main(void)
{
    points_store_init();
    sedentary_store_init();
    sedentary_state_t *s = sedentary_store_get_state();

    CHECK(s->enabled);
    CHECK(s->interval_min == 60);
    CHECK(s->ui_state == SEDENTARY_UI_IDLE);

    /* disable default quiet window for base flow tests */
    sedentary_store_set_quiet_hours(0, 0);

    sedentary_store_start();
    CHECK(s->running);
    CHECK(s->ui_state == SEDENTARY_UI_RUNNING);
    CHECK(s->remaining_sec == 3600);
    CHECK(s->total_sec == 3600);

    g_now += 100;
    sedentary_store_tick();
    CHECK(s->remaining_sec == 3500);

    /* manual pause freezes */
    g_now += 500;
    sedentary_store_toggle_pause();
    CHECK(s->manually_paused);
    g_now += 1000;
    sedentary_store_tick();
    CHECK(s->remaining_sec == 3500);

    /* resume continues from remaining (elapsed while paused not counted) */
    sedentary_store_toggle_pause();
    g_now += 60;
    sedentary_store_tick();
    CHECK(s->remaining_sec == 3440);

    /* countdown finishes -> alert */
    g_now += 4000;
    sedentary_store_tick();
    CHECK(s->running == false);
    CHECK(s->pending_alert);
    CHECK(sedentary_store_consume_alert());
    CHECK(!sedentary_store_consume_alert());

    /* snooze re-fires after 10 min */
    sedentary_store_start();
    g_now += 3600;
    sedentary_store_tick();
    CHECK(s->pending_alert);
    sedentary_store_consume_alert();
    sedentary_store_snooze();
    CHECK(s->snoozing);
    CHECK(!s->pending_alert);
    g_now += 9 * 60;
    sedentary_store_tick();
    CHECK(!s->pending_alert);
    g_now += 2 * 60;
    sedentary_store_tick();
    CHECK(s->pending_alert);

    /* snooze returns to IDLE */
    sedentary_store_snooze();
    CHECK(s->ui_state == SEDENTARY_UI_IDLE);

    /* enter_break moves to BREAK (from alert Start Break) */
    sedentary_store_enter_break();
    CHECK(s->ui_state == SEDENTARY_UI_BREAK);

    /* quiet hours pause then auto-resume (wrap: 23:00 - 07:00) */
    sedentary_store_set_quiet_hours(23 * 60, 7 * 60);
    g_now = make_time(2023, 11, 14, 22, 30);
    sedentary_store_start();
    g_now += 600;
    sedentary_store_tick();
    CHECK(!s->in_quiet);
    CHECK(s->remaining_sec == 3000);
    g_now = make_time(2023, 11, 14, 23, 1);
    g_now += 10;
    sedentary_store_tick();
    CHECK(s->in_quiet);
    int frozen = s->remaining_sec;
    g_now += 3600;
    sedentary_store_tick();
    CHECK(s->remaining_sec == frozen);
    g_now = make_time(2023, 11, 15, 7, 1);
    g_now += 10;
    sedentary_store_tick();
    CHECK(s->in_quiet == false);
    CHECK(s->remaining_sec < frozen);

    /* sleep session pauses */
    g_now = make_time(2023, 11, 15, 9, 0);
    sleep_store_init();
    sleep_store_start_session();
    sedentary_store_start();
    g_now += 300;
    sedentary_store_tick();
    CHECK(s->sleep_paused);
    int sp = s->remaining_sec;
    g_now += 600;
    sedentary_store_tick();
    CHECK(s->remaining_sec == sp);
    sleep_store_end_session();
    g_now += 60;
    sedentary_store_tick();
    CHECK(!s->sleep_paused);
    CHECK(s->remaining_sec < sp);

    /* day rollover resets counters */
    g_now = make_time(2023, 11, 15, 12, 0);
    sedentary_store_init();
    sedentary_store_set_quiet_hours(0, 0);
    sedentary_store_set_exercises((const char *[]){ "Walk", "Stretch", 0, 0 }, 2);
    sedentary_store_start();
    g_now += 3600;
    sedentary_store_tick();
    sedentary_store_consume_alert();
    sedentary_store_snooze();
    sedentary_store_confirm_break(0);
    CHECK(s->breaks_today == 1);
    CHECK(s->rewarded_today == 1);
    CHECK(s->ui_state == SEDENTARY_UI_SUMMARY);
    CHECK(s->last_exercise_idx == 0);

    sedentary_store_acknowledge_summary();
    CHECK(s->ui_state == SEDENTARY_UI_RUNNING);
    CHECK(s->running);

    /* cap: 5 rewarded breaks max per day */
    g_now = make_time(2023, 11, 16, 12, 0);
    sedentary_store_init();
    sedentary_store_set_quiet_hours(0, 0);
    sedentary_store_set_exercises((const char *[]){ "Walk", 0, 0, 0 }, 1);
    int seeded = 0;
    for (int i = 0; i < 8; i++) {
        sedentary_store_start();
        g_now += 3600;
        sedentary_store_tick();
        sedentary_store_consume_alert();
        sedentary_store_snooze();
        seeded += sedentary_store_confirm_break(0);
        sedentary_store_acknowledge_summary();
    }
    CHECK(seeded == 5 * POINTS_MOVE);
    CHECK(s->breaks_today == 8);
    CHECK(s->rewarded_today == 5);

    /* master off resets a running countdown */
    sedentary_store_start();
    CHECK(s->running);
    sedentary_store_set_enabled(false);
    CHECK(s->running == false);
    CHECK(s->remaining_sec == 0);
    CHECK(s->ui_state == SEDENTARY_UI_IDLE);

    if (g_failures == 0) {
        printf("All %d sedentary checks passed\n", g_checks);
        return 0;
    }
    printf("%d/%d checks FAILED\n", g_failures, g_checks);
    return 1;
}
