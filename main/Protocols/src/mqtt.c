#include "esp_log.h"
#include "mqtt_client.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_crt_bundle.h"
#include "esp_system.h"
#include "esp_random.h"   

#include "mqtt.h"
#include "protocol.h"

static const char *TAG = "MQTT_CLIENT";

#define MQTT_HOST     "92ca7f6444f944cbae30908602073cd5.s1.eu.hivemq.cloud"
#define MQTT_PORT     8883
#define MQTT_USER     "Steph"
#define MQTT_PASS     "Steph123"
#define TOPIC_PREFIX  "tcc_project/"
#define MQTT_QOS      0

static esp_mqtt_client_handle_t mqtt_client = NULL;

static void mqtt_event_handler_cb(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data);

// void app_interface_start_mqtt(protocol_t *proto)
// {
//     ESP_LOGI(TAG, "%s", __func__);
//     char uri[256];
//     snprintf(uri, sizeof(uri), "mqtts://%s:%d", MQTT_HOST, MQTT_PORT);

//     char client_id[64];
//     snprintf(client_id, sizeof(client_id), "%s_client", proto->device_id);

//     esp_mqtt_client_config_t cfg = {
//         .broker.address.uri = uri,

//         .credentials.client_id = client_id,
//         .credentials.username = MQTT_USER,
//         .credentials.authentication.password = MQTT_PASS,

//         .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
//     };

//     mqtt_client = esp_mqtt_client_init(&cfg);

//     esp_mqtt_client_register_event(
//         mqtt_client,
//         ESP_EVENT_ANY_ID,
//         mqtt_event_handler_cb,
//         proto
//     );

//     esp_mqtt_client_start(mqtt_client);

//     ESP_LOGI(TAG, "MQTT iniciado");
// }

void app_interface_start_mqtt(protocol_t *proto)
{
    if (!proto || strlen(proto->device_id) == 0) {
        ESP_LOGE(TAG, "device_id inválido!");
        return;
    }

    char uri[128];
    snprintf(uri, sizeof(uri), "mqtts://%s:%d", MQTT_HOST, MQTT_PORT);

    char client_id[64];
    snprintf(client_id, sizeof(client_id), "%s", proto->device_id);

    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = uri,

        .credentials.client_id = client_id,
        .credentials.username = MQTT_USER,
        .credentials.authentication.password = MQTT_PASS,

        .session.keepalive = 30,

        .network.timeout_ms = 10000,
        .network.reconnect_timeout_ms = 5000,

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

    ESP_LOGI(TAG, "MQTT iniciado!");
}

static void handle_incoming_message(protocol_t *proto, const char *topic, int topic_len, const char *data, int data_len)
{
    ESP_LOGI(TAG, "%s", __func__);
    ESP_LOGI(TAG, "Mensagem recebida: topic=%.*s, data=%.*s", topic_len, topic, data_len, data);

    char topic_buf[128];
    char payload[64];

    snprintf(topic_buf, topic_len + 1, "%s", topic);
    snprintf(payload, data_len + 1, "%s", data);

    char *p = strstr(topic_buf, "/gpio/");
    
    if (!p) return;

    int gpio = atoi(p + 6);
    int state = atoi(payload);

    protocol_handle_gpio_command(proto, gpio, state);
}

static void mqtt_event_handler_cb(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    protocol_t *proto = (protocol_t *) handler_args;

    switch (event_id) {

        case MQTT_EVENT_CONNECTED: {
            ESP_LOGI(TAG, "MQTT conectado");

            if (!proto || strlen(proto->device_id) == 0) {
                ESP_LOGE(TAG, "device_id inválido no subscribe!");
                return;
            }

            char sub_topic[128];
            snprintf(sub_topic, sizeof(sub_topic),TOPIC_PREFIX "%s/gpio/1/set",proto->device_id);

            int msg_id = esp_mqtt_client_subscribe(mqtt_client, sub_topic, MQTT_QOS);

            ESP_LOGI(TAG, "Inscrito em: %s (msg_id=%d)", sub_topic, msg_id);
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT desconectado");
            break;

        case MQTT_EVENT_DATA: {
            char topic[128] = {0};
            char data[64] = {0};

            memcpy(topic, event->topic, event->topic_len);
            memcpy(data, event->data, event->data_len);

            ESP_LOGI(TAG, "TOPIC=%s DATA=%s", topic, data);

            char *p = strstr(topic, "/gpio/");
            if (!p) return;

            int gpio = atoi(p + 6);
            int state = atoi(data);

            protocol_handle_gpio_command(proto, gpio, state);
            break;
        }

        default:
            break;
    }
}