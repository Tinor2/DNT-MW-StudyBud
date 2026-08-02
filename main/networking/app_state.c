#include "../display/app_state.h"
#include "../display/utils/persistence.h"
#include "../display/utils/sleep_store.h"
#include "../display/utils/session_store.h"
#include "../display/utils/points_store.h"
#include "ST7701S.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "app_state";

static app_state_t s_state;
static ws_broadcast_fn s_broadcast = NULL;

static int s_last_water_glasses = -1;

static SemaphoreHandle_t s_broadcast_mutex = NULL;
static char s_broadcast_msg[MAX_BROADCAST];

static char *broadcast_lock(void)
{
    if (s_broadcast_mutex == NULL) {
        s_broadcast_mutex = xSemaphoreCreateMutex();
    }
    xSemaphoreTake(s_broadcast_mutex, portMAX_DELAY);
    return s_broadcast_msg;
}

static void broadcast_unlock(void)
{
    xSemaphoreGive(s_broadcast_mutex);
}

static const char *screen_names[] = {
    "home", "menu", "timer", "timer_presets", "timer_edit", "todos",
    "water", "breathing", "sedentary", "settings", "backgrounds",
    "notifications", "sleep", "tamagotchi"
};
#define SCREEN_NAMES_COUNT ((int)(sizeof(screen_names) / sizeof(screen_names[0])))

void app_state_set_broadcaster(ws_broadcast_fn broadcaster)
{
    s_broadcast = broadcaster;
}

void app_state_init(ws_broadcast_fn broadcaster)
{
    s_broadcast = broadcaster;
    memset(&s_state, 0, sizeof(s_state));

    s_state.todo_count = 0;
    s_state.next_todo_id = 1;

    s_state.preset_count = 3;
    s_state.next_preset_id = 4;
    strncpy(s_state.presets[0].name, "Pomodoro", MAX_NAME_LEN);
    s_state.presets[0].focus_ms = 25 * 60 * 1000;
    s_state.presets[0].break_ms = 5 * 60 * 1000;
    s_state.presets[0].is_pomodoro = true;
    s_state.presets[0].id = 1;

    strncpy(s_state.presets[1].name, "Short Focus", MAX_NAME_LEN);
    s_state.presets[1].focus_ms = 15 * 60 * 1000;
    s_state.presets[1].break_ms = 3 * 60 * 1000;
    s_state.presets[1].is_pomodoro = false;
    s_state.presets[1].id = 2;

    strncpy(s_state.presets[2].name, "Long Focus", MAX_NAME_LEN);
    s_state.presets[2].focus_ms = 50 * 60 * 1000;
    s_state.presets[2].break_ms = 10 * 60 * 1000;
    s_state.presets[2].is_pomodoro = false;
    s_state.presets[2].id = 3;

    s_state.active_preset_id = 1;

    s_state.water.glasses = 0;
    s_state.water.goal = 8;

    s_state.exercise_count = 3;
    strncpy(s_state.exercises[0].name, "4-7-8 Breathing", MAX_NAME_LEN);
    s_state.exercises[0].inhale_ms = 4000;
    s_state.exercises[0].hold_ms = 7000;
    s_state.exercises[0].exhale_ms = 8000;
    s_state.exercises[0].hold2_ms = 0;
    s_state.exercises[0].id = 1;

    strncpy(s_state.exercises[1].name, "Box Breathing", MAX_NAME_LEN);
    s_state.exercises[1].inhale_ms = 4000;
    s_state.exercises[1].hold_ms = 4000;
    s_state.exercises[1].exhale_ms = 4000;
    s_state.exercises[1].hold2_ms = 4000;
    s_state.exercises[1].id = 2;

    strncpy(s_state.exercises[2].name, "Deep Calm", MAX_NAME_LEN);
    s_state.exercises[2].inhale_ms = 5000;
    s_state.exercises[2].hold_ms = 2000;
    s_state.exercises[2].exhale_ms = 7000;
    s_state.exercises[2].hold2_ms = 0;
    s_state.exercises[2].id = 3;

    s_state.settings.brightness = 80;
    s_state.settings.volume = 40;
    s_state.settings.idle_timeout = 60;

    ESP_LOGI(TAG, "App state initialized");
}

app_state_t *app_state_get(void)
{
    return &s_state;
}

timer_state_t *app_state_get_timer(void)
{
    return &s_state.timer;
}

static const char *find_field(const char *json, const char *key)
{
    char needle[48];
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char *pos = strstr(json, needle);
    if (!pos) return NULL;
    pos += strlen(needle);
    while (*pos == ' ' || *pos == ':') pos++;
    return pos;
}

