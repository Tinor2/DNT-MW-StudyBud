#include "persistence.h"
#include "sd_card.h"
#include "sleep_store.h"
#include "sedentary_store.h"
#include "session_store.h"
#include "points_store.h"
#include "../app_state.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static const char *TAG = "Persistence";

#define SAVE_PATH "/sdcard/studybud.json"
#define SAVE_INTERVAL_US (5 * 1000 * 1000)
#define SAVE_BUF_SIZE 32768

static volatile bool s_dirty = false;
static esp_timer_handle_t s_save_timer = NULL;

static void json_escape(const char *in, char *out, size_t out_len)
{
    size_t j = 0;
    for (const char *p = in; *p && j < out_len - 1; p++) {
        if (*p == '"' || *p == '\\') {
            if (j + 2 >= out_len - 1) break;
            out[j++] = '\\';
            out[j++] = *p;
        } else {
            out[j++] = *p;
        }
    }
    out[j] = '\0';
}

static const char *find_field(const char *json, const char *key)
{
    char needle[64];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *pos = strstr(json, needle);
    if (!pos) return NULL;
    pos += strlen(needle);
    while (*pos == ' ' || *pos == ':' || *pos == '\n' || *pos == '\r' || *pos == '\t') pos++;
    return pos;
}

static int find_int(const char *json, const char *key, int default_val)
{
    const char *pos = find_field(json, key);
    if (!pos) return default_val;
    return atoi(pos);
}

static bool find_bool(const char *json, const char *key, bool default_val)
{
    const char *pos = find_field(json, key);
    if (!pos) return default_val;
    return (strncmp(pos, "true", 4) == 0);
}

static const char *find_string(const char *json, const char *key, char *buf, size_t buf_len, const char *def)
{
    const char *pos = find_field(json, key);
    if (!pos || *pos != '"') {
        if (def) strncpy(buf, def, buf_len);
        return def ? buf : NULL;
    }
    pos++;
    const char *end = strchr(pos, '"');
    if (!end) { if (def) strncpy(buf, def, buf_len); return def ? buf : NULL; }
    size_t len = end - pos;
    if (len >= buf_len) len = buf_len - 1;
    memcpy(buf, pos, len);
    buf[len] = '\0';
    return buf;
}

static void skip_spaces(const char **p)
{
    while (**p == ' ' || **p == '\n' || **p == '\r' || **p == '\t') (*p)++;
}

static void skip_comma(const char **p)
{
    skip_spaces(p);
    if (**p == ',') (*p)++;
    skip_spaces(p);
}

static const char *find_object_end(const char *p)
{
    bool in_string = false;
    for (; *p; p++) {
        if (in_string) {
            if (*p == '\\') { p++; continue; }
            if (*p == '"') in_string = false;
        } else {
            if (*p == '"') in_string = true;
            else if (*p == '}') return p;
        }
    }
    return NULL;
}

static bool parse_array_start(const char **p)
{
    skip_spaces(p);
    if (**p != '[') return false;
    (*p)++;
    return true;
}

static bool parse_array_end(const char **p)
{
    skip_spaces(p);
    if (**p == ']') { (*p)++; return true; }
    if (**p == ',') { (*p)++; skip_spaces(p); }
    return false;
}

