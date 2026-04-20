#include "protocol.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

static const char *TAG = "PROTOCOL";

#define API_BASE_URL "http://br76.teste.website/~steph999/api/populate.php"

void protocol_init(protocol_t *proto)
{
    strcpy(proto->device_id, "device1");
}

void protocol_send_gpio(protocol_t *proto, int gpio, int state)
{
    char timestamp[32];
    time_t now;
    time(&now);

    const int BRASIL_OFFSET_SECONDS = 3 * 3600; 
    time_t br_now = now - BRASIL_OFFSET_SECONDS;
    struct tm timeinfo;
    gmtime_r(&br_now, &timeinfo);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

    char timestamp_enc[64];
    strncpy(timestamp_enc, timestamp, sizeof(timestamp_enc));
    timestamp_enc[sizeof(timestamp_enc)-1] = '\0';
    for (size_t i = 0; timestamp_enc[i] != '\0'; ++i) {
        if (timestamp_enc[i] == ' ') timestamp_enc[i] = '+';
    }

    char post_data[256];
    snprintf(post_data, sizeof(post_data),
             "timestamp=%s&device_id=%s&gpio_id=%d&state=%d",
             timestamp_enc, proto->device_id, gpio, 0);


    esp_http_client_config_t config = {
        .url = API_BASE_URL,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Falha ao inicializar http client");
        return;
    }

    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "POST enviado com sucesso, status = %d", status);

        int content_length = esp_http_client_get_content_length(client);
        if (content_length > 0) {
            char *body = malloc(content_length + 1);
            if (body) {
                int read_len = esp_http_client_read(client, body, content_length);
                if (read_len >= 0) {
                    body[read_len] = '\0';
                    ESP_LOGI(TAG, "Response body: %s", body);
                } else {
                    ESP_LOGW(TAG, "Falha ao ler corpo da resposta (read_len=%d)", read_len);
                }
                free(body);
            } else {
                ESP_LOGW(TAG, "Não foi possível alocar buffer para resposta");
            }
        } else {
            char buf[256];
            int rlen = 0;
            while ((rlen = esp_http_client_read(client, buf, sizeof(buf) - 1)) > 0) {
                buf[rlen] = '\0';
                ESP_LOGI(TAG, "Response chunk: %s", buf);
            }
        }
    } else {
        ESP_LOGE(TAG, "Erro na requisição: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}