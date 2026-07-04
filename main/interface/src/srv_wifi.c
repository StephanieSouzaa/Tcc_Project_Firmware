#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_http_server.h"
#include <time.h>
#include "esp_sntp.h"

#include "srv_wifi.h"

#define WIFI_AP_SSID "Euroino-Setup"
#define WIFI_AP_PASS "12345678"
#define WIFI_NVS_NAMESPACE "wifi_config"
#define WIFI_SSID_KEY "ssid"
#define WIFI_PASSWORD_KEY "password"

static const char *TAG = "SRV_WIFI";
static httpd_handle_t server = NULL;
static srv_wifi_status_t wifi_status = SRV_WIFI_DISCONNECTED;
static void initialize_sntp_and_wait();
static bool config_mode_active = false;
static wifi_ap_record_t ap_list[20];
static uint16_t ap_count = 0;

esp_err_t srv_wifi_save_credentials(const char *ssid, const char *password);


static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START)
    {
        if (!config_mode_active)
        {
            ESP_LOGI(TAG, "Conectando STA...");
            esp_wifi_connect();
            wifi_status = SRV_WIFI_CONNECTING;
        }
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_status = SRV_WIFI_DISCONNECTED;

        wifi_mode_t mode;
        esp_wifi_get_mode(&mode);

        if (!config_mode_active && (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA))
        {
            ESP_LOGI(TAG, "Tentando reconectar...");
            esp_wifi_connect();
        }
        else
        {
            ESP_LOGI(TAG, "Modo configuração ativo. Sem reconexão.");
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_status = SRV_WIFI_CONNECTED;

        ESP_LOGI(TAG, "Wi-Fi conectado!");
    }
}

void srv_initialize_sntp()
{
    initialize_sntp_and_wait();
}

static void initialize_sntp_and_wait(void)
{
    ESP_LOGI(TAG, "Inicializando SNTP...");

    setenv("TZ", "America/Sao_Paulo", 1);
    tzset();

    sntp_setoperatingmode(SNTP_OPMODE_POLL);

    // 🔥 vários servidores (isso muda tudo)
    sntp_setservername(0, "pool.ntp.org");
    sntp_setservername(1, "time.google.com");
    sntp_setservername(2, "a.st1.ntp.br");

    sntp_init();

    int retry = 0;
    const int retry_count = 30; // 🔥 aumentei

    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && retry < retry_count)
    {
        ESP_LOGI(TAG, "Aguardando SNTP... (%d/%d)", retry + 1, retry_count);
        vTaskDelay(pdMS_TO_TICKS(2000));
        retry++;
    }

    time_t now = 0;
    time(&now);

    if (now < 1577836800)
    {
        ESP_LOGW(TAG, "SNTP falhou! Tempo inválido");
    }
    else
    {
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        char strftime_buf[64];
        strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);

        ESP_LOGI(TAG, "SNTP OK! Hora: %s", strftime_buf);
    }
}

esp_err_t srv_wifi_init(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();

    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
    {
        ESP_ERROR_CHECK(err);
    }

    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    return ESP_OK;
}

esp_err_t srv_wifi_start_ap(void)
{
    config_mode_active = true;
    esp_wifi_disconnect();
    esp_wifi_stop();

    wifi_config_t ap_config = {
        .ap = {
            .ssid = WIFI_AP_SSID,
            .password = WIFI_AP_PASS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .max_connection = 4,
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP ativo: %s", WIFI_AP_SSID);
    ESP_LOGI(TAG, "http://192.168.4.1");

    return ESP_OK;
}

void srv_wifi_scan_networks(void)
{
   wifi_scan_config_t scan_config = {
    .ssid = 0,
    .bssid = 0,
    .channel = 0,
    .show_hidden = false
};

    ESP_LOGI(TAG, "Escaneando redes...");

    esp_err_t err = esp_wifi_scan_start(&scan_config, true);

    if(err != ESP_OK) 
    {
    ESP_LOGE(TAG, "Erro ao iniciar scan: %s", esp_err_to_name(err));
    }

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_list));

    ESP_LOGI(TAG, "Encontradas %d redes", ap_count);
}

