#include "app_state.h"

#include <string.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "app_state";

static app_state_t s_state;
static ws_broadcast_fn s_broadcast = NULL;

static const char *screen_names[] = {
    "home", "menu", "timer", "timer_presets", "todos",
    "water", "breathing", "sedentary", "settings",
    "backgrounds", "notifications"
};

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
    char msg[MAX_BROADCAST];
    snprintf(msg, MAX_BROADCAST, "{\"type\":\"%s\",%s}", type, json_body);
    s_broadcast(msg);
}

static void broadcast_todo_sync(void)
{
    if (!s_broadcast) return;
    char msg[MAX_BROADCAST];
    int off = snprintf(msg, MAX_BROADCAST, "{\"type\":\"todo_sync\",\"tasks\":[");
    for (int i = 0; i < s_state.todo_count && off < MAX_BROADCAST - 200; i++) {
        todo_item_t *t = &s_state.todos[i];
        if (i > 0) off += snprintf(msg + off, MAX_BROADCAST - off, ",");
        off += snprintf(msg + off, MAX_BROADCAST - off,
                        "{\"id\":%d,\"text\":\"%s\",\"done\":%s,\"priority\":%d,\"order\":%d}",
                        t->id, t->text, t->done ? "true" : "false", t->priority, t->order);
    }
    snprintf(msg + off, MAX_BROADCAST - off, "]}");
    s_broadcast(msg);
}

static void broadcast_preset_sync(void)
{
    if (!s_broadcast) return;
    char msg[MAX_BROADCAST];
    int off = snprintf(msg, MAX_BROADCAST, "{\"type\":\"presets_sync\",\"presets\":[");
    for (int i = 0; i < s_state.preset_count && off < MAX_BROADCAST - 200; i++) {
        preset_t *p = &s_state.presets[i];
        if (i > 0) off += snprintf(msg + off, MAX_BROADCAST - off, ",");
        off += snprintf(msg + off, MAX_BROADCAST - off,
                        "{\"id\":%d,\"name\":\"%s\",\"focus\":%d,\"break_duration\":%d,\"is_pomodoro\":%s}",
                        p->id, p->name, p->focus_ms, p->break_ms, p->is_pomodoro ? "true" : "false");
    }
    snprintf(msg + off, MAX_BROADCAST - off, "],\"active_preset_id\":%d}", s_state.active_preset_id);
    s_broadcast(msg);
}

static void broadcast_water_sync(void)
{
    if (!s_broadcast) return;
    char msg[MAX_BROADCAST];
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"water_update\",\"glasses\":%d,\"goal\":%d}",
             s_state.water.glasses, s_state.water.goal);
    s_broadcast(msg);
}

static void broadcast_timer_sync(void)
{
    if (!s_broadcast) return;
    char msg[MAX_BROADCAST];
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"timer_update\",\"remaining_ms\":%d,\"running\":%s,\"preset_id\":%d,\"phase\":%d}",
             s_state.timer.remaining_ms,
             s_state.timer.running ? "true" : "false",
             s_state.timer.preset_id,
             s_state.timer.phase);
    s_broadcast(msg);
}

static void broadcast_breathing_sync(void)
{
    if (!s_broadcast) return;
    char msg[MAX_BROADCAST];
    int off = snprintf(msg, MAX_BROADCAST, "{\"type\":\"breathing_sync\",\"exercises\":[");
    for (int i = 0; i < s_state.exercise_count && off < MAX_BROADCAST - 200; i++) {
        exercise_t *e = &s_state.exercises[i];
        if (i > 0) off += snprintf(msg + off, MAX_BROADCAST - off, ",");
        off += snprintf(msg + off, MAX_BROADCAST - off,
                        "{\"id\":%d,\"name\":\"%s\",\"inhale\":%d,\"hold\":%d,\"exhale\":%d,\"hold2\":%d}",
                        e->id, e->name, e->inhale_ms, e->hold_ms, e->exhale_ms, e->hold2_ms);
    }
    snprintf(msg + off, MAX_BROADCAST - off, "],\"active\":%s,\"active_id\":%d}",
             s_state.breathing_active ? "true" : "false", s_state.breathing_exercise_id);
    s_broadcast(msg);
}

static void broadcast_settings_sync(void)
{
    if (!s_broadcast) return;
    char msg[MAX_BROADCAST];
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"settings_sync\",\"brightness\":%d,\"volume\":%d,\"idle_timeout\":%d}",
             s_state.settings.brightness,
             s_state.settings.volume,
             s_state.settings.idle_timeout);
    s_broadcast(msg);
}

void app_state_broadcast_screen_change(int screen_id)
{
    if (!s_broadcast) return;
    s_state.current_screen = screen_id;
    const char *name = (screen_id >= 0 && screen_id < 11) ? screen_names[screen_id] : "unknown";
    char msg[MAX_BROADCAST];
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"screen_change\",\"screen_id\":%d,\"screen\":\"%s\"}",
             screen_id, name);
    s_broadcast(msg);
}

