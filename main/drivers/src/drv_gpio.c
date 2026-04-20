#include "drv_gpio.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define GPIO_INPUT 4

static const char *TAG = "GPIO";

static QueueHandle_t gpio_evt_queue = NULL;
static protocol_t *proto_ref;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    int gpio_num = (int) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task(void* arg)
{
    int io_num;

    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            int level = gpio_get_level(io_num);

            ESP_LOGI(TAG, "GPIO %d mudou para %d", io_num, level);

            protocol_send_gpio(proto_ref, io_num, level);
        }
    }
}

void drv_gpio_init(protocol_t *proto)
{
    proto_ref = proto;

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << GPIO_INPUT),
        .pull_up_en = 1,
    };

    gpio_config(&io_conf);

    gpio_evt_queue = xQueueCreate(10, sizeof(int));

    xTaskCreate(gpio_task, "gpio_task", 4096, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(GPIO_INPUT, gpio_isr_handler, (void*) GPIO_INPUT);
}