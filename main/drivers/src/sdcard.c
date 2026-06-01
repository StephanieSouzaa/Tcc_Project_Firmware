#include "sdcard.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

static const char *TAG = "SDCARD";
static bool s_mounted = false;

static void sdcard_timestamp(char *buffer, size_t size)
{
    time_t now = time(NULL);
    struct tm timeinfo;

    if (localtime_r(&now, &timeinfo) != NULL) {
        strftime(buffer, size, "%Y-%m-%d %H:%M:%S", &timeinfo);
    } else {
        strncpy(buffer, "0000-00-00 00:00:00", size - 1);
        buffer[size - 1] = '\0';
    }
}

bool sdcard_init(void)
{
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 5,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t *card;

    spi_bus_config_t bus_cfg = {
        .mosi_io_num = 23,
        .miso_io_num = 19,
        .sclk_io_num = 18,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4000,
    };

    esp_err_t err = spi_bus_initialize(
        SPI2_HOST,
        &bus_cfg,
        SDSPI_DEFAULT_DMA
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao inicializar SPI: %s",
                 esp_err_to_name(err));
        return false;
    }

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI2_HOST;
    host.max_freq_khz = 400;

    sdspi_device_config_t slot_config =
        SDSPI_DEVICE_CONFIG_DEFAULT();

    slot_config.host_id = SPI2_HOST;
    slot_config.gpio_cs = 5;

    err = esp_vfs_fat_sdspi_mount(
        "/sdcard",
        &host,
        &slot_config,
        &mount_config,
        &card
    );

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao montar SD card: %s",
                 esp_err_to_name(err));

        spi_bus_free(SPI2_HOST);
        s_mounted = false;
        return false;
    }

    ESP_LOGI(TAG, "SD card montado em /sdcard");
    s_mounted = true;
    return true;
}

void sdcard_log(const char *fmt, ...)
{
    if (!s_mounted) {
        return;
    }

    FILE *f = fopen("/sdcard/log.txt", "a");
    if (!f) {
        ESP_LOGE(TAG, "Não foi possível abrir /sdcard/log.txt");
        return;
    }

    char timestamp[32];
    sdcard_timestamp(timestamp, sizeof(timestamp));

    fprintf(f, "[%s] ", timestamp);

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);

    fprintf(f, "\n");
    fclose(f);
}