void app_state_broadcast_encoder_event(const char *direction, const char *press_state)
{
    if (!s_broadcast) return;
    char msg[MAX_BROADCAST];
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"encoder_event\",\"direction\":\"%s\",\"press\":\"%s\",\"screen\":%d}",
             direction, press_state, s_state.current_screen);
    s_broadcast(msg);
}

void app_state_send_full_sync(char *resp, size_t resp_len)
{
    int off = snprintf(resp, resp_len, "{\"type\":\"full_sync\",\"todos\":[");

    for (int i = 0; i < s_state.todo_count && off < (int)resp_len - 200; i++) {
        todo_item_t *t = &s_state.todos[i];
        if (i > 0) off += snprintf(resp + off, resp_len - off, ",");
        off += snprintf(resp + off, resp_len - off,
                        "{\"id\":%d,\"text\":\"%s\",\"done\":%s,\"priority\":%d,\"order\":%d}",
                        t->id, t->text, t->done ? "true" : "false", t->priority, t->order);
    }

    off += snprintf(resp + off, resp_len - off, "],\"presets\":[");
    for (int i = 0; i < s_state.preset_count && off < (int)resp_len - 200; i++) {
        preset_t *p = &s_state.presets[i];
        if (i > 0) off += snprintf(resp + off, resp_len - off, ",");
        off += snprintf(resp + off, resp_len - off,
                        "{\"id\":%d,\"name\":\"%s\",\"focus\":%d,\"break_duration\":%d,\"is_pomodoro\":%s}",
                        p->id, p->name, p->focus_ms, p->break_ms, p->is_pomodoro ? "true" : "false");
    }

    off += snprintf(resp + off, resp_len - off,
                    "],\"active_preset_id\":%d,"
                    "\"water\":{\"glasses\":%d,\"goal\":%d},"
                    "\"timer\":{\"remaining_ms\":%d,\"running\":%s,\"preset_id\":%d,\"phase\":%d},"
                    "\"settings\":{\"brightness\":%d,\"volume\":%d,\"idle_timeout\":%d},"
                    "\"current_screen\":%d,\"screen\":\"%s\"}",
                    s_state.active_preset_id,
                    s_state.water.glasses, s_state.water.goal,
                    s_state.timer.remaining_ms,
                    s_state.timer.running ? "true" : "false",
                    s_state.timer.preset_id, s_state.timer.phase,
                    s_state.settings.brightness,
                    s_state.settings.volume,
                    s_state.settings.idle_timeout,
                    s_state.current_screen,
                    (s_state.current_screen >= 0 && s_state.current_screen < 11)
                        ? screen_names[s_state.current_screen] : "unknown");
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

    snprintf(resp, resp_len, "{\"type\":\"todo_added\",\"id\":%d}", t->id);
    broadcast_todo_sync();
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
            const char *done_pos = find_field(json, "done");
            if (done_pos) {
                t->done = (strncmp(done_pos, "true", 4) == 0);
            }
            const char *pri_pos = find_field(json, "priority");
            if (pri_pos) t->priority = atoi(pri_pos);

            snprintf(resp, resp_len, "{\"type\":\"todo_updated\",\"id\":%d}", id);
            broadcast_todo_sync();
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
            snprintf(resp, resp_len, "{\"type\":\"todo_deleted\",\"id\":%d}", id);
            broadcast_todo_sync();
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
            return;
        }
    }
    snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"preset not found\"}");
}