static bool load_state(void)
{
    app_state_t *state = app_state_get();

    FILE *f = fopen(SAVE_PATH, "r");
    if (!f) {
        ESP_LOGW(TAG, "No state file found, using defaults");
        return false;
    }

    fseek(f, 0, SEEK_END);
    long file_len = ftell(f);
    if (file_len <= 0) {
        fclose(f);
        return false;
    }
    rewind(f);

    char *buf = (char *)malloc((size_t)file_len + 1);
    if (!buf) {
        fclose(f);
        return false;
    }

    size_t read_len = fread(buf, 1, (size_t)file_len, f);
    buf[read_len] = '\0';
    fclose(f);

    if (find_int(buf, "version", 0) < 1) {
        ESP_LOGW(TAG, "Unknown state file version");
        free(buf);
        return false;
    }

    state->todo_count = find_int(buf, "todo_count", 0);
    state->next_todo_id = find_int(buf, "next_todo_id", 1);

    const char *todos_start = find_field(buf, "todos");
    if (todos_start) {
        const char *p = strchr(todos_start, '[');
        if (p) {
            p++;
            int ti = 0;
            while (ti < MAX_TODOS) {
                skip_comma(&p);
                if (*p == ']') break;
                if (*p != '{') break;

                int id = find_int(p, "i", 0);
                if (id == 0) break;

                todo_item_t *t = &state->todos[ti];
                t->id = id;
                find_string(p, "t", t->text, MAX_TODO_LEN, "");
                t->done = find_bool(p, "d", false);
                t->priority = find_int(p, "p", 1);
                t->order = find_int(p, "o", ti);
                t->points_awarded = find_bool(p, "a", false);
                ti++;

                const char *close = find_object_end(p);
                if (!close) break;
                p = close + 1;
            }
            state->todo_count = ti;
        }
    }

    state->preset_count = find_int(buf, "preset_count", 0);
    state->next_preset_id = find_int(buf, "next_preset_id", 1);
    state->active_preset_id = find_int(buf, "active_preset_id", -1);

    const char *presets_start = find_field(buf, "presets");
    if (presets_start) {
        const char *p = strchr(presets_start, '[');
        if (p) {
            p++;
            int pi = 0;
            while (pi < MAX_PRESETS) {
                skip_comma(&p);
                if (*p == ']') break;
                if (*p != '{') break;

                int id = find_int(p, "i", 0);
                if (id == 0) break;

                preset_t *pr = &state->presets[pi];
                pr->id = id;
                find_string(p, "n", pr->name, MAX_NAME_LEN, "");
                pr->focus_ms = find_int(p, "f", 25 * 60 * 1000);
                pr->break_ms = find_int(p, "b", 5 * 60 * 1000);
                pr->is_pomodoro = find_bool(p, "p", false);
                pi++;

                const char *close = find_object_end(p);
                if (!close) break;
                p = close + 1;
            }
            state->preset_count = pi;
        }
    }

    state->exercise_count = find_int(buf, "exercise_count", 0);

    const char *exercises_start = find_field(buf, "exercises");
    if (exercises_start) {
        const char *p = strchr(exercises_start, '[');
        if (p) {
            p++;
            int ei = 0;
            while (ei < MAX_EXERCISES) {
                skip_comma(&p);
                if (*p == ']') break;
                if (*p != '{') break;

                int id = find_int(p, "i", 0);
                if (id == 0 && ei > 0) break;

                exercise_t *e = &state->exercises[ei];
                e->id = id;
                find_string(p, "n", e->name, MAX_NAME_LEN, "");
                e->inhale_ms = find_int(p, "in", 4000);
                e->hold_ms = find_int(p, "h", 4000);
                e->exhale_ms = find_int(p, "ex", 4000);
                e->hold2_ms = find_int(p, "h2", 0);
                ei++;

                const char *close = find_object_end(p);
                if (!close) break;
                p = close + 1;
            }
            state->exercise_count = ei;
        }
    }

    const char *water_start = find_field(buf, "water");
    if (water_start) {
        state->water.glasses = find_int(water_start, "glasses", 0);
        state->water.goal = find_int(water_start, "goal", 8);
    }

    const char *settings_start = find_field(buf, "settings");
    if (settings_start) {
        state->settings.brightness = find_int(settings_start, "brightness", 80);
        state->settings.volume = find_int(settings_start, "volume", 40);
        state->settings.idle_timeout = find_int(settings_start, "idle_timeout", 60);
        state->settings.reading_light = find_int(settings_start, "reading_light", 0);
    }

    uint16_t sleep_history[7] = {0};
    int16_t sleep_starts[7] = {-1, -1, -1, -1, -1, -1, -1};
    int sleep_count = 0;
    const char *sleep_start = find_field(buf, "sleep_history");
    if (sleep_start) {
        const char *p = strchr(sleep_start, '[');
        if (p) {
            p++;
            while (sleep_count < 7) {
                skip_spaces(&p);
                if (*p == ']') break;
                if (*p == ',') p++;
                skip_spaces(&p);
                sleep_history[sleep_count] = (uint16_t)atoi(p);
                sleep_count++;
                while (*p && *p != ',' && *p != ']') p++;
            }
        }
    }
    const char *starts_start = find_field(buf, "sleep_starts");
    if (starts_start) {
        const char *p = strchr(starts_start, '[');
        if (p) {
            p++;
            int i = 0;
            while (i < 7) {
                skip_spaces(&p);
                if (*p == ']') break;
                if (*p == ',') p++;
                skip_spaces(&p);
                sleep_starts[i] = (int16_t)atoi(p);
                i++;
                while (*p && *p != ',' && *p != ']') p++;
            }
        }
    }
    sleep_store_set_history_entries(sleep_history, sleep_starts, sleep_count);

    int breath = find_int(buf, "breath_count", 0);
    session_store_set_breath_count(breath);

    const char *points_start = find_field(buf, "points");
    if (points_start) {
        points_state_t *ps = points_store_get_state();
        ps->total_points = find_int(points_start, "total", 0);
        ps->today_points = find_int(points_start, "today", 0);
        find_string(points_start, "day", ps->day_key, sizeof(ps->day_key), "");
        ps->last_breathing_ts = (int64_t)find_int(points_start, "bt", 0);

        const char *bt = find_field(points_start, "bedtime");
        if (bt) {
            ps->bedtime_hour = find_int(bt, "hour", 23);
            ps->bedtime_min = find_int(bt, "min", 30);
        }

        const char *ct = find_field(points_start, "counts");
        if (ct) {
            ps->water_today = find_int(ct, "water", 0);
            ps->focus_today = find_int(ct, "focus", 0);
            ps->breathing_today = find_int(ct, "breathing", 0);
            ps->todos_done_today = find_int(ct, "todos", 0);
        }

        const char *bn = find_field(points_start, "bonus");
        if (bn) {
            ps->water_bonus_claimed = find_bool(bn, "water", false);
            ps->bedtime_bonus_claimed = find_bool(bn, "bedtime", false);
            ps->all_goals_bonus_claimed = find_bool(bn, "all_goals", false);
        }

        const char *goals_start = find_field(points_start, "goals");
        if (goals_start) {
            const char *p = strchr(goals_start, '[');
            if (p) {
                p++;
                int gi = 0;
                while (gi < MAX_DAILY_GOALS) {
                    skip_spaces(&p);
                    if (*p == ']') break;
                    if (*p != '{') break;
                    daily_goal_t *g = &ps->goals[gi];
                    find_string(p, "label", g->label, MAX_GOAL_LEN, "");
                    g->metric = find_int(p, "metric", GOAL_METRIC_NONE);
                    g->target = find_int(p, "target", 0);
                    g->done = find_bool(p, "done", false);
                    gi++;
                    const char *close = strchr(p, '}');
                    if (!close) break;
                    p = close + 1;
                }
            }
        }

        const char *streaks_start = find_field(points_start, "streaks");
        if (streaks_start) {
            const char *p = strchr(streaks_start, '[');
            if (p) {
                p++;
                int si = 0;
                while (si < STREAK_COUNT) {
                    skip_spaces(&p);
                    if (*p == ']') break;
                    if (*p != '{') break;
                    streak_t *st = &ps->streaks[si];
                    st->streak = find_int(p, "streak", 0);
                    find_string(p, "last", st->last_active, sizeof(st->last_active), "");
                    si++;
                    const char *close = strchr(p, '}');
                    if (!close) break;
                    p = close + 1;
                }
            }
        }

        const char *hist_start = find_field(points_start, "history");
        if (hist_start) {
            const char *p = strchr(hist_start, '[');
            if (p) {
                p++;
                int hi = 0;
                while (hi < POINT_HISTORY_LEN) {
                    skip_spaces(&p);
                    if (*p == ']') break;
                    if (*p != '{') break;
                    point_event_t *ev = &ps->history[hi];
                    ev->amount = find_int(p, "amount", 0);
                    ev->reason = find_int(p, "reason", 0);
                    ev->detail = find_int(p, "detail", 0);
                    find_string(p, "day", ev->day_key, sizeof(ev->day_key), "");
                    ev->timestamp = (int64_t)find_int(p, "ts", 0);
                    hi++;
                    const char *close = strchr(p, '}');
                    if (!close) break;
                    p = close + 1;
                }
                ps->history_count = find_int(hist_start, "history_count", hi);
                if (ps->history_count > hi) ps->history_count = hi;
            }
        }
    }

    const char *sed_start = find_field(buf, "sedentary");
    if (sed_start) {
        sedentary_state_t *sed = sedentary_store_get_state();
        sed->enabled = find_bool(sed_start, "enabled", true);
        sed->interval_min = find_int(sed_start, "interval_min", SEDENTARY_DEFAULT_INTERVAL_MIN);
        sed->quiet_start_min = find_int(sed_start, "quiet_start_min", 23 * 60);
        sed->quiet_end_min = find_int(sed_start, "quiet_end_min", 7 * 60);

        const char *ex_start = find_field(sed_start, "exercises");
        if (ex_start) {
            const char *p = strchr(ex_start, '[');
            if (p) {
                char names[SEDENTARY_MAX_EXERCISES][SEDENTARY_EXERCISE_LEN];
                const char *name_ptrs[SEDENTARY_MAX_EXERCISES];
                int count = 0;
                p++;
                while (count < SEDENTARY_MAX_EXERCISES) {
                    skip_comma(&p);
                    if (*p == ']') break;
                    if (*p != '"') break;
                    p++;
                    const char *end = strchr(p, '"');
                    if (!end) break;
                    size_t len = (size_t)(end - p);
                    if (len >= SEDENTARY_EXERCISE_LEN) len = SEDENTARY_EXERCISE_LEN - 1;
                    memcpy(names[count], p, len);
                    names[count][len] = '\0';
                    name_ptrs[count] = names[count];
                    count++;
                    p = end + 1;
                }
                sedentary_store_set_exercises(name_ptrs, count);
            }
        }

        sed->ui_state = find_int(sed_start, "ui_state", SEDENTARY_UI_IDLE);
        sed->running = find_bool(sed_start, "running", false);
        sed->remaining_sec = find_int(sed_start, "remaining_sec", 0);
        sed->total_sec = find_int(sed_start, "total_sec", 0);
        sed->end_epoch = (int64_t)find_int(sed_start, "end_epoch", 0);
        sed->snoozing = find_bool(sed_start, "snoozing", false);
        sed->snooze_until = (int64_t)find_int(sed_start, "snooze_until", 0);
        find_string(sed_start, "day", sed->day_key, sizeof(sed->day_key), "");
        sed->breaks_today = find_int(sed_start, "breaks_today", 0);
        sed->rewarded_today = find_int(sed_start, "rewarded_today", 0);
        sed->last_exercise_idx = find_int(sed_start, "last_exercise_idx", -1);

        if (sed->running && sed->end_epoch > 0) {
            time_t now;
            time(&now);
            int64_t remaining = sed->end_epoch - (int64_t)now;
            if (remaining <= 0) {
                sed->remaining_sec = 0;
                sed->running = false;
                sed->pending_alert = true;
            } else {
                sed->remaining_sec = (int)remaining;
            }
        }
    }

    ESP_LOGI(TAG, "State loaded: %d todos, %d presets, %d exercises, %d sleep entries",
             state->todo_count, state->preset_count, state->exercise_count, sleep_count);

    free(buf);
    return true;
}

