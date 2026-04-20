#include <string.h>
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <time.h>
#include "esp_sntp.h"

#include "srv_wifi.h"

static const char *TAG = "SRV_WIFI";
static srv_wifi_status_t wifi_status = SRV_WIFI_DISCONNECTED;
static void initialize_sntp_and_wait();

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        wifi_status = SRV_WIFI_CONNECTING;
        ESP_LOGI(TAG, "Conectando ao Wi-Fi...");
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_status = SRV_WIFI_DISCONNECTED;
        ESP_LOGW(TAG, "Wi-Fi desconectado, tentando reconectar...");
        esp_wifi_connect();
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) 
    {
    
        ESP_LOGI(TAG, "Obtido IP, iniciando sincronizacao de horario (SNTP)...");
        initialize_sntp_and_wait();
        wifi_status = SRV_WIFI_CONNECTED;
        ESP_LOGI(TAG, "Wi-Fi conectado com sucesso!");
    }
}

static void initialize_sntp_and_wait(void)
{
    ESP_LOGI(TAG, "Inicializando SNTP e configurando timezone (America/Sao_Paulo)");
    setenv("TZ", "America/Sao_Paulo", 1);
    tzset();

    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    int retry = 0;
    const int retry_count = 15;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < retry_count) {
        ESP_LOGI(TAG, "Aguardando sincronizacao SNTP... (%d/%d)", retry+1, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
        retry++;
    }

    if (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET) 
    {
        ESP_LOGW(TAG, "SNTP não sincronizado após timeout");
    } 
    else
    {
        time_t now = 0;
        struct tm timeinfo = { 0 };
        time(&now);
        localtime_r(&now, &timeinfo);
        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
        ESP_LOGI(TAG, "SNTP sincronizado, hora atual: %s", strftime_buf);
    }
}

esp_err_t srv_wifi_init(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    return ESP_OK;
}

esp_err_t srv_wifi_connect(const char *ssid, const char *password)
{
    wifi_config_t wifi_config = {0};

    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

srv_wifi_status_t srv_wifi_get_status(void)
{
    return wifi_status;
}