static int find_int(const char *json, const char *key, int default_val)
{
    const char *pos = find_field(json, key);
    if (!pos) return default_val;
    return atoi(pos);
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

static bool find_bool(const char *json, const char *key, bool default_val)
{
    const char *pos = find_field(json, key);
    if (!pos) return default_val;
    return (strncmp(pos, "true", 4) == 0);
}

static void broadcast_state(const char *type, const char *json_body)
{
    if (!s_broadcast) return;
    char *msg = broadcast_lock();
    snprintf(msg, MAX_BROADCAST, "{\"type\":\"%s\",%s}", type, json_body);
    s_broadcast(msg);
    broadcast_unlock();
}

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

static const char *streak_names[] = { "focus", "water", "breathing", "goals", "sleep" };
#define STREAK_NAMES_COUNT ((int)(sizeof(streak_names) / sizeof(streak_names[0])))

static int write_points_fields(char *buf, size_t len)
{
    points_state_t *ps = points_store_get_state();
    app_state_t *st = app_state_get();
    char esc[MAX_GOAL_LEN * 2];
    int off = 0;

    off += snprintf(buf + off, len - off,
                    "\"total\":%d,\"today\":%d,\"day\":\"%s\",",
                    ps->total_points, ps->today_points, ps->day_key);
    off += snprintf(buf + off, len - off,
                    "\"level\":%d,\"level_progress\":%d,\"level_threshold\":%d,",
                    points_store_get_level(),
                    points_store_get_level_progress(),
                    points_store_get_level_threshold());
    off += snprintf(buf + off, len - off,
                    "\"water_today\":%d,\"water_goal\":%d,\"water_bonus\":%s,"
                    "\"bedtime_bonus\":%s,\"bedtime\":{\"hour\":%d,\"min\":%d},\"goals\":[",
                    ps->water_today, st->water.goal,
                    ps->water_bonus_claimed ? "true" : "false",
                    ps->bedtime_bonus_claimed ? "true" : "false",
                    ps->bedtime_hour, ps->bedtime_min);
    for (int i = 0; i < MAX_DAILY_GOALS && off < (int)len - 200; i++) {
        if (i > 0) off += snprintf(buf + off, len - off, ",");
        json_escape(ps->goals[i].label, esc, sizeof(esc));
        off += snprintf(buf + off, len - off,
                        "{\"index\":%d,\"label\":\"%s\",\"metric\":%d,\"target\":%d,\"done\":%s}",
                        i, esc, ps->goals[i].metric, ps->goals[i].target,
                        ps->goals[i].done ? "true" : "false");
    }
    off += snprintf(buf + off, len - off, "],\"streaks\":[");
    for (int i = 0; i < STREAK_NAMES_COUNT && i < STREAK_COUNT; i++) {
        if (i > 0) off += snprintf(buf + off, len - off, ",");
        int mult = 100 + 10 * (ps->streaks[i].streak - 1);
        if (mult < 100) mult = 100;
        if (mult > 200) mult = 200;
        off += snprintf(buf + off, len - off,
                        "{\"activity\":\"%s\",\"days\":%d,\"multiplier\":%d}",
                        streak_names[i], ps->streaks[i].streak, mult);
    }
    off += snprintf(buf + off, len - off, "],\"history\":[");
    for (int i = 0; i < ps->history_count && i < POINT_HISTORY_LEN && off < (int)len - 200; i++) {
        if (i > 0) off += snprintf(buf + off, len - off, ",");
        point_event_t *ev = &ps->history[i];
        off += snprintf(buf + off, len - off,
                        "{\"amount\":%d,\"reason\":%d,\"detail\":%d,\"day\":\"%s\",\"ts\":%lld}",
                        ev->amount, ev->reason, ev->detail, ev->day_key,
                        (long long)ev->timestamp);
    }
    off += snprintf(buf + off, len - off, "]");
    return off;
}

void app_state_broadcast_todo_sync(void)
{
    if (!s_broadcast) return;
    char *msg = broadcast_lock();
    int off = snprintf(msg, MAX_BROADCAST, "{\"type\":\"todo_sync\",\"tasks\":[");
    char esc[MAX_TODO_LEN * 2];
    for (int i = 0; i < s_state.todo_count && off < MAX_BROADCAST - 200; i++) {
        todo_item_t *t = &s_state.todos[i];
        if (i > 0) off += snprintf(msg + off, MAX_BROADCAST - off, ",");
        json_escape(t->text, esc, sizeof(esc));
        off += snprintf(msg + off, MAX_BROADCAST - off,
                        "{\"id\":%d,\"text\":\"%s\",\"done\":%s,\"priority\":%d,\"order\":%d}",
                        t->id, esc, t->done ? "true" : "false", t->priority, t->order);
    }
    snprintf(msg + off, MAX_BROADCAST - off, "]}");
    s_broadcast(msg);
    broadcast_unlock();
}

void app_state_broadcast_sleep_state(const char *state)
{
    if (!s_broadcast) return;
    char *msg = broadcast_lock();
    snprintf(msg, MAX_BROADCAST, "{\"type\":\"sleep_state\",\"state\":\"%s\"}", state);
    s_broadcast(msg);
    broadcast_unlock();
}

void app_state_broadcast_sleep_session(int duration_min)
{
    int start = sleep_store_get_last_start_hour_min();

    if (duration_min > 0) {
        int tracked = points_store_award_sleep_tracked(duration_min);
        if (tracked > 0) {
            app_state_broadcast_points_earned(tracked, POINT_REASON_SLEEP, duration_min);
            app_state_broadcast_points_sync();
        }
    }
    if (duration_min >= MIN_SLEEP_MINUTES) {
        int pts = points_store_award_bedtime(start);
        if (pts > 0) {
            app_state_broadcast_points_earned(pts, POINT_REASON_BEDTIME, start);
            app_state_broadcast_points_sync();
        }
    }

    if (!s_broadcast) return;
    char *msg = broadcast_lock();
    if (start >= 0) {
        snprintf(msg, MAX_BROADCAST,
                 "{\"type\":\"sleep_session\",\"duration_min\":%d,\"start_hour_min\":%d}",
                 duration_min, start);
    } else {
        snprintf(msg, MAX_BROADCAST,
                 "{\"type\":\"sleep_session\",\"duration_min\":%d}",
                 duration_min);
    }
    s_broadcast(msg);
    broadcast_unlock();
}

void app_state_broadcast_breathing_complete(int cycles)
{
    int pts = points_store_award_breathing(cycles);
    if (pts > 0) {
        app_state_broadcast_points_earned(pts, POINT_REASON_BREATHING, cycles);
        app_state_broadcast_points_sync();
    }

    if (!s_broadcast) return;
    char *msg = broadcast_lock();
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"breathing_complete\",\"cycles\":%d,\"exercise_id\":%d}",
             cycles, s_state.breathing_exercise_id);
    s_broadcast(msg);
    broadcast_unlock();
}

static const char *timer_phase_names[] = { "session", "short_break", "long_break" };

static bool find_preset_by_id(int id, bool *is_pomodoro, char *name_out, size_t name_len)
{
    for (int i = 0; i < s_state.preset_count; i++) {
        if (s_state.presets[i].id == id) {
            if (is_pomodoro) *is_pomodoro = s_state.presets[i].is_pomodoro;
            if (name_out) {
                char esc[MAX_NAME_LEN * 2];
                json_escape(s_state.presets[i].name, esc, sizeof(esc));
                strncpy(name_out, esc, name_len - 1);
                name_out[name_len - 1] = '\0';
            }
            return true;
        }
    }
    return false;
}

void app_state_broadcast_timer_sync(void)
{
    if (!s_broadcast) return;
    char *msg = broadcast_lock();
    bool is_pomodoro = false;
    find_preset_by_id(s_state.timer.preset_id, &is_pomodoro, NULL, 0);
    const char *phase_name = (s_state.timer.phase >= 0 && s_state.timer.phase <= 2)
        ? timer_phase_names[s_state.timer.phase] : "session";
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"timer_update\",\"remaining_ms\":%ld,\"running\":%s,"
             "\"preset_id\":%d,\"phase\":%d,\"phase_name\":\"%s\",\"is_pomodoro\":%s,"
             "\"total_ms\":%ld,\"phase_complete\":%s}",
             (long)s_state.timer.remaining_seconds * 1000,
             (s_state.timer.is_running && !s_state.timer.phase_complete_awaiting_press) ? "true" : "false",
             s_state.timer.preset_id, s_state.timer.phase, phase_name,
             is_pomodoro ? "true" : "false",
             (long)s_state.timer.total_seconds * 1000,
             s_state.timer.phase_complete_awaiting_press ? "true" : "false");
    s_broadcast(msg);
    broadcast_unlock();
}