bool persistence_save(void)
{
    app_state_t *state = app_state_get();
    char esc[256];

    char *buf = (char *)malloc(SAVE_BUF_SIZE);
    if (!buf) return false;

    int off = 0;
    off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                    "{\"version\":3,");

    off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                    "\"todo_count\":%d,\"next_todo_id\":%d,\"todos\":[",
                    state->todo_count, state->next_todo_id);
    for (int i = 0; i < state->todo_count && off < SAVE_BUF_SIZE - 500; i++) {
        if (i > 0) off += snprintf(buf + off, SAVE_BUF_SIZE - off, ",");
        json_escape(state->todos[i].text, esc, sizeof(esc));
        off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                        "{\"i\":%d,\"t\":\"%s\",\"d\":%s,\"p\":%d,\"o\":%d,\"a\":%s}",
                        state->todos[i].id, esc,
                        state->todos[i].done ? "true" : "false",
                        state->todos[i].priority, state->todos[i].order,
                        state->todos[i].points_awarded ? "true" : "false");
    }
    off += snprintf(buf + off, SAVE_BUF_SIZE - off, "],");

    off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                    "\"preset_count\":%d,\"next_preset_id\":%d,\"active_preset_id\":%d,\"presets\":[",
                    state->preset_count, state->next_preset_id, state->active_preset_id);
    for (int i = 0; i < state->preset_count && off < SAVE_BUF_SIZE - 500; i++) {
        if (i > 0) off += snprintf(buf + off, SAVE_BUF_SIZE - off, ",");
        json_escape(state->presets[i].name, esc, sizeof(esc));
        off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                        "{\"i\":%d,\"n\":\"%s\",\"f\":%d,\"b\":%d,\"p\":%s}",
                        state->presets[i].id, esc,
                        state->presets[i].focus_ms, state->presets[i].break_ms,
                        state->presets[i].is_pomodoro ? "true" : "false");
    }
    off += snprintf(buf + off, SAVE_BUF_SIZE - off, "],");

    off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                    "\"exercise_count\":%d,\"exercises\":[",
                    state->exercise_count);
    for (int i = 0; i < state->exercise_count && off < SAVE_BUF_SIZE - 500; i++) {
        if (i > 0) off += snprintf(buf + off, SAVE_BUF_SIZE - off, ",");
        json_escape(state->exercises[i].name, esc, sizeof(esc));
        off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                        "{\"i\":%d,\"n\":\"%s\",\"in\":%d,\"h\":%d,\"ex\":%d,\"h2\":%d}",
                        state->exercises[i].id, esc,
                        state->exercises[i].inhale_ms, state->exercises[i].hold_ms,
                        state->exercises[i].exhale_ms, state->exercises[i].hold2_ms);
    }
    off += snprintf(buf + off, SAVE_BUF_SIZE - off, "],");

    off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                    "\"water\":{\"glasses\":%d,\"goal\":%d},",
                    state->water.glasses, state->water.goal);

    off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                    "\"settings\":{\"brightness\":%d,\"volume\":%d,\"idle_timeout\":%d,\"reading_light\":%d},",
                    state->settings.brightness, state->settings.volume,
                    state->settings.idle_timeout, state->settings.reading_light);

    uint16_t sleep_history[7];
    int16_t sleep_starts[7];
    sleep_store_get_last_7_entries(sleep_history, sleep_starts);
    off += snprintf(buf + off, SAVE_BUF_SIZE - off, "\"sleep_history\":[");
    for (int i = 0; i < 7 && (sleep_history[i] > 0 || sleep_starts[i] >= 0); i++) {
        if (i > 0) off += snprintf(buf + off, SAVE_BUF_SIZE - off, ",");
        off += snprintf(buf + off, SAVE_BUF_SIZE - off, "%u", sleep_history[i]);
    }
    off += snprintf(buf + off, SAVE_BUF_SIZE - off, "],\"sleep_starts\":[");
    for (int i = 0; i < 7 && (sleep_history[i] > 0 || sleep_starts[i] >= 0); i++) {
        if (i > 0) off += snprintf(buf + off, SAVE_BUF_SIZE - off, ",");
        off += snprintf(buf + off, SAVE_BUF_SIZE - off, "%d", sleep_starts[i]);
    }
    off += snprintf(buf + off, SAVE_BUF_SIZE - off, "],");

    off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                    "\"breath_count\":%d,",
                    session_store_get_breath_count());

    points_state_t *ps = points_store_get_state();
    off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                    "\"points\":{\"total\":%d,\"today\":%d,\"day\":\"%s\",\"bt\":%lld,"
                    "\"bedtime\":{\"hour\":%d,\"min\":%d},"
                    "\"counts\":{\"water\":%d,\"focus\":%d,\"breathing\":%d,\"todos\":%d},"
                    "\"bonus\":{\"water\":%s,\"bedtime\":%s,\"all_goals\":%s},"
                    "\"goals\":[",
                    ps->total_points, ps->today_points, ps->day_key,
                    (long long)ps->last_breathing_ts,
                    ps->bedtime_hour, ps->bedtime_min,
                    ps->water_today, ps->focus_today, ps->breathing_today, ps->todos_done_today,
                    ps->water_bonus_claimed ? "true" : "false",
                    ps->bedtime_bonus_claimed ? "true" : "false",
                    ps->all_goals_bonus_claimed ? "true" : "false");
    for (int i = 0; i < MAX_DAILY_GOALS; i++) {
        json_escape(ps->goals[i].label, esc, sizeof(esc));
        off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                        "%s{\"label\":\"%s\",\"metric\":%d,\"target\":%d,\"done\":%s}",
                        i > 0 ? "," : "", esc, ps->goals[i].metric, ps->goals[i].target,
                        ps->goals[i].done ? "true" : "false");
    }
    off += snprintf(buf + off, SAVE_BUF_SIZE - off, "],\"streaks\":[");
    for (int i = 0; i < STREAK_COUNT; i++) {
        off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                        "%s{\"streak\":%d,\"last\":\"%s\"}",
                        i > 0 ? "," : "", ps->streaks[i].streak, ps->streaks[i].last_active);
    }
    off += snprintf(buf + off, SAVE_BUF_SIZE - off, "],\"history\":[");
    for (int i = 0; i < ps->history_count && i < POINT_HISTORY_LEN; i++) {
        point_event_t *ev = &ps->history[i];
        off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                        "%s{\"amount\":%d,\"reason\":%d,\"detail\":%d,\"day\":\"%s\",\"ts\":%lld}",
                        i > 0 ? "," : "", ev->amount, ev->reason, ev->detail, ev->day_key,
                        (long long)ev->timestamp);
    }
    off += snprintf(buf + off, SAVE_BUF_SIZE - off, "],\"history_count\":%d},",
                    ps->history_count);

    sedentary_state_t *sed = sedentary_store_get_state();
    off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                    "\"sedentary\":{\"enabled\":%s,\"interval_min\":%d,"
                    "\"quiet_start_min\":%d,\"quiet_end_min\":%d,"
                    "\"exercise_count\":%d,\"exercises\":[",
                    sed->enabled ? "true" : "false",
                    sed->interval_min, sed->quiet_start_min, sed->quiet_end_min,
                    sed->exercise_count);
    for (int i = 0; i < SEDENTARY_MAX_EXERCISES; i++) {
        json_escape(sed->exercises[i], esc, sizeof(esc));
        off += snprintf(buf + off, SAVE_BUF_SIZE - off, "%s\"%s\"",
                        i > 0 ? "," : "", esc);
    }
    off += snprintf(buf + off, SAVE_BUF_SIZE - off,
                    "],\"ui_state\":%d,\"running\":%s,\"remaining_sec\":%d,\"total_sec\":%d,"
                    "\"end_epoch\":%lld,\"snoozing\":%s,\"snooze_until\":%lld,"
                    "\"day\":\"%s\",\"breaks_today\":%d,\"rewarded_today\":%d,"
                    "\"last_exercise_idx\":%d},",
                    sed->ui_state,
                    sed->running ? "true" : "false",
                    sed->remaining_sec, sed->total_sec,
                    (long long)sed->end_epoch,
                    sed->snoozing ? "true" : "false",
                    (long long)sed->snooze_until,
                    sed->day_key, sed->breaks_today, sed->rewarded_today,
                    sed->last_exercise_idx);

    off += snprintf(buf + off, SAVE_BUF_SIZE - off, "\"todo_id_cnt\":%d}", state->next_todo_id);

    FILE *f = fopen(SAVE_PATH, "w");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open state file for writing");
        free(buf);
        return false;
    }

    size_t written = fwrite(buf, 1, (size_t)off, f);
    fclose(f);
    free(buf);

    if ((int)written != off) {
        ESP_LOGE(TAG, "Wrote %d of %d bytes", (int)written, off);
    }

    return true;
}

static void save_timer_cb(void *arg)
{
    (void)arg;
    if (s_dirty) {
        if (persistence_save()) {
            s_dirty = false;
            ESP_LOGI(TAG, "State saved");
        } else {
            ESP_LOGW(TAG, "Save failed, will retry");
        }
    }
}

void persistence_mark_dirty(void)
{
    s_dirty = true;
}

bool persistence_init(void)
{
    if (!sd_card_init()) {
        ESP_LOGW(TAG, "SD card not available, running without persistence");
        return false;
    }

    load_state();

    esp_timer_create_args_t timer_args = {
        .callback = save_timer_cb,
        .arg = NULL,
        .name = "persist_save",
        .skip_unhandled_events = true,
    };

    esp_err_t ret = esp_timer_create(&timer_args, &s_save_timer);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to create save timer: %d", ret);
    } else {
        esp_timer_start_periodic(s_save_timer, SAVE_INTERVAL_US);
    }

    ESP_LOGI(TAG, "Persistence initialized");
    return true;
}
