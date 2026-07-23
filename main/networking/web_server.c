#include "web_server.h"
#include "app_state.h"

#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_timer.h"

static const char *TAG = "web_srv";

#define MAX_WS_CLIENTS 4
#define MAX_MSG_LEN    1024

static httpd_handle_t s_server = NULL;
static int s_ws_fds[MAX_WS_CLIENTS];
static int s_ws_fd_count = 0;

static void register_ws_client(int fd)
{
    if (s_ws_fd_count < MAX_WS_CLIENTS) {
        s_ws_fds[s_ws_fd_count++] = fd;
        ESP_LOGI(TAG, "WS client connected (fd=%d), total=%d", fd, s_ws_fd_count);
    }
}

static void ws_broadcast(const char *msg)
{
    httpd_ws_frame_t ws_pkt = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)msg,
        .len = strlen(msg),
    };

    for (int i = 0; i < s_ws_fd_count; i++) {
        esp_err_t ret = httpd_ws_send_frame_async(s_server, s_ws_fds[i], &ws_pkt);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "WS send failed fd=%d: %s", s_ws_fds[i], esp_err_to_name(ret));
        }
    }
}

static void send_ws_frame(httpd_req_t *req, const char *msg)
{
    httpd_ws_frame_t resp = {
        .final = true,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t *)msg,
        .len = strlen(msg),
    };
    httpd_ws_send_frame(req, &resp);
}

static const char *extract_type(const char *json, char *buf, size_t buf_len)
{
    const char *key = "\"type\"";
    const char *found = strstr(json, key);
    if (!found) return NULL;

    found += strlen(key);
    while (*found == ' ' || *found == ':') found++;

    if (*found != '"') return NULL;
    found++;

    const char *end = strchr(found, '"');
    if (!end) return NULL;

    size_t len = end - found;
    if (len >= buf_len) len = buf_len - 1;
    memcpy(buf, found, len);
    buf[len] = '\0';
    return buf;
}

static void heartbeat_task(void *arg)
{
    char msg[MAX_MSG_LEN];
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));

        if (s_ws_fd_count == 0) continue;

        snprintf(msg, MAX_MSG_LEN,
                 "{\"type\":\"heartbeat\",\"uptime_ms\":%lld,\"free_heap\":%lu,\"clients\":%d}",
                 (long long)(esp_timer_get_time() / 1000),
                 (unsigned long)esp_get_free_heap_size(),
                 s_ws_fd_count);
        ws_broadcast(msg);
    }
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        int fd = httpd_req_to_sockfd(req);
        register_ws_client(fd);

        char sync[MAX_MSG_LEN];
        app_state_send_full_sync(sync, sizeof(sync));
        send_ws_frame(req, sync);

        return ESP_OK;
    }

    httpd_ws_frame_t ws_pkt = {
        .type = HTTPD_WS_TYPE_TEXT,
    };

    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) return ret;
    if (ws_pkt.len == 0) return ESP_OK;

    uint8_t *buf = calloc(1, ws_pkt.len + 1);
    ws_pkt.payload = buf;

    ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
    if (ret != ESP_OK) {
        free(buf);
        return ret;
    }

    ESP_LOGI(TAG, "WS recv: %s", buf);

    char type_buf[32];
    const char *type = extract_type((const char *)buf, type_buf, sizeof(type_buf));

    char resp[MAX_MSG_LEN];

    if (!type) {
        free(buf);
        snprintf(resp, MAX_MSG_LEN, "{\"type\":\"error\",\"message\":\"missing type\"}");
        send_ws_frame(req, resp);
        return ESP_OK;
    }

    app_state_handle_message(type, (const char *)buf, resp, sizeof(resp));
    free(buf);

    send_ws_frame(req, resp);
    return ESP_OK;
}

static esp_err_t status_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");

    char json[MAX_MSG_LEN];
    snprintf(json, MAX_MSG_LEN,
             "{\"status\":\"ok\",\"uptime_ms\":%lld,\"free_heap\":%lu,\"ws_clients\":%d}",
             (long long)(esp_timer_get_time() / 1000),
             (unsigned long)esp_get_free_heap_size(),
             s_ws_fd_count);
    httpd_resp_sendstr(req, json);

    return ESP_OK;
}

esp_err_t web_server_init(void)
{
    app_state_init(ws_broadcast);

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 8;
    config.stack_size = 8192;
    config.lru_purge_enable = true;

    esp_err_t ret = httpd_start(&s_server, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    httpd_uri_t ws_uri = {
        .uri = "/ws",
        .method = HTTP_GET,
        .handler = ws_handler,
        .is_websocket = true,
        .handle_ws_control_frames = true,
    };
    httpd_register_uri_handler(s_server, &ws_uri);

    httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = status_handler,
    };
    httpd_register_uri_handler(s_server, &status_uri);

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);

    xTaskCreate(heartbeat_task, "heartbeat", 4096, NULL, 5, NULL);

    return ESP_OK;
}
