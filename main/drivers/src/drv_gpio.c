
#include "drv_gpio.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "GPIO";

static QueueHandle_t gpio_evt_queue = NULL;
static protocol_t *proto_ref;

static const int input_pins[] = {34, 35, 39};
static const int output_pins[] = {2, 4, 16, 17};

#define INPUT_COUNT  (sizeof(input_pins)/sizeof(int))
#define OUTPUT_COUNT (sizeof(output_pins)/sizeof(int))

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    int gpio_num = (int) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task(void* arg)
{
    int io_num;

    for (;;){
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) 
        {
            int level = gpio_get_level(io_num);

            ESP_LOGI(TAG, "GPIO %d mudou para %d", io_num, level);

            protocol_send_gpio(proto_ref, io_num, level);
        }
    }
}

void drv_gpio_init(protocol_t *proto)
{
    proto_ref = proto;

    gpio_evt_queue = xQueueCreate(10, sizeof(int));

    // ✅ PRIMEIRO instala o serviço de ISR
    gpio_install_isr_service(0);

    // -------- INPUTS --------
    for(int i = 0; i < INPUT_COUNT; i++)
    {
        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_ANYEDGE,
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = (1ULL << input_pins[i]),
            .pull_up_en = 1,
            .pull_down_en = 0
        };

        gpio_config(&io_conf);

        // ✅ AGORA pode adicionar handler
        gpio_isr_handler_add(input_pins[i], gpio_isr_handler, (void*) input_pins[i]);
    }

    // -------- OUTPUTS --------
    for(int i = 0; i < OUTPUT_COUNT; i++)
    {
        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = (1ULL << output_pins[i]),
            .pull_up_en = 0,
            .pull_down_en = 0
        };

        gpio_config(&io_conf);

        gpio_set_level(output_pins[i], 0);
    }

    xTaskCreate(gpio_task, "gpio_task", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "GPIO inicializado");
}

void drv_gpio_set(int gpio, int level)
{
    ESP_LOGI(TAG,"%s", __func__);

    for(int i = 0; i < OUTPUT_COUNT; i++)
    {
        if(output_pins[i] == gpio)
        {
            gpio_set_level(gpio, level);
            ESP_LOGI(TAG, "GPIO %d setado para %d", gpio, level);
            return;
        }
    }

    ESP_LOGW(TAG, "GPIO %d não é saída válida!", gpio);
}