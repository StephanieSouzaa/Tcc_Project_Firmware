#include "interface.h"
#include "srv_wifi.h"
#include "drv_gpio.h"
#include "protocol.h"

#define WIFI_SSID "Souza"
#define WIFI_PASS "abi53911214"

static protocol_t proto;

void app_interface_start_wifi(void)
{
    srv_wifi_init();
    srv_wifi_connect(WIFI_SSID, WIFI_PASS);

    protocol_init(&proto);
}

void app_interface_init_gpio(void)
{
    drv_gpio_init(&proto);
}