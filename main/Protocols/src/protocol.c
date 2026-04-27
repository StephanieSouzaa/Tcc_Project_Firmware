#include "protocol.h"
#include "http.h"
#include "mqtt.h"
#include "drv_gpio.h"
#include "esp_log.h"
#include <string.h>

#define DEVICE_ID "device1"

static const char *TAG = "PROTOCOL";


void protocol_init(protocol_t *proto)
{
   strncpy(proto->device_id, DEVICE_ID, sizeof(proto->device_id) - 1);
   proto->device_id[sizeof(proto->device_id) - 1] = '\0';
}

void protocol_start(protocol_t *proto)
{
    app_interface_start_mqtt(proto);
}

void protocol_send_gpio(protocol_t *proto, int gpio, int state)
{
    http_send_gpio(proto, gpio, state);
}

void protocol_handle_gpio_command(protocol_t *proto, int gpio, int state)
{
    ESP_LOGI(TAG, "Executando comando: GPIO %d -> %d", gpio, state);

    drv_gpio_set(gpio, state);
    protocol_send_gpio(proto, gpio, state);
}