#include "protocol.h"
#include "http.h"
#include "mqtt.h"
#include "drv_gpio.h"
#include "sdcard.h"
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

void protocol_send_gpio(protocol_t *proto, int gpio, int state, const char *message_id)
{
    http_send_gpio(proto, gpio, state, message_id);
}

void protocol_handle_gpio_command(protocol_t *proto, int gpio, int state, const char *message_id)
{
    ESP_LOGI(TAG, "Executando comando: GPIO %d -> %d (msg_id=%s)", gpio, state, message_id?message_id:"-");
    sdcard_log("Executando comando: GPIO %d -> %d (msg_id=%s)", gpio, state, message_id?message_id:"-");

    drv_gpio_set(gpio, state);
    protocol_send_gpio(proto, gpio, state, message_id);
}