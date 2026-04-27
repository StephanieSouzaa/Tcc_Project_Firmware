#include "esp_log.h"
#include "interface.h"
#include "protocol.h"
#include "srv_wifi.h"
#include "mqtt.h"
#include "drv_gpio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

// void app_main(void)
// {
//     ESP_LOGI(TAG, "Inicializando sistema...");

//     app_interface_start_wifi();
//     app_interface_init_gpio();
// }


static protocol_t proto;

void app_main(void)
{
    ESP_LOGI(TAG, "Teste HTTP iniciando...");

    app_interface_start_wifi();

    while(srv_wifi_get_status() != SRV_WIFI_CONNECTED) 
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        ESP_LOGI(TAG, "Aguardando Wi-Fi...");
    }

    protocol_init(&proto);

    ESP_LOGI(TAG, "Wi-Fi conectado, iniciando MQTT...");
    srv_initialize_sntp(); 

    protocol_start(&proto);

    ESP_LOGI(TAG, "Iniciando GPIO...");
    drv_gpio_init(&proto);

    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Enviando POST de teste...");
    protocol_send_gpio(&proto, 1, 1);

    ESP_LOGI(TAG, "POST enviado!");
}