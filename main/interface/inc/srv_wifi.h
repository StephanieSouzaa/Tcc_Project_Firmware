#ifndef SRV_WIFI_H
#define SRV_WIFI_H

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SRV_WIFI_DISCONNECTED,
    SRV_WIFI_CONNECTING,
    SRV_WIFI_CONNECTED
} srv_wifi_status_t;

esp_err_t srv_wifi_init(void);
esp_err_t srv_wifi_connect(const char *ssid, const char *password);

bool srv_wifi_load_credentials(char *ssid, size_t ssid_size, char *password, size_t password_size);
bool srv_wifi_has_saved_credentials(void);
esp_err_t srv_wifi_start_ap(void);
esp_err_t srv_wifi_start_config_server(void);
srv_wifi_status_t srv_wifi_get_status(void);
void srv_initialize_sntp(void);
void srv_wifi_scan_networks(void);
bool srv_wifi_is_config_mode(void);

#endif