static void handle_timer_command(const char *json, char *resp, size_t resp_len)
{
    char action[16];
    find_string(json, "action", action, sizeof(action), "");

    if (strcmp(action, "start") == 0) {
        if (!s_state.timer.running) {
            if (s_state.timer.remaining_ms <= 0) {
                for (int i = 0; i < s_state.preset_count; i++) {
                    if (s_state.presets[i].id == s_state.active_preset_id) {
                        s_state.timer.remaining_ms = s_state.presets[i].focus_ms;
                        s_state.timer.phase = 0;
                        s_state.timer.preset_id = s_state.presets[i].id;
                        break;
                    }
                }
            }
            s_state.timer.running = true;
            s_state.timer.last_tick = esp_timer_get_time();
        }
        snprintf(resp, resp_len, "{\"type\":\"timer_update\",\"remaining_ms\":%d,\"running\":true,\"preset_id\":%d,\"phase\":%d}",
                 s_state.timer.remaining_ms, s_state.timer.preset_id, s_state.timer.phase);
    } else if (strcmp(action, "pause") == 0) {
        s_state.timer.running = false;
        snprintf(resp, resp_len, "{\"type\":\"timer_update\",\"remaining_ms\":%d,\"running\":false,\"preset_id\":%d,\"phase\":%d}",
                 s_state.timer.remaining_ms, s_state.timer.preset_id, s_state.timer.phase);
    } else if (strcmp(action, "reset") == 0) {
        s_state.timer.running = false;
        for (int i = 0; i < s_state.preset_count; i++) {
            if (s_state.presets[i].id == s_state.active_preset_id) {
                s_state.timer.remaining_ms = s_state.presets[i].focus_ms;
                s_state.timer.preset_id = s_state.presets[i].id;
                break;
            }
        }
        s_state.timer.phase = 0;
        snprintf(resp, resp_len, "{\"type\":\"timer_update\",\"remaining_ms\":%d,\"running\":false,\"preset_id\":%d,\"phase\":0}",
                 s_state.timer.remaining_ms, s_state.timer.preset_id);
    } else if (strcmp(action, "skip") == 0) {
        s_state.timer.running = false;
        s_state.timer.phase = (s_state.timer.phase + 1) % 2;
        for (int i = 0; i < s_state.preset_count; i++) {
            if (s_state.presets[i].id == s_state.active_preset_id) {
                s_state.timer.remaining_ms = (s_state.timer.phase == 0)
                    ? s_state.presets[i].focus_ms
                    : s_state.presets[i].break_ms;
                break;
            }
        }
        snprintf(resp, resp_len, "{\"type\":\"timer_update\",\"remaining_ms\":%d,\"running\":false,\"preset_id\":%d,\"phase\":%d}",
                 s_state.timer.remaining_ms, s_state.timer.preset_id, s_state.timer.phase);
    } else {
        snprintf(resp, resp_len, "{\"type\":\"error\",\"message\":\"unknown timer action\"}");
        return;
    }
    broadcast_timer_sync();
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
}

static void handle_water_goal(const char *json, char *resp, size_t resp_len)
{
    s_state.water.goal = find_int(json, "goal", 8);
    snprintf(resp, resp_len, "{\"type\":\"water_update\",\"glasses\":%d,\"goal\":%d}",
             s_state.water.glasses, s_state.water.goal);
    broadcast_water_sync();
}

static void handle_breathing_start(const char *json, char *resp, size_t resp_len)
{
    int id = find_int(json, "exercise_id", 1);
    s_state.breathing_active = true;
    s_state.breathing_exercise_id = id;
    snprintf(resp, resp_len, "{\"type\":\"breathing_started\",\"exercise_id\":%d}", id);
    broadcast_breathing_sync();
}

static void handle_breathing_stop(const char *json, char *resp, size_t resp_len)
{
    s_state.breathing_active = false;
    snprintf(resp, resp_len, "{\"type\":\"breathing_stopped\"}");
    broadcast_breathing_sync();
}

static void handle_settings_update(const char *json, char *resp, size_t resp_len)
{
    const char *b_pos = find_field(json, "brightness");
    if (b_pos) s_state.settings.brightness = atoi(b_pos);
    const char *v_pos = find_field(json, "volume");
    if (v_pos) s_state.settings.volume = atoi(v_pos);
    const char *i_pos = find_field(json, "idle_timeout");
    if (i_pos)     s_state.settings.idle_timeout = atoi(i_pos);

    snprintf(resp, resp_len,
             "{\"type\":\"settings_sync\",\"brightness\":%d,\"volume\":%d,\"idle_timeout\":%d}",
             s_state.settings.brightness,
             s_state.settings.volume,
             s_state.settings.idle_timeout);
    broadcast_settings_sync();
}

void app_state_broadcast_todo_toggled(int index, const char *text, bool done)
{
    if (!s_broadcast) return;
    char msg[MAX_BROADCAST];
    snprintf(msg, MAX_BROADCAST,
             "{\"type\":\"todo_toggled\",\"index\":%d,\"text\":\"%s\",\"done\":%s}",
             index, text, done ? "true" : "false");
    s_broadcast(msg);
}


void app_state_handle_message(const char *type, const char *json_msg, char *resp, size_t resp_len)
{
    ESP_LOGI(TAG, "Handling: %s", type);

    if (strcmp(type, "ping") == 0) {
        snprintf(resp, resp_len, "{\"type\":\"pong\",\"uptime_ms\":%lld}",
                 (long long)(esp_timer_get_time() / 1000));
    } else if (strcmp(type, "echo") == 0) {
        char data_buf[128];
        if (find_string(json_msg, "data", data_buf, sizeof(data_buf), NULL)) {
            snprintf(resp, resp_len, "{\"type\":\"echo\",\"data\":\"%s\"}", data_buf);
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
    } else if (strcmp(type, "settings_update") == 0) {
        handle_settings_update(json_msg, resp, resp_len);
    } else {
        snprintf(resp, resp_len,
                 "{\"type\":\"error\",\"message\":\"unknown type\",\"received_type\":\"%s\"}", type);
    }
}
