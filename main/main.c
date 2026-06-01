#include "esp_log.h"
#include "interface.h"
#include "protocol.h"
#include "srv_wifi.h"
#include "mqtt.h"
#include "drv_gpio.h"
#include "sdcard.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";
static protocol_t proto;

void app_main(void)
{
    ESP_LOGI(TAG, "Inicializando sistema...");
    if (!sdcard_init()) {
        ESP_LOGW(TAG, "SD card não disponível. Logs serão feitos apenas no console.");
    }

    app_interface_start_wifi();

    app_interface_start_button();

    while (srv_wifi_get_status() != SRV_WIFI_CONNECTED)
    {
        if (srv_wifi_is_config_mode())
        {
            ESP_LOGW(TAG, "Modo configuração ativo. MAIN pausado.");

            while (1)
            {
                vTaskDelay(pdMS_TO_TICKS(1000));
            }
        }

        ESP_LOGI(TAG, "Aguardando Wi-Fi...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    ESP_LOGI(TAG, "Wi-Fi conectado!");

    protocol_init(&proto);

    srv_initialize_sntp();

    protocol_start(&proto);

    drv_gpio_init(&proto);

    ESP_LOGI(TAG, "Sistema iniciado!");
}