static void wifi_list_to_html(char *buffer, size_t max_len)
{
    strncat(buffer,
        "<label>Selecione a rede:</label><br>"
        "<select name='ssid' style='width:250px;height:35px;'>",
        max_len - strlen(buffer) - 1);

    if (ap_count == 0)
    {
        strncat(buffer,
            "<option>Nenhuma rede encontrada</option>",
            max_len - strlen(buffer) - 1);
    }

   for(int i = 0; i < ap_count; i++)
   {
    if (strlen((char*)ap_list[i].ssid) == 0) continue;

    strncat(buffer, "<option value='", max_len - strlen(buffer) - 1);
    strncat(buffer, (char*)ap_list[i].ssid, max_len - strlen(buffer) - 1);
    strncat(buffer, "'>", max_len - strlen(buffer) - 1);
    strncat(buffer, (char*)ap_list[i].ssid, max_len - strlen(buffer) - 1);
    strncat(buffer, "</option>", max_len - strlen(buffer) - 1);
}

    strncat(buffer, "</select>", max_len - strlen(buffer) - 1);
}

static esp_err_t root_get(httpd_req_t *req)
{
    #define HTML_BUFFER_SIZE 4096

    char *html = malloc(HTML_BUFFER_SIZE);

    if (!html)
        return ESP_FAIL;

    html[0] = '\0';

    strcat(html,
           "<html><head>"
           "<meta charset='UTF-8'>"
           "<meta name='viewport' content='width=device-width'>"
           "</head><body style='text-align:center;font-family:Arial;'>"
           "<h2>📡 Arduino Setup</h2>"
           "<form method='POST' action='/save'>");

    wifi_list_to_html(html, HTML_BUFFER_SIZE);

    strcat(html,
           "<br><br>"
           "Senha:<br>"
           "<input name='pass' type='password'><br><br>"
           "<input type='submit' value='Conectar'>"
           "</form></body></html>");

    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);

    free(html);

    return ESP_OK;
}

static esp_err_t save_post(httpd_req_t *req)
{
    char buf[256] = {0};

    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);

    if (len <= 0) return ESP_FAIL;
    
    buf[len] = '\0';

    //ESP_LOGI(TAG, "POST RECEBIDO: %s", buf);

    char ssid[64] = {0};
    char pass[64] = {0};

    httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid));
    httpd_query_key_value(buf, "pass", pass, sizeof(pass));

    ESP_LOGI(TAG, "SSID: %s", ssid);
    ESP_LOGI(TAG, "PASS: %s", pass);

    srv_wifi_save_credentials(ssid, pass);

    httpd_resp_sendstr(req, "WiFi salvo! Reiniciando...");

    vTaskDelay(pdMS_TO_TICKS(2000));

    esp_restart();

    return ESP_OK;
}

esp_err_t srv_wifi_start_config_server(void)
{
    if (server != NULL)
    {
        ESP_LOGW(TAG, "Servidor já rodando");
        return ESP_OK;
    }

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get
    };

    httpd_uri_t save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = save_post
    };

    httpd_register_uri_handler(server, &root);
    httpd_register_uri_handler(server, &save);

    return ESP_OK;
}

srv_wifi_status_t srv_wifi_get_status(void)
{
    return wifi_status;
}

esp_err_t srv_wifi_save_credentials(const char *ssid, const char *password)
{
    nvs_handle_t nvs;
    nvs_open(WIFI_NVS_NAMESPACE, NVS_READWRITE, &nvs);

    nvs_set_str(nvs, WIFI_SSID_KEY, ssid);
    nvs_set_str(nvs, WIFI_PASSWORD_KEY, password);

    nvs_commit(nvs);
    nvs_close(nvs);

    return ESP_OK;
}

bool srv_wifi_load_credentials(char *ssid, size_t ssid_size, char *password, size_t pass_size)
{
    nvs_handle_t nvs;

    if (nvs_open(WIFI_NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK) return false;
    if (nvs_get_str(nvs, WIFI_SSID_KEY, ssid, &ssid_size) != ESP_OK) return false;
    if (nvs_get_str(nvs, WIFI_PASSWORD_KEY, password, &pass_size) != ESP_OK) return false;

    nvs_close(nvs);
    return true;
}

bool srv_wifi_has_saved_credentials(void)
{
    char ssid[32], pass[64];
    return srv_wifi_load_credentials(ssid, sizeof(ssid), pass, sizeof(pass));
}

esp_err_t srv_wifi_connect(const char *ssid, const char *password)
{
    wifi_config_t wifi_config = {0};

    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));

    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);

    ESP_LOGI("SRV_WIFI", "Conectando ao SSID: %s", ssid);

    return ESP_OK;
}

bool srv_wifi_is_config_mode(void)
{
    return config_mode_active;
}