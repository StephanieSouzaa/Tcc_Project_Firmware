#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <stdbool.h>
#include <stdint.h>

#include "mqtt.h"
#include "interface.h"
#include "srv_wifi.h"

static const char *TAG = "INTERFACE";
#define WIFI_BUTTON_GPIO GPIO_NUM_36
#define BUTTON_HOLD_MS 5000
#define BUTTON_POLL_MS 100

static void wifi_button_task(void *arg)
{
    (void)arg;
    int64_t press_start = 0;
    bool pressed = false;

    while (true) {
        int level = gpio_get_level(WIFI_BUTTON_GPIO);

        if (level == 1) 
        {
            ESP_LOGI(TAG, "GPIO LEVEL: %d", level);
            if (!pressed) 
            {
                pressed = true;
                press_start = esp_timer_get_time();
            } 
            else 
            {
                int64_t elapsed = esp_timer_get_time() - press_start;

                if (elapsed >= BUTTON_HOLD_MS * 1000LL) 
                {
                 ESP_LOGI(TAG, "Botão pressionado por 5 segundos");

                 app_interface_start_config_mode();

                    while (gpio_get_level(WIFI_BUTTON_GPIO) == 0) 
                    {
                        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
                    }
                    pressed = false;
                }
            }
        } else {
            pressed = false;
        }

        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_MS));
    }
}

void app_interface_start_wifi(void)
{
    srv_wifi_init();

    if (srv_wifi_has_saved_credentials())
    {
        char ssid[32];
        char password[64];

        if (srv_wifi_load_credentials(ssid, sizeof(ssid), password, sizeof(password)))
        {
            ESP_LOGI(TAG, "Credenciais salvas encontradas: %s", ssid);
            srv_wifi_connect(ssid, password);
            return;
        }
    }

    ESP_LOGW(TAG, "Nenhuma credencial Wi-Fi salva.");

    app_interface_start_config_mode();
}

void app_interface_start_button(void)
{
    gpio_config_t cfg = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = 1ULL << WIFI_BUTTON_GPIO,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };

    gpio_config(&cfg);
    xTaskCreate(wifi_button_task, "wifi_button", 4096, NULL, 5, NULL);
}

void app_interface_start_config_mode(void)
{
    ESP_LOGW(TAG, "Iniciando modo configuração Wi-Fi...");

    mqtt_stop();

    srv_wifi_start_ap();

    vTaskDelay(pdMS_TO_TICKS(2000));

    srv_wifi_scan_networks();

    srv_wifi_start_config_server();
}