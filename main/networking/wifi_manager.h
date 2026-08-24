#pragma once

#include "esp_err.h"
#include "esp_netif_ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    WIFI_CRED_PSK,
    WIFI_CRED_ENTERPRISE,
} wifi_cred_mode_t;

typedef struct {
    const char *ssid;
    wifi_cred_mode_t auth_mode;
    /* PSK fields */
    const char *password;
    /* Enterprise fields (PEAP) */
    const char *identity;
    const char *username;
    const char *ent_password;
} wifi_credential_t;

esp_err_t wifi_manager_init(const wifi_credential_t *credentials, int count);
esp_ip4_addr_t wifi_manager_get_ip(void);
bool wifi_manager_is_connected(void);

#ifdef __cplusplus
}
#endif
