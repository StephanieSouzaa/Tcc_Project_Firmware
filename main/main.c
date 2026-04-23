#include "esp_log.h"
#include "interface.h"
#include "protocol.h"
#include "srv_wifi.h"
#include "mqtt.h" // updated from "mqtt_client.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

// void app_main(void)
// {
//     ESP_LOGI(TAG, "Inicializando sistema...");

//     app_interface_start_wifi();
//     app_interface_init_gpio();
// }

void app_main(void)
{
    ESP_LOGI(TAG, "Teste HTTP iniciando...");

    app_interface_start_wifi();

    while (srv_wifi_get_status() != SRV_WIFI_CONNECTED) 
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_LOGI(TAG, "Aguardando Wi-Fi...");
    }

    // Prepare protocol and start MQTT (uses device_id)
    protocol_t proto;
    protocol_init(&proto);


    if (srv_wifi_get_status() == SRV_WIFI_CONNECTED)
    {
        ESP_LOGI(TAG, "Wi-Fi conectado, iniciando MQTT...");
        srv_initialize_sntp(); 
        app_interface_start_mqtt(&proto);

        ESP_LOGI(TAG, "Iniciando GPIO...");
        app_interface_init_gpio(&proto);
    }

    // give MQTT a short time to connect/subscribed (adjust if necessary)
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Enviando POST de teste...");

    protocol_send_gpio(&proto, 1, 1);

    ESP_LOGI(TAG, "POST enviado!");
}