void app_state_broadcast_timer_session_complete(int phase, int preset_id)
{
    if (phase == TIMER_PHASE_SESSION) {
        int pts = points_store_award_focus();
        if (pts > 0) {
            app_state_broadcast_points_earned(pts, POINT_REASON_FOCUS, preset_id);
            app_state_broadcast_points_sync();
        }
    }

    if (!s_broadcast) return;
    char *msg = broadcast_lock();
    bool is_pomodoro = false;
    char esc_name[MAX_NAME_LEN * 2] = "unknown";
    find_preset_by_id(preset_id, &is_pomodoro, esc_name, sizeof(esc_name));
    const char *phase_name = (phase >= 0 && phase <= 2) ? timer_phase_names[phase] : "session";
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"timer_session_complete\",\"preset_id\":%d,\"preset_name\":\"%s\","
             "\"is_pomodoro\":%s,\"phase\":%d,\"phase_name\":\"%s\",\"duration_sec\":%ld}",
             preset_id, esc_name, is_pomodoro ? "true" : "false", phase, phase_name,
             (long)s_state.timer.total_seconds);
    s_broadcast(msg);
    broadcast_unlock();
}

void app_state_broadcast_water_sync(void)
{
    if (s_last_water_glasses < 0) {
        s_last_water_glasses = s_state.water.glasses;
    } else if (s_state.water.glasses > s_last_water_glasses) {
        int delta = s_state.water.glasses - s_last_water_glasses;
        if (delta > 16) delta = 16;
        bool in_break = (s_state.timer.phase == TIMER_PHASE_SHORT_BREAK ||
                         s_state.timer.phase == TIMER_PHASE_LONG_BREAK) &&
                        s_state.timer.is_running &&
                        !s_state.timer.phase_complete_awaiting_press;
        int pts = 0;
        int break_pts = 0;
        for (int i = 0; i < delta; i++) {
            pts += points_store_award_water(s_state.water.goal);
            if (in_break) break_pts += points_store_award_water_break();
        }
        s_last_water_glasses = s_state.water.glasses;
        if (break_pts > 0) {
            app_state_broadcast_points_earned(break_pts, POINT_REASON_WATER_BREAK, delta);
            app_state_broadcast_points_sync();
        }
        if (pts > 0) {
            app_state_broadcast_points_earned(pts, POINT_REASON_WATER, delta);
            app_state_broadcast_points_sync();
        }
    } else if (s_state.water.glasses < s_last_water_glasses) {
        int delta = s_last_water_glasses - s_state.water.glasses;
        if (delta > 16) delta = 16;
        int pts = points_store_revoke_water(delta, s_state.water.goal);
        s_last_water_glasses = s_state.water.glasses;
        if (pts < 0) {
            app_state_broadcast_points_earned(pts, POINT_REASON_WATER, -delta);
            app_state_broadcast_points_sync();
        }
    } else {
        s_last_water_glasses = s_state.water.glasses;
    }

    if (!s_broadcast) return;
    char *msg = broadcast_lock();
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"water_update\",\"glasses\":%d,\"goal\":%d}",
             s_state.water.glasses, s_state.water.goal);
    s_broadcast(msg);
    broadcast_unlock();
}

void app_state_broadcast_points_earned(int amount, int reason, int detail)
{
    persistence_mark_dirty();
    if (!s_broadcast) return;
    points_state_t *ps = points_store_get_state();
    char *msg = broadcast_lock();
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"points_earned\",\"amount\":%d,\"reason\":%d,\"detail\":%d,"
             "\"total\":%d,\"today\":%d}",
             amount, reason, detail, ps->total_points, ps->today_points);
    s_broadcast(msg);
    broadcast_unlock();
}

void app_state_broadcast_points_sync(void)
{
    if (!s_broadcast) return;
    char *msg = broadcast_lock();
    int off = snprintf(msg, MAX_BROADCAST, "{\"type\":\"points_sync\",");
    off += write_points_fields(msg + off, MAX_BROADCAST - off);
    snprintf(msg + off, MAX_BROADCAST - off, "}");
    s_broadcast(msg);
    broadcast_unlock();
}

static void broadcast_preset_sync(void)
{
    if (!s_broadcast) return;
    char *msg = broadcast_lock();
    int off = snprintf(msg, MAX_BROADCAST, "{\"type\":\"presets_sync\",\"presets\":[");
    char esc[MAX_NAME_LEN * 2];
    for (int i = 0; i < s_state.preset_count && off < MAX_BROADCAST - 200; i++) {
        preset_t *p = &s_state.presets[i];
        if (i > 0) off += snprintf(msg + off, MAX_BROADCAST - off, ",");
        json_escape(p->name, esc, sizeof(esc));
        off += snprintf(msg + off, MAX_BROADCAST - off,
                        "{\"id\":%d,\"name\":\"%s\",\"focus\":%d,\"break_duration\":%d,\"is_pomodoro\":%s}",
                        p->id, esc, p->focus_ms, p->break_ms, p->is_pomodoro ? "true" : "false");
    }
    snprintf(msg + off, MAX_BROADCAST - off, "],\"active_preset_id\":%d}", s_state.active_preset_id);
    s_broadcast(msg);
    broadcast_unlock();
}

static void broadcast_water_sync(void)
{
    app_state_broadcast_water_sync();
}

void app_state_broadcast_breathing_sync(void)
{
    if (!s_broadcast) return;
    char *msg = broadcast_lock();
    char esc[MAX_NAME_LEN * 2];
    int off = snprintf(msg, MAX_BROADCAST, "{\"type\":\"breathing_sync\",\"exercises\":[");
    for (int i = 0; i < s_state.exercise_count && off < MAX_BROADCAST - 200; i++) {
        exercise_t *e = &s_state.exercises[i];
        if (i > 0) off += snprintf(msg + off, MAX_BROADCAST - off, ",");
        json_escape(e->name, esc, sizeof(esc));
        off += snprintf(msg + off, MAX_BROADCAST - off,
                        "{\"id\":%d,\"name\":\"%s\",\"inhale\":%d,\"hold\":%d,\"exhale\":%d,\"hold2\":%d}",
                        e->id, esc, e->inhale_ms, e->hold_ms, e->exhale_ms, e->hold2_ms);
    }
    snprintf(msg + off, MAX_BROADCAST - off,
             "],\"active\":%s,\"active_id\":%d,\"sessions_today\":%d}",
             s_state.breathing_active ? "true" : "false", s_state.breathing_exercise_id,
             session_store_get_breath_count());
    s_broadcast(msg);
    broadcast_unlock();
}

