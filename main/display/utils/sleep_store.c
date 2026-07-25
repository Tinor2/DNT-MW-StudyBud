#include "sleep_store.h"
#include <time.h>
#include <string.h>

#define MAX_HISTORY 7

static uint32_t start_timestamp = 0;
static uint16_t history[MAX_HISTORY];
static int history_count = 0;

static int day_bucket_for_timestamp(time_t ts)
{
    struct tm tm_info;
    localtime_r(&ts, &tm_info);
    int day_of_year = tm_info.tm_yday;
    if (tm_info.tm_hour < 12) {
        day_of_year--;
        if (day_of_year < 0) day_of_year = 365;
    }
    return day_of_year;
}

void sleep_store_init(void)
{
    start_timestamp = 0;
    history_count = 0;
    memset(history, 0, sizeof(history));
}

void sleep_store_start_session(void)
{
    time_t now;
    time(&now);
    start_timestamp = (uint32_t)now;
}

uint32_t sleep_store_end_session(void)
{
    if (start_timestamp == 0) return 0;

    time_t now;
    time(&now);
    uint32_t duration_secs = (uint32_t)now - start_timestamp;
    uint16_t duration_mins = (uint16_t)(duration_secs / 60);

    int start_bucket = day_bucket_for_timestamp((time_t)start_timestamp);
    int end_bucket = day_bucket_for_timestamp(now);

    if (start_bucket == end_bucket) {
        if (history_count < MAX_HISTORY) {
            history[history_count] = duration_mins;
            history_count++;
        }
    } else {
        struct tm start_tm;
        localtime_r((time_t *)(&start_timestamp), &start_tm);
        uint32_t noon_secs;
        if (start_tm.tm_hour < 12) {
            struct tm yesterday_noon = start_tm;
            yesterday_noon.tm_hour = 12;
            yesterday_noon.tm_min = 0;
            yesterday_noon.tm_sec = 0;
            yesterday_noon.tm_mday--;
            noon_secs = (uint32_t)mktime(&yesterday_noon);
        } else {
            struct tm today_noon = start_tm;
            today_noon.tm_hour = 12;
            today_noon.tm_min = 0;
            today_noon.tm_sec = 0;
            noon_secs = (uint32_t)mktime(&today_noon);
        }
        uint16_t first_part = (uint16_t)(noon_secs > start_timestamp ?
            (noon_secs - start_timestamp) / 60 : 0);
        uint16_t second_part = (uint16_t)((uint32_t)now > noon_secs ?
            ((uint32_t)now - noon_secs) / 60 : 0);

        if (history_count < MAX_HISTORY) {
            history[history_count] = first_part;
            history_count++;
        }
        if (history_count < MAX_HISTORY) {
            history[history_count] = second_part;
            history_count++;
        }
    }

    start_timestamp = 0;
    return duration_mins;
}

float sleep_store_get_weekly_avg_hours(void)
{
    if (history_count == 0) return 0.0f;
    uint32_t total = 0;
    for (int i = 0; i < history_count; i++) {
        total += history[i];
    }
    return (float)total / (float)history_count / 60.0f;
}

void sleep_store_get_last_7_days(uint16_t out_minutes[7])
{
    memset(out_minutes, 0, 7 * sizeof(uint16_t));
    int start_idx = (history_count > MAX_HISTORY) ? (history_count - MAX_HISTORY) : 0;
    int count = (history_count > MAX_HISTORY) ? MAX_HISTORY : history_count;
    for (int i = 0; i < count; i++) {
        out_minutes[i] = history[start_idx + i];
    }
}
