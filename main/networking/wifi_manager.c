#include "wifi_manager.h"

#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_eap_client.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_mgr";

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define MAX_RETRY_PER_NETWORK 5
#define CONNECT_TIMEOUT_MS    15000

extern const uint8_t school_ca_pem_start[] asm("_binary_school_ca_pem_start");
extern const uint8_t school_ca_pem_end[]   asm("_binary_school_ca_pem_end");

static EventGroupHandle_t s_wifi_event_group;
static esp_netif_t *s_sta_netif = NULL;
static esp_ip4_addr_t s_ip_addr;
static bool s_connected = false;
static bool s_wifi_started = false;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_connected = false;
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        s_ip_addr = event->ip_info.ip;
        s_connected = true;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "Connected! IP: " IPSTR, IP2STR(&s_ip_addr));
    }
}

static void start_sntp(void)
{
    setenv("TZ", CONFIG_STUDYBUD_TIMEZONE, 1);
    tzset();

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "time.google.com");
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP started (TZ='%s')", CONFIG_STUDYBUD_TIMEZONE);
}

static void stop_wifi(void)
{
    if (s_wifi_started) {
        esp_wifi_disconnect();
        esp_wifi_stop();
        s_wifi_started = false;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void configure_psk(const wifi_credential_t *cred)
{
    esp_wifi_sta_enterprise_disable();

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, cred->ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (cred->password) {
        strncpy((char *)wifi_config.sta.password, cred->password, sizeof(wifi_config.sta.password) - 1);
    }
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
}

static void configure_enterprise(const wifi_credential_t *cred)
{
    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, cred->ssid, sizeof(wifi_config.sta.ssid) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    int ca_len = school_ca_pem_end - school_ca_pem_start;
    ESP_ERROR_CHECK(esp_eap_client_set_ca_cert(school_ca_pem_start, ca_len));

    if (cred->identity) {
        ESP_ERROR_CHECK(esp_eap_client_set_identity((uint8_t *)cred->identity, strlen(cred->identity)));
    }
    if (cred->username) {
        ESP_ERROR_CHECK(esp_eap_client_set_username((uint8_t *)cred->username, strlen(cred->username)));
    }
    if (cred->ent_password) {
        ESP_ERROR_CHECK(esp_eap_client_set_password((uint8_t *)cred->ent_password, strlen(cred->ent_password)));
    }
    ESP_ERROR_CHECK(esp_eap_client_set_eap_methods(ESP_EAP_TYPE_PEAP));

    ESP_ERROR_CHECK(esp_wifi_sta_enterprise_enable());
}

esp_err_t wifi_manager_init(const wifi_credential_t *credentials, int count)
{
    if (count <= 0 || credentials == NULL) {
        ESP_LOGW(TAG, "No WiFi credentials provided, skipping WiFi");
        return ESP_FAIL;
    }

    s_wifi_event_group = xEventGroupCreate();

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t inst_any_id;
    esp_event_handler_instance_t inst_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler, NULL, &inst_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler, NULL, &inst_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    for (int i = 0; i < count; i++) {
        const wifi_credential_t *cred = &credentials[i];
        ESP_LOGI(TAG, "Trying network %d/%d: '%s' (%s)", i + 1, count, cred->ssid,
                 cred->auth_mode == WIFI_CRED_ENTERPRISE ? "Enterprise" : "PSK");

        stop_wifi();

        if (cred->auth_mode == WIFI_CRED_ENTERPRISE) {
            configure_enterprise(cred);
        } else {
            configure_psk(cred);
        }

        ESP_ERROR_CHECK(esp_wifi_start());
        s_wifi_started = true;

        for (int retry = 0; retry < MAX_RETRY_PER_NETWORK; retry++) {
            xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

            esp_err_t connect_ret = esp_wifi_connect();
            if (connect_ret != ESP_OK) {
                ESP_LOGW(TAG, "Connect call failed: %d, retrying...", connect_ret);
                vTaskDelay(pdMS_TO_TICKS(1000));
                continue;
            }

            EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                                   WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                                   pdFALSE, pdFALSE,
                                                   pdMS_TO_TICKS(CONNECT_TIMEOUT_MS));

            if (bits & WIFI_CONNECTED_BIT) {
                ESP_LOGI(TAG, "WiFi connected to '%s'", cred->ssid);
                start_sntp();
                return ESP_OK;
            }

            ESP_LOGW(TAG, "Failed to connect to '%s' (attempt %d/%d)",
                     cred->ssid, retry + 1, MAX_RETRY_PER_NETWORK);
        }

        ESP_LOGW(TAG, "All retries exhausted for '%s'", cred->ssid);
    }

    ESP_LOGE(TAG, "Failed to connect to any WiFi network");
    return ESP_FAIL;
}

esp_ip4_addr_t wifi_manager_get_ip(void)
{
    return s_ip_addr;
}

bool wifi_manager_is_connected(void)
{
    return s_connected;
}