void app_state_broadcast_settings_sync(void)
{
    if (!s_broadcast) return;
    char *msg = broadcast_lock();
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"settings_sync\",\"brightness\":%d,\"volume\":%d,\"idle_timeout\":%d}",
             s_state.settings.brightness,
             s_state.settings.volume,
             s_state.settings.idle_timeout);
    s_broadcast(msg);
    broadcast_unlock();
}

void app_state_broadcast_screen_change(int screen_id)
{
    if (!s_broadcast) return;
    s_state.current_screen = screen_id;
    const char *name = (screen_id >= 0 && screen_id < SCREEN_NAMES_COUNT) ? screen_names[screen_id] : "unknown";
    char *msg = broadcast_lock();
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"screen_change\",\"screen_id\":%d,\"screen\":\"%s\"}",
             screen_id, name);
    s_broadcast(msg);
    broadcast_unlock();
}

void app_state_broadcast_encoder_event(const char *direction, const char *press_state)
{
    if (!s_broadcast) return;
    char *msg = broadcast_lock();
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"encoder_event\",\"direction\":\"%s\",\"press\":\"%s\",\"screen\":%d}",
             direction, press_state, s_state.current_screen);
    s_broadcast(msg);
    broadcast_unlock();
}

void app_state_send_full_sync(char *resp, size_t resp_len)
{
    char esc[MAX_TODO_LEN * 2];
    int off = snprintf(resp, resp_len, "{\"type\":\"full_sync\",\"todos\":[");

    for (int i = 0; i < s_state.todo_count && off < (int)resp_len - 200; i++) {
        todo_item_t *t = &s_state.todos[i];
        if (i > 0) off += snprintf(resp + off, resp_len - off, ",");
        json_escape(t->text, esc, sizeof(esc));
        off += snprintf(resp + off, resp_len - off,
                        "{\"id\":%d,\"text\":\"%s\",\"done\":%s,\"priority\":%d,\"order\":%d}",
                        t->id, esc, t->done ? "true" : "false", t->priority, t->order);
    }

    off += snprintf(resp + off, resp_len - off, "],\"presets\":[");
    char esc2[MAX_NAME_LEN * 2];
    for (int i = 0; i < s_state.preset_count && off < (int)resp_len - 200; i++) {
        preset_t *p = &s_state.presets[i];
        if (i > 0) off += snprintf(resp + off, resp_len - off, ",");
        json_escape(p->name, esc2, sizeof(esc2));
        off += snprintf(resp + off, resp_len - off,
                        "{\"id\":%d,\"name\":\"%s\",\"focus\":%d,\"break_duration\":%d,\"is_pomodoro\":%s}",
                        p->id, esc2, p->focus_ms, p->break_ms, p->is_pomodoro ? "true" : "false");
    }

    bool timer_is_pomodoro = false;
    find_preset_by_id(s_state.timer.preset_id, &timer_is_pomodoro, NULL, 0);

    points_store_rollover_if_new_day();

    off += snprintf(resp + off, resp_len - off,
                    "],\"active_preset_id\":%d,"
                    "\"water\":{\"glasses\":%d,\"goal\":%d},"
                    "\"timer\":{\"remaining_ms\":%ld,\"running\":%s,\"preset_id\":%d,\"phase\":%d,"
                    "\"phase_name\":\"%s\",\"is_pomodoro\":%s,\"total_ms\":%ld,\"phase_complete\":%s},"
                    "\"settings\":{\"brightness\":%d,\"volume\":%d,\"idle_timeout\":%d},"
                    "\"current_screen\":%d,\"screen\":\"%s\","
                    "\"points\":{",
                    s_state.active_preset_id,
                    s_state.water.glasses, s_state.water.goal,
                    (long)s_state.timer.remaining_seconds * 1000,
                    (s_state.timer.is_running && !s_state.timer.phase_complete_awaiting_press) ? "true" : "false",
                    s_state.timer.preset_id, s_state.timer.phase,
                    (s_state.timer.phase >= 0 && s_state.timer.phase <= 2)
                        ? timer_phase_names[s_state.timer.phase] : "session",
                    timer_is_pomodoro ? "true" : "false",
                    (long)s_state.timer.total_seconds * 1000,
                    s_state.timer.phase_complete_awaiting_press ? "true" : "false",
                    s_state.settings.brightness,
                    s_state.settings.volume,
                    s_state.settings.idle_timeout,
                    s_state.current_screen,
                    (s_state.current_screen >= 0 && s_state.current_screen < SCREEN_NAMES_COUNT)
                        ? screen_names[s_state.current_screen] : "unknown");
    off += write_points_fields(resp + off, resp_len - off);
    snprintf(resp + off, resp_len - off, "}}");
}

static void handle_todo_add(const char *json, char *resp, size_t resp_len)
{
    if (s_state.todo_count >= MAX_TODOS) {
        snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"todo list full\"}");
        return;
    }
    todo_item_t *t = &s_state.todos[s_state.todo_count];
    t->id = s_state.next_todo_id++;
    char text_buf[MAX_TODO_LEN];
    find_string(json, "text", text_buf, MAX_TODO_LEN, "New task");
    strncpy(t->text, text_buf, MAX_TODO_LEN);
    t->done = false;
    t->priority = find_int(json, "priority", 1);
    t->order = find_int(json, "order", s_state.todo_count);
    s_state.todo_count++;

    app_state_broadcast_todo_sync();
    snprintf(resp, resp_len, "{\"type\":\"todo_sync\",\"tasks\":[");
    int roff = strlen(resp);
    char resc[MAX_TODO_LEN * 2];
    for (int ri = 0; ri < s_state.todo_count && roff < (int)resp_len - 200; ri++) {
        todo_item_t *rt = &s_state.todos[ri];
        if (ri > 0) roff += snprintf(resp + roff, resp_len - roff, ",");
        json_escape(rt->text, resc, sizeof(resc));
        roff += snprintf(resp + roff, resp_len - roff,
                        "{\"id\":%d,\"text\":\"%s\",\"done\":%s,\"priority\":%d,\"order\":%d}",
                        rt->id, resc, rt->done ? "true" : "false", rt->priority, rt->order);
    }
    snprintf(resp + roff, resp_len - roff, "]}");
    persistence_mark_dirty();
}

