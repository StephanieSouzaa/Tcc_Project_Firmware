#ifndef SRV_WIFI_H
#define SRV_WIFI_H

#include "esp_err.h"

typedef enum {
    SRV_WIFI_DISCONNECTED,
    SRV_WIFI_CONNECTING,
    SRV_WIFI_CONNECTED
} srv_wifi_status_t;

esp_err_t srv_wifi_init(void);
esp_err_t srv_wifi_connect(const char *ssid, const char *password);
srv_wifi_status_t srv_wifi_get_status(void);

#endif