#include "esp_log.h"
#include "interface.h"
#include "protocol.h"
#include "srv_wifi.h"

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

    ESP_LOGI(TAG, "Wi-Fi conectado, enviando POST...");

    protocol_t proto;
    protocol_init(&proto);

    protocol_send_gpio(&proto, 1, 1);

    ESP_LOGI(TAG, "POST enviado!");
}