static void handle_todo_update(const char *json, char *resp, size_t resp_len)
{
    int id = find_int(json, "id", -1);
    for (int i = 0; i < s_state.todo_count; i++) {
        if (s_state.todos[i].id == id) {
            todo_item_t *t = &s_state.todos[i];
            char buf[MAX_TODO_LEN];
            if (find_string(json, "text", buf, MAX_TODO_LEN, NULL)) {
                strncpy(t->text, buf, MAX_TODO_LEN);
            }
            bool was_done = t->done;
            const char *done_pos = find_field(json, "done");
            if (done_pos) {
                t->done = (strncmp(done_pos, "true", 4) == 0);
            }
            const char *pri_pos = find_field(json, "priority");
            if (pri_pos) t->priority = atoi(pri_pos);

            if (!was_done && t->done && !t->points_awarded) {
                t->points_awarded = true;
                int pts = points_store_award_todo();
                app_state_broadcast_points_earned(pts, POINT_REASON_TODO, t->id);
                app_state_broadcast_points_sync();
            }

            app_state_broadcast_todo_sync();
            snprintf(resp, resp_len, "{\"type\":\"todo_sync\",\"tasks\":[");
            int roff = strlen(resp);
            char resc[MAX_TODO_LEN * 2];
            for (int ri = 0; ri < s_state.todo_count && roff < (int)resp_len - 200; ri++) {
                todo_item_t *rt = &s_state.todos[ri];
                if (ri > 0) roff += snprintf(resp + roff, resp_len - roff, ",");
                json_escape(rt->text, resc, sizeof(resc));
                roff += snprintf(resp + roff, resp_len - roff,
                                "{\"id\":%d,\"text\":\"%s\",\"done\":%s,\"priority\":%d,\"order\":%d}",
                                rt->id, resc, rt->done ? "true" : "false", rt->priority, rt->order);
            }
            snprintf(resp + roff, resp_len - roff, "]}");
            persistence_mark_dirty();
            return;
        }
    }
    snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"todo not found\"}");
}

static void handle_todo_delete(const char *json, char *resp, size_t resp_len)
{
    int id = find_int(json, "id", -1);
    for (int i = 0; i < s_state.todo_count; i++) {
        if (s_state.todos[i].id == id) {
            for (int j = i; j < s_state.todo_count - 1; j++) {
                s_state.todos[j] = s_state.todos[j + 1];
            }
            s_state.todo_count--;
            app_state_broadcast_todo_sync();
            snprintf(resp, resp_len, "{\"type\":\"todo_sync\",\"tasks\":[");
            int roff = strlen(resp);
            char resc[MAX_TODO_LEN * 2];
            for (int ri = 0; ri < s_state.todo_count && roff < (int)resp_len - 200; ri++) {
                todo_item_t *rt = &s_state.todos[ri];
                if (ri > 0) roff += snprintf(resp + roff, resp_len - roff, ",");
                json_escape(rt->text, resc, sizeof(resc));
                roff += snprintf(resp + roff, resp_len - roff,
                                "{\"id\":%d,\"text\":\"%s\",\"done\":%s,\"priority\":%d,\"order\":%d}",
                                rt->id, resc, rt->done ? "true" : "false", rt->priority, rt->order);
            }
            snprintf(resp + roff, resp_len - roff, "]}");
            persistence_mark_dirty();
            return;
        }
    }
    snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"todo not found\"}");
}

static void handle_preset_add(const char *json, char *resp, size_t resp_len)
{
    if (s_state.preset_count >= MAX_PRESETS) {
        snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"preset list full\"}");
        return;
    }
    preset_t *p = &s_state.presets[s_state.preset_count];
    p->id = s_state.next_preset_id++;
    char name_buf[MAX_NAME_LEN];
    find_string(json, "name", name_buf, MAX_NAME_LEN, "New Preset");
    strncpy(p->name, name_buf, MAX_NAME_LEN);
    p->focus_ms = find_int(json, "focus", 25 * 60 * 1000);
    p->break_ms = find_int(json, "break_duration", 5 * 60 * 1000);
    p->is_pomodoro = find_bool(json, "is_pomodoro", false);
    s_state.preset_count++;

    snprintf(resp, resp_len, "{\"type\":\"preset_added\",\"id\":%d}", p->id);
    broadcast_preset_sync();
    persistence_mark_dirty();
}

static void handle_preset_update(const char *json, char *resp, size_t resp_len)
{
    int id = find_int(json, "id", -1);
    for (int i = 0; i < s_state.preset_count; i++) {
        if (s_state.presets[i].id == id) {
            preset_t *p = &s_state.presets[i];
            char buf[MAX_NAME_LEN];
            if (find_string(json, "name", buf, MAX_NAME_LEN, NULL)) {
                strncpy(p->name, buf, MAX_NAME_LEN);
            }
            const char *f_pos = find_field(json, "focus");
            if (f_pos) p->focus_ms = atoi(f_pos);
            const char *b_pos = find_field(json, "break_duration");
            if (b_pos) p->break_ms = atoi(b_pos);
            const char *p_pos = find_field(json, "is_pomodoro");
            if (p_pos) p->is_pomodoro = (strncmp(p_pos, "true", 4) == 0);

            snprintf(resp, resp_len, "{\"type\":\"preset_updated\",\"id\":%d}", id);
            broadcast_preset_sync();
            persistence_mark_dirty();
            return;
        }
    }
    snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"preset not found\"}");
}

static void handle_preset_delete(const char *json, char *resp, size_t resp_len)
{
    int id = find_int(json, "id", -1);
    for (int i = 0; i < s_state.preset_count; i++) {
        if (s_state.presets[i].id == id) {
            for (int j = i; j < s_state.preset_count - 1; j++) {
                s_state.presets[j] = s_state.presets[j + 1];
            }
            s_state.preset_count--;
            if (s_state.active_preset_id == id && s_state.preset_count > 0) {
                s_state.active_preset_id = s_state.presets[0].id;
            }
            snprintf(resp, resp_len, "{\"type\":\"preset_deleted\",\"id\":%d}", id);
            broadcast_preset_sync();
            persistence_mark_dirty();
            return;
        }
    }
    snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"preset not found\"}");
}

