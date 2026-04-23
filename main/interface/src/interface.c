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

    // removed: protocol_init(&proto);
    // protocol should be initialized by caller (main) and passed where needed
}

void app_interface_init_gpio(protocol_t *proto)
{
    // initialize GPIO driver with given protocol (device_id, etc.)
    drv_gpio_init(proto);

    // removed: app_interface_start_mqtt(&proto);
    // MQTT now started explicitly from main after protocol_init
}

// removed wrapper app_interface_start_mqtt to avoid duplicate/undefined symbol