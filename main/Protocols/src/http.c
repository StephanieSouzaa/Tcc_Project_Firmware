
#include "http.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>

static const char *TAG = "HTTP";

#define API_BASE_URL "http://br76.teste.website/~steph999/api/populate.php"

void http_send_gpio(protocol_t *proto, int gpio, int state, const char *message_id)
{
    char timestamp[32];

    time_t now;
    time(&now);

    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo);

    char post_data[320];
    snprintf(post_data, sizeof(post_data),
             "timestamp=%s&device_id=%s&gpio_id=%d&state=%d&message_id=%s",
             timestamp,
             proto->device_id,
             gpio,
             state,
             message_id?message_id:"");

    esp_http_client_config_t config = {
        .url = API_BASE_URL,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    if (!client) return;

    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK)
    {
        int status = esp_http_client_get_status_code(client);
        //ESP_LOGI(TAG, "HTTP OK (%d)", status);
    }
    else
    {
        ESP_LOGE(TAG, "Erro HTTP: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}