static void handle_preset_select(const char *json, char *resp, size_t resp_len)
{
    int id = find_int(json, "preset_id", -1);
    for (int i = 0; i < s_state.preset_count; i++) {
        if (s_state.presets[i].id == id) {
            s_state.active_preset_id = id;
            snprintf(resp, resp_len, "{\"type\":\"preset_selected\",\"preset_id\":%d}", id);
            broadcast_preset_sync();
            persistence_mark_dirty();
            return;
        }
    }
    snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"preset not found\"}");
}

static void handle_timer_command(const char *json, char *resp, size_t resp_len)
{
    (void)json;
    snprintf(resp, resp_len,
             "{\"type\":\"error\",\"message\":\"timers are device-only - the web app can only monitor\"}");
}

static void handle_water_log(const char *json, char *resp, size_t resp_len)
{
    char action[16];
    find_string(json, "action", action, sizeof(action), "add");

    if (strcmp(action, "add") == 0) {
        s_state.water.glasses++;
    } else if (strcmp(action, "remove") == 0) {
        if (s_state.water.glasses > 0) s_state.water.glasses--;
    }

    snprintf(resp, resp_len, "{\"type\":\"water_update\",\"glasses\":%d,\"goal\":%d}",
             s_state.water.glasses, s_state.water.goal);
    broadcast_water_sync();
    persistence_mark_dirty();
}

static void handle_water_goal(const char *json, char *resp, size_t resp_len)
{
    int goal = find_int(json, "goal", 8);
    if (goal < 1) goal = 1;
    if (goal > 32) goal = 32;
    s_state.water.goal = goal;
    snprintf(resp, resp_len, "{\"type\":\"water_update\",\"glasses\":%d,\"goal\":%d}",
             s_state.water.glasses, s_state.water.goal);
    broadcast_water_sync();
    persistence_mark_dirty();
}

static void handle_breathing_start(const char *json, char *resp, size_t resp_len)
{
    (void)json;
    snprintf(resp, resp_len,
             "{\"type\":\"error\",\"message\":\"breathing control is device-only - the web app can only monitor\"}");
}

static void handle_breathing_stop(const char *json, char *resp, size_t resp_len)
{
    (void)json;
    snprintf(resp, resp_len,
             "{\"type\":\"error\",\"message\":\"breathing control is device-only - the web app can only monitor\"}");
}

static void handle_breathing_select(const char *json, char *resp, size_t resp_len)
{
    int id = find_int(json, "exercise_id", -1);
    for (int i = 0; i < s_state.exercise_count; i++) {
        if (s_state.exercises[i].id == id) {
            s_state.breathing_exercise_id = id;
            snprintf(resp, resp_len, "{\"type\":\"breathing_selected\",\"active_id\":%d}", id);
            app_state_broadcast_breathing_sync();
            return;
        }
    }
    snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"exercise not found\"}");
}

static void handle_breathing_update(const char *json, char *resp, size_t resp_len)
{
    int id = find_int(json, "exercise_id", -1);
    for (int i = 0; i < s_state.exercise_count; i++) {
        if (s_state.exercises[i].id == id) {
            exercise_t *e = &s_state.exercises[i];
            char buf[MAX_NAME_LEN];
            if (find_string(json, "name", buf, MAX_NAME_LEN, NULL)) {
                strncpy(e->name, buf, MAX_NAME_LEN);
            }
            const char *in_pos = find_field(json, "inhale");
            if (in_pos) e->inhale_ms = atoi(in_pos);
            const char *h_pos = find_field(json, "hold");
            if (h_pos) e->hold_ms = atoi(h_pos);
            const char *ex_pos = find_field(json, "exhale");
            if (ex_pos) e->exhale_ms = atoi(ex_pos);
            const char *h2_pos = find_field(json, "hold2");
            if (h2_pos) e->hold2_ms = atoi(h2_pos);

            snprintf(resp, resp_len, "{\"type\":\"breathing_updated\",\"id\":%d}", id);
            app_state_broadcast_breathing_sync();
            persistence_mark_dirty();
            return;
        }
    }
    snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"exercise not found\"}");
}

static void handle_settings_update(const char *json, char *resp, size_t resp_len)
{
    const char *b_pos = find_field(json, "brightness");
    if (b_pos) {
        s_state.settings.brightness = atoi(b_pos);
        if (s_state.settings.brightness < 0) s_state.settings.brightness = 0;
        if (s_state.settings.brightness > 100) s_state.settings.brightness = 100;
        Set_Backlight((uint8_t)s_state.settings.brightness);
    }
    const char *v_pos = find_field(json, "volume");
    if (v_pos) s_state.settings.volume = atoi(v_pos);
    const char *i_pos = find_field(json, "idle_timeout");
    if (i_pos)     s_state.settings.idle_timeout = atoi(i_pos);

    snprintf(resp, resp_len,
             "{\"type\":\"settings_sync\",\"brightness\":%d,\"volume\":%d,\"idle_timeout\":%d}",
             s_state.settings.brightness,
             s_state.settings.volume,
             s_state.settings.idle_timeout);
    app_state_broadcast_settings_sync();
    persistence_mark_dirty();
}

void app_state_broadcast_todo_toggled(int index, int id, const char *text, bool done)
{
    if (done && index >= 0 && index < s_state.todo_count) {
        todo_item_t *t = &s_state.todos[index];
        if (t->id == id && !t->points_awarded) {
            t->points_awarded = true;
            int pts = points_store_award_todo();
            app_state_broadcast_points_earned(pts, POINT_REASON_TODO, id);
            app_state_broadcast_points_sync();
        }
    }

    if (!s_broadcast) return;
    char esc[MAX_TODO_LEN * 2];
    json_escape(text, esc, sizeof(esc));
    char *msg = broadcast_lock();
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"todo_toggled\",\"index\":%d,\"id\":%d,\"text\":\"%s\",\"done\":%s}",
             index, id, esc, done ? "true" : "false");
    s_broadcast(msg);
    broadcast_unlock();
}


static void handle_get_screen(const char *json, char *resp, size_t resp_len)
{
    (void)json;
    const char *name = (s_state.current_screen >= 0 && s_state.current_screen < SCREEN_NAMES_COUNT)
        ? screen_names[s_state.current_screen] : "unknown";
    snprintf(resp, resp_len,
             "{\"type\":\"screen_info\",\"screen_id\":%d,\"screen\":\"%s\"}",
             s_state.current_screen, name);
}

static void handle_get_todos(const char *json, char *resp, size_t resp_len)
{
    (void)json;
    char esc[MAX_TODO_LEN * 2];
    int off = snprintf(resp, resp_len, "{\"type\":\"todos_info\",\"tasks\":[");
    for (int i = 0; i < s_state.todo_count && off < (int)resp_len - 200; i++) {
        todo_item_t *t = &s_state.todos[i];
        if (i > 0) off += snprintf(resp + off, resp_len - off, ",");
        json_escape(t->text, esc, sizeof(esc));
        off += snprintf(resp + off, resp_len - off,
                        "{\"id\":%d,\"text\":\"%s\",\"done\":%s,\"priority\":%d,\"order\":%d}",
                        t->id, esc, t->done ? "true" : "false", t->priority, t->order);
    }
    snprintf(resp + off, resp_len - off, "]}");
}

static void handle_get_breathing(const char *json, char *resp, size_t resp_len)
{
    (void)json;
    char esc[MAX_NAME_LEN * 2];
    int off = snprintf(resp, resp_len, "{\"type\":\"breathing_info\",\"exercises\":[");
    for (int i = 0; i < s_state.exercise_count && off < (int)resp_len - 200; i++) {
        exercise_t *e = &s_state.exercises[i];
        if (i > 0) off += snprintf(resp + off, resp_len - off, ",");
        json_escape(e->name, esc, sizeof(esc));
        off += snprintf(resp + off, resp_len - off,
                        "{\"id\":%d,\"name\":\"%s\",\"inhale\":%d,\"hold\":%d,\"exhale\":%d,\"hold2\":%d}",
                        e->id, esc, e->inhale_ms, e->hold_ms, e->exhale_ms, e->hold2_ms);
    }
    snprintf(resp + off, resp_len - off,
             "],\"active\":%s,\"active_id\":%d,\"sessions_today\":%d}",
             s_state.breathing_active ? "true" : "false", s_state.breathing_exercise_id,
             session_store_get_breath_count());
}

static void handle_get_water(const char *json, char *resp, size_t resp_len)
{
    (void)json;
    snprintf(resp, resp_len,
             "{\"type\":\"water_info\",\"glasses\":%d,\"goal\":%d}",
             s_state.water.glasses, s_state.water.goal);
}

static void handle_get_timer(const char *json, char *resp, size_t resp_len)
{
    (void)json;
    bool is_pomodoro = false;
    find_preset_by_id(s_state.timer.preset_id, &is_pomodoro, NULL, 0);
    const char *phase_name = (s_state.timer.phase >= 0 && s_state.timer.phase <= 2)
        ? timer_phase_names[s_state.timer.phase] : "session";
    snprintf(resp, resp_len,
             "{\"type\":\"timer_info\",\"remaining_ms\":%ld,\"running\":%s,\"preset_id\":%d,\"phase\":%d,"
             "\"phase_name\":\"%s\",\"is_pomodoro\":%s,\"total_ms\":%ld,\"phase_complete\":%s}",
             (long)s_state.timer.remaining_seconds * 1000,
             (s_state.timer.is_running && !s_state.timer.phase_complete_awaiting_press) ? "true" : "false",
             s_state.timer.preset_id, s_state.timer.phase, phase_name,
             is_pomodoro ? "true" : "false",
             (long)s_state.timer.total_seconds * 1000,
             s_state.timer.phase_complete_awaiting_press ? "true" : "false");
}

static void handle_get_presets(const char *json, char *resp, size_t resp_len)
{
    (void)json;
    char esc[MAX_NAME_LEN * 2];
    int off = snprintf(resp, resp_len, "{\"type\":\"presets_info\",\"presets\":[");
    for (int i = 0; i < s_state.preset_count && off < (int)resp_len - 200; i++) {
        preset_t *p = &s_state.presets[i];
        if (i > 0) off += snprintf(resp + off, resp_len - off, ",");
        json_escape(p->name, esc, sizeof(esc));
        off += snprintf(resp + off, resp_len - off,
                        "{\"id\":%d,\"name\":\"%s\",\"focus\":%d,\"break_duration\":%d,\"is_pomodoro\":%s}",
                        p->id, esc, p->focus_ms, p->break_ms, p->is_pomodoro ? "true" : "false");
    }
    snprintf(resp + off, resp_len - off, "],\"active_preset_id\":%d}", s_state.active_preset_id);
}

static void handle_get_sleep(const char *json, char *resp, size_t resp_len)
{
    (void)json;
    uint16_t minutes[7] = {0};
    int count = 0;
    sleep_store_get_history(minutes, &count);
    int off = snprintf(resp, resp_len, "{\"type\":\"sleep_info\",\"history\":[");
    for (int i = 0; i < count; i++) {
        if (i > 0) off += snprintf(resp + off, resp_len - off, ",");
        off += snprintf(resp + off, resp_len - off, "%u", (unsigned)minutes[i]);
    }
    snprintf(resp + off, resp_len - off,
             "],\"history_count\":%d,\"breathing_sessions_today\":%d}",
             count, session_store_get_breath_count());
}

static void handle_get_settings(const char *json, char *resp, size_t resp_len)
{
    (void)json;
    snprintf(resp, resp_len,
             "{\"type\":\"settings_info\",\"brightness\":%d,\"volume\":%d,\"idle_timeout\":%d}",
             s_state.settings.brightness,
             s_state.settings.volume,
             s_state.settings.idle_timeout);
}

static void handle_get_points(const char *json, char *resp, size_t resp_len)
{
    (void)json;
    points_store_rollover_if_new_day();
    int off = snprintf(resp, resp_len, "{\"type\":\"points_sync\",");
    off += write_points_fields(resp + off, resp_len - off);
    snprintf(resp + off, resp_len - off, "}");
}

static void handle_goal_set(const char *json, char *resp, size_t resp_len)
{
    int index = find_int(json, "index", -1);
    char label[MAX_GOAL_LEN];
    const char *label_p = find_string(json, "label", label, MAX_GOAL_LEN, NULL);
    int metric = find_int(json, "metric", -1);
    int target = find_int(json, "target", -1);

    if (index < 0 || index >= MAX_DAILY_GOALS) {
        snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"bad goal index\"}");
        return;
    }
    if (!points_store_set_goal(index, label_p, metric, target)) {
        snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"goal not set\"}");
        return;
    }
    app_state_broadcast_points_sync();
    persistence_mark_dirty();
    snprintf(resp, resp_len, "{\"type\":\"goal_update\",\"index\":%d}", index);
}

static void handle_goal_toggle(const char *json, char *resp, size_t resp_len)
{
    int index = find_int(json, "index", -1);
    bool done = find_bool(json, "done", false);

    if (index < 0 || index >= MAX_DAILY_GOALS) {
        snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"bad goal index\"}");
        return;
    }
    int pts = points_store_toggle_goal(index, done);
    if (pts > 0) {
        app_state_broadcast_points_earned(pts, POINT_REASON_DAILY_GOAL, index);
    }
    app_state_broadcast_points_sync();
    persistence_mark_dirty();
    snprintf(resp, resp_len, "{\"type\":\"goal_update\",\"index\":%d,\"done\":%s}",
             index, done ? "true" : "false");
}

static void handle_points_setting(const char *json, char *resp, size_t resp_len)
{
    int hour = find_int(json, "hour", -1);
    int min = find_int(json, "min", -1);
    int cur_h, cur_m;
    points_store_get_bedtime(&cur_h, &cur_m);
    if (hour < 0) hour = cur_h;
    if (min < 0) min = cur_m;
    points_store_set_bedtime(hour, min);
    app_state_broadcast_points_sync();
    persistence_mark_dirty();
    snprintf(resp, resp_len, "{\"type\":\"points_setting\",\"hour\":%d,\"min\":%d}", hour, min);
}

static void handle_points_admin(const char *json, char *resp, size_t resp_len)
{
    char action[16];
    find_string(json, "action", action, sizeof(action), "");
    int amount = find_int(json, "amount", 0);
    int result = 0;

    if (strcmp(action, "add") == 0) {
        result = points_store_admin_add(amount);
    } else if (strcmp(action, "sub") == 0) {
        result = points_store_admin_sub(amount);
    } else if (strcmp(action, "reset") == 0) {
        points_store_admin_reset();
    } else {
        snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"unknown admin action\"}");
        return;
    }

    if (result != 0) {
        app_state_broadcast_points_earned(result, POINT_REASON_ADMIN, amount);
    }
    app_state_broadcast_points_sync();
    persistence_mark_dirty();
    snprintf(resp, resp_len, "{\"type\":\"points_admin\",\"result\":%d}", result);
}

void app_state_handle_message(const char *type, const char *json_msg, char *resp, size_t resp_len)
{
    ESP_LOGI(TAG, "Handling: %s", type);

    if (strcmp(type, "ping") == 0) {
        snprintf(resp, resp_len, "{\"type\":\"pong\",\"uptime_ms\":%lld}",
                 (long long)(esp_timer_get_time() / 1000));
    } else if (strcmp(type, "get_screen") == 0) {
        handle_get_screen(json_msg, resp, resp_len);
    } else if (strcmp(type, "get_todos") == 0) {
        handle_get_todos(json_msg, resp, resp_len);
    } else if (strcmp(type, "get_breathing") == 0) {
        handle_get_breathing(json_msg, resp, resp_len);
    } else if (strcmp(type, "get_water") == 0) {
        handle_get_water(json_msg, resp, resp_len);
    } else if (strcmp(type, "get_timer") == 0) {
        handle_get_timer(json_msg, resp, resp_len);
    } else if (strcmp(type, "get_presets") == 0) {
        handle_get_presets(json_msg, resp, resp_len);
    } else if (strcmp(type, "get_sleep") == 0) {
        handle_get_sleep(json_msg, resp, resp_len);
    } else if (strcmp(type, "get_settings") == 0) {
        handle_get_settings(json_msg, resp, resp_len);
    } else if (strcmp(type, "echo") == 0) {
        char data_buf[128];
        char esc[256];
        if (find_string(json_msg, "data", data_buf, sizeof(data_buf), NULL)) {
            json_escape(data_buf, esc, sizeof(esc));
            snprintf(resp, resp_len, "{\"type\":\"echo\",\"data\":\"%s\"}", esc);
        } else {
            snprintf(resp, resp_len, "{\"type\":\"echo\",\"data\":null}");
        }
    } else if (strcmp(type, "full_sync") == 0) {
        app_state_send_full_sync(resp, resp_len);
    } else if (strcmp(type, "todo_add") == 0) {
        handle_todo_add(json_msg, resp, resp_len);
    } else if (strcmp(type, "todo_update") == 0) {
        handle_todo_update(json_msg, resp, resp_len);
    } else if (strcmp(type, "todo_delete") == 0) {
        handle_todo_delete(json_msg, resp, resp_len);
    } else if (strcmp(type, "preset_add") == 0) {
        handle_preset_add(json_msg, resp, resp_len);
    } else if (strcmp(type, "preset_update") == 0) {
        handle_preset_update(json_msg, resp, resp_len);
    } else if (strcmp(type, "preset_delete") == 0) {
        handle_preset_delete(json_msg, resp, resp_len);
    } else if (strcmp(type, "preset_select") == 0) {
        handle_preset_select(json_msg, resp, resp_len);
    } else if (strcmp(type, "timer_command") == 0) {
        handle_timer_command(json_msg, resp, resp_len);
    } else if (strcmp(type, "water_log") == 0) {
        handle_water_log(json_msg, resp, resp_len);
    } else if (strcmp(type, "water_goal") == 0) {
        handle_water_goal(json_msg, resp, resp_len);
    } else if (strcmp(type, "breathing_start") == 0) {
        handle_breathing_start(json_msg, resp, resp_len);
    } else if (strcmp(type, "breathing_stop") == 0) {
        handle_breathing_stop(json_msg, resp, resp_len);
    } else if (strcmp(type, "breathing_select") == 0) {
        handle_breathing_select(json_msg, resp, resp_len);
    } else if (strcmp(type, "breathing_update") == 0) {
        handle_breathing_update(json_msg, resp, resp_len);
    } else if (strcmp(type, "settings_update") == 0) {
        handle_settings_update(json_msg, resp, resp_len);
    } else if (strcmp(type, "get_points") == 0) {
        handle_get_points(json_msg, resp, resp_len);
    } else if (strcmp(type, "goal_set") == 0) {
        handle_goal_set(json_msg, resp, resp_len);
    } else if (strcmp(type, "goal_toggle") == 0) {
        handle_goal_toggle(json_msg, resp, resp_len);
    } else if (strcmp(type, "points_setting") == 0) {
        handle_points_setting(json_msg, resp, resp_len);
    } else if (strcmp(type, "points_admin") == 0) {
        handle_points_admin(json_msg, resp, resp_len);
    } else {
        snprintf(resp, resp_len,
                 "{\"type\":\"error\",\"message\":\"unknown type\",\"received_type\":\"%s\"}", type);
    }
}
