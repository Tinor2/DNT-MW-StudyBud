#pragma once

#include "esp_err.h"
#include "esp_netif_ip_addr.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t wifi_manager_init(const char *ssid, const char *password);
esp_ip4_addr_t wifi_manager_get_ip(void);
bool wifi_manager_is_connected(void);

#ifdef __cplusplus
}
#endif
