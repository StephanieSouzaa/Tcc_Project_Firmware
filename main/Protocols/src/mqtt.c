#include "esp_log.h"
#include "mqtt_client.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "drv_gpio.h"
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

void mqtt_stop(void)
{
    if (mqtt_client != NULL)
    {
        ESP_LOGW(TAG, "Parando MQTT...");

        esp_mqtt_client_stop(mqtt_client);
        esp_mqtt_client_destroy(mqtt_client);

        mqtt_client = NULL;
    }
}

static void handle_incoming_message(protocol_t *proto, char *topic, int topic_len, const char *data, int data_len)
{
    ESP_LOGI(TAG, "%s", __func__);
    ESP_LOGI(TAG, "Mensagem recebida: topic=%.*s, data=%.*s", topic_len, topic, data_len, data);

    char topic_buf[128];
    char payload[512];

    int tlen = (topic_len < sizeof(topic_buf) - 1) ? topic_len : sizeof(topic_buf) - 1;

    memcpy(topic_buf, topic, tlen);
    topic_buf[tlen] = '\0';

    int plen = (data_len < sizeof(payload) - 1) ? data_len : sizeof(payload) - 1;

    memcpy(payload, data, plen);
    payload[plen] = '\0';

    char *p = strstr(topic_buf, "/gpio/");
    
    if (!p)
    {
        ESP_LOGW(TAG, "Topico nao contem /gpio/");
        return;
    }

    int gpio = atoi(p + 6);

    cJSON *root = cJSON_Parse(payload);

    if (!root)
    {
        ESP_LOGW(TAG, "JSON invalido");
        return;
    }

    cJSON *cmd = cJSON_GetObjectItem(root, "command");
    cJSON *msg_id = cJSON_GetObjectItem(root, "message_id");

    if (!cJSON_IsString(cmd) || !cJSON_IsString(msg_id))
    {
        ESP_LOGW(TAG, "Payload invalido");
        cJSON_Delete(root);
        return;
    }

    int state = atoi(cmd->valuestring);

    ESP_LOGI(TAG, "Executando comando: GPIO %d -> %d (msg_id=%s)", gpio, state, msg_id->valuestring);

    protocol_handle_gpio_command(proto, gpio, state, msg_id->valuestring);

    cJSON_Delete(root);
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

            // subscribe to gpio set
            snprintf(sub_topic, sizeof(sub_topic), TOPIC_PREFIX "%s/gpio/+/set", device_id);
            int msg_id = esp_mqtt_client_subscribe(mqtt_client, sub_topic, MQTT_QOS);
            ESP_LOGI(TAG, "Inscrito em: %s (msg_id=%d)", sub_topic, msg_id);

            // subscribe to config topic
            char config_topic[128];
            snprintf(config_topic, sizeof(config_topic), TOPIC_PREFIX "%s/config", device_id);
            int cfg_id = esp_mqtt_client_subscribe(mqtt_client, config_topic, MQTT_QOS);
            ESP_LOGI(TAG, "Inscrito em: %s (msg_id=%d)", config_topic, cfg_id);
            break;
        }

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "MQTT desconectado");
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "TOPIC=%.*s DATA=%.*s",
                     event->topic_len, event->topic,
                     event->data_len, event->data);

            // if this is a /config topic, parse JSON and reconfigure GPIO
            {
                // construct topic string
                char topic_buf[128];
                int tlen = (event->topic_len < (int)sizeof(topic_buf)-1) ? event->topic_len : (int)sizeof(topic_buf)-1;
                memcpy(topic_buf, event->topic, tlen);
                topic_buf[tlen] = '\0';

                // check if topic ends with "/config"
                const char *suffix = "/config";
                size_t slen = strlen(suffix);
                if (tlen >= (int)slen && strcmp(topic_buf + tlen - slen, suffix) == 0) {
                    // parse JSON payload
                    char payload_buf[512];
                    int plen = (event->data_len < (int)sizeof(payload_buf)-1) ? event->data_len : (int)sizeof(payload_buf)-1;
                    memcpy(payload_buf, event->data, plen);
                    payload_buf[plen] = '\0';

                    cJSON *root = cJSON_Parse(payload_buf);
                    if (root) {
                        cJSON *inputs = cJSON_GetObjectItemCaseSensitive(root, "inputs");
                        cJSON *outputs = cJSON_GetObjectItemCaseSensitive(root, "outputs");

                        int *ins = NULL; size_t ins_n = 0;
                        int *outs = NULL; size_t outs_n = 0;

                        if (cJSON_IsArray(inputs)) {
                            ins_n = cJSON_GetArraySize(inputs);
                            if (ins_n) {
                                ins = malloc(sizeof(int) * ins_n);
                                for (size_t i = 0; i < ins_n; ++i) {
                                    cJSON *it = cJSON_GetArrayItem(inputs, i);
                                    ins[i] = cJSON_IsNumber(it) ? it->valueint : -1;
                                }
                            }
                        }

                        if (cJSON_IsArray(outputs)) {
                            outs_n = cJSON_GetArraySize(outputs);
                            if (outs_n) {
                                outs = malloc(sizeof(int) * outs_n);
                                for (size_t i = 0; i < outs_n; ++i) {
                                    cJSON *it = cJSON_GetArrayItem(outputs, i);
                                    outs[i] = cJSON_IsNumber(it) ? it->valueint : -1;
                                }
                            }
                        }

                        ESP_LOGI(TAG, "Applying GPIO config: inputs=%d outputs=%d", (int)ins_n, (int)outs_n);
                        drv_gpio_configure(ins, ins_n, outs, outs_n);

                        free(ins); free(outs);
                        cJSON_Delete(root);
                    } else {
                        ESP_LOGW(TAG, "Invalid JSON in config payload");
                    }

                } else {
                    // handle normal gpio set messages
                    handle_incoming_message(proto, event->topic, event->topic_len, event->data, event->data_len);
                }
            }

            break;
        

        default:
            break;
    }
}