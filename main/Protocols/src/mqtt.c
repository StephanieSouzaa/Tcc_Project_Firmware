#include "mqtt.h"
#include "esp_log.h"
#include "esp_err.h"
#include "mqtt_client.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_crt_bundle.h"

static const char *TAG = "MQTT_CLIENT";


#define MQTT_HOST     "92ca7f6444f944cbae30908602073cd5.s1.eu.hivemq.cloud"
#define MQTT_PORT     8883
#define MQTT_USER     "Steph"
#define MQTT_PASS     "Steph123"
#define TOPIC_PREFIX  "tcc_project/"
#define MQTT_QOS      0

static esp_mqtt_client_handle_t mqtt_client = NULL;

#define MAX_GPIO_TRACK 32
static int last_states[MAX_GPIO_TRACK];

static void handle_incoming_message(const char *topic, int topic_len, const char *data, int data_len)
{
    char topic_buf[128];
    char payload[64];

    int tlen = (topic_len < (int)sizeof(topic_buf)-1) ? topic_len : (int)sizeof(topic_buf)-1;
    memcpy(topic_buf, topic, tlen);
    topic_buf[tlen] = '\0';

    int plen = (data_len < (int)sizeof(payload)-1) ? data_len : (int)sizeof(payload)-1;
    memcpy(payload, data, plen);
    payload[plen] = '\0';

    char *p = strstr(topic_buf, "/gpio/");
    if (!p) {
        ESP_LOGW(TAG, "Topic sem /gpio/: %s", topic_buf);
        return;
    }

    int gpio = atoi(p + 6);
    if (gpio < 0 || gpio >= MAX_GPIO_TRACK) {
        ESP_LOGW(TAG, "GPIO inválido: %d", gpio);
        return;
    }

    int new_state = atoi(payload);
    int old_state = last_states[gpio];

    last_states[gpio] = new_state;

    if (old_state == 0 && new_state == 1)
    {
        ESP_LOGI(TAG, "TRIGGER GPIO %d (0 -> 1)", gpio);
    } 
    else 
    {
        ESP_LOGI(TAG, "MQTT msg: %s = %s (prev=%d)", topic_buf, payload, old_state);
    }
}


static void mqtt_event_handler_cb(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    protocol_t *proto = (protocol_t *) handler_args;

    switch (event_id) {

        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "MQTT conectado");

            char sub_topic[128];
            const char *device_id = (proto && proto->device_id[0]) ? proto->device_id : "";

            snprintf(sub_topic, sizeof(sub_topic), TOPIC_PREFIX "%s/gpio/+/set", device_id);

            int msg_id = esp_mqtt_client_subscribe(mqtt_client, sub_topic, MQTT_QOS);
            ESP_LOGI(TAG, "Inscrito em: %s (msg_id=%d)", sub_topic, msg_id);
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT desconectado");
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "TOPIC=%.*s DATA=%.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);

            handle_incoming_message(event->topic, event->topic_len,
                                    event->data, event->data_len);
            break;

        default:
            break;
    }
}


void app_interface_start_mqtt(protocol_t *proto)
{
    if (!proto) {
        ESP_LOGE(TAG, "Protocol NULL");
        return;
    }

    for (int i = 0; i < MAX_GPIO_TRACK; i++) {
        last_states[i] = -1;
    }

    char uri[256];
    snprintf(uri, sizeof(uri), "mqtts://%s:%d", MQTT_HOST, MQTT_PORT);

    char client_id[64];
    snprintf(client_id, sizeof(client_id), "%s_client", proto->device_id);

esp_mqtt_client_config_t cfg = {
    .broker.address.uri = uri,

    .credentials.client_id = client_id,
    .credentials.username = MQTT_USER,
    .credentials.authentication.password = MQTT_PASS,

    .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
};

    mqtt_client = esp_mqtt_client_init(&cfg);

    if (!mqtt_client) {
        ESP_LOGE(TAG, "Erro ao criar cliente MQTT");
        return;
    }

    esp_mqtt_client_register_event(
        mqtt_client,
        ESP_EVENT_ANY_ID,
        mqtt_event_handler_cb,
        proto
    );

    esp_err_t err = esp_mqtt_client_start(mqtt_client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao iniciar MQTT: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "MQTT iniciado com sucesso!");
}