#include "interface.h"
#include "srv_wifi.h"
#include "drv_gpio.h"
#include "protocol.h"
#include "mqtt.h" // updated from "mqtt_client.h"

#define WIFI_SSID "Souza"
#define WIFI_PASS "abi53911214"

void app_interface_start_wifi(void)
{
    srv_wifi_init();
    srv_wifi_connect(WIFI_SSID, WIFI_PASS);
}
