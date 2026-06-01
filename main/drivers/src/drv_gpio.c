#include "drv_gpio.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "sdcard.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static const char *TAG = "GPIO";

static QueueHandle_t gpio_evt_queue = NULL;
static protocol_t *proto_ref;

// dynamic storage
static int *input_pins = NULL;
static size_t input_count = 0;
static int *output_pins = NULL;
static size_t output_count = 0;

static const int possible_pins[] = { 2, 4, 15, 16, 17, 13, 14, 27, 26, 25, 33, 32, 35, 34, 39 };
static const size_t possible_pins_count = sizeof(possible_pins)/sizeof(possible_pins[0]);

static const int readonly_pins[] = {35, 34, 39};
static const size_t readonly_pins_count = sizeof(readonly_pins)/sizeof(readonly_pins[0]);

#define DEFAULT_INPUTS_COUNT 3
static const int default_inputs[DEFAULT_INPUTS_COUNT] = {34, 35, 39};

#define DEFAULT_OUTPUTS_COUNT 0
static const int default_outputs[DEFAULT_OUTPUTS_COUNT] = {};

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    int gpio_num = (int) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task(void* arg)
{
    int io_num;

    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) 
        {
            int level = gpio_get_level(io_num);

            ESP_LOGI(TAG, "GPIO %d mudou para %d", io_num, level);
            sdcard_log("GPIO %d mudou para %d", io_num, level);

            protocol_send_gpio(proto_ref, io_num, level);
        }
    }
}

static bool pin_in_list(int pin, const int *list, size_t n)
{
    for (size_t i = 0; i < n; ++i) if (list[i] == pin) return true;
    return false;
}

static bool is_possible_pin(int pin)
{
    return pin_in_list(pin, possible_pins, possible_pins_count);
}

static bool is_readonly_pin(int pin)
{
    return pin_in_list(pin, readonly_pins, readonly_pins_count);
}

void drv_gpio_configure(const int *inputs, size_t n_inputs, const int *outputs, size_t n_outputs)
{
  
    for (size_t i = 0; i < input_count; ++i) 
    {
        gpio_isr_handler_remove(input_pins[i]);
    }

    free(input_pins);
    free(output_pins);
    input_pins = NULL;
    output_pins = NULL;
    input_count = 0;
    output_count = 0;

    if (n_inputs) 
    {
        int *tmp_inputs = malloc(n_inputs * sizeof(int));

        if (tmp_inputs) 
        {
            size_t cnt = 0;

            for (size_t i = 0; i < n_inputs; ++i) 
            {
                int pin = inputs[i];
                tmp_inputs[cnt++] = pin;
            }
            if (cnt) 
            {
                input_pins = malloc(cnt * sizeof(int));

                if (input_pins) 
                {
                    memcpy(input_pins, tmp_inputs, cnt * sizeof(int));
                    input_count = cnt;
                }
            }
            free(tmp_inputs);
        }
    }

    if (n_outputs)
     {
        int *tmp_outputs = malloc(n_outputs * sizeof(int));
        if (tmp_outputs) 
        {
            size_t cnt = 0;

            for (size_t i = 0; i < n_outputs; ++i)
             {
                int pin = outputs[i];

                if (is_readonly_pin(pin)) 
                {
                    ESP_LOGW(TAG, "Ignoring read-only pin for output: %d", pin);
                    continue;
                }
                tmp_outputs[cnt++] = pin;
            }
            if (cnt) 
            {
                output_pins = malloc(cnt * sizeof(int));

                if (output_pins) 
                {
                    memcpy(output_pins, tmp_outputs, cnt * sizeof(int));
                    output_count = cnt;
                }
            }
            free(tmp_outputs);
        }
    }

    if (input_count) 
    {
        uint64_t mask = 0;
        for (size_t i = 0; i < input_count; ++i) mask |= (1ULL << input_pins[i]);

        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_ANYEDGE,
            .mode = GPIO_MODE_INPUT,
            .pin_bit_mask = mask,
            .pull_up_en = 1,
            .pull_down_en = 0
        };
        gpio_config(&io_conf);

        for (size_t i = 0; i < input_count; ++i) {
            gpio_isr_handler_add(input_pins[i], gpio_isr_handler, (void*)input_pins[i]);
        }
    }

    if (output_count) 
    {
        uint64_t mask = 0;
        for (size_t i = 0; i < output_count; ++i) mask |= (1ULL << output_pins[i]);

        gpio_config_t io_conf = {
            .intr_type = GPIO_INTR_DISABLE,
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = mask,
            .pull_up_en = 0,
            .pull_down_en = 0
        };
        gpio_config(&io_conf);

        for (size_t i = 0; i < output_count; ++i) 
        {
            gpio_set_level(output_pins[i], 0);
        }
    }

    ESP_LOGI(TAG, "Configured inputs=%d outputs=%d", (int)input_count, (int)output_count);
}

void drv_gpio_init(protocol_t *proto)
{
    proto_ref = proto;

    gpio_evt_queue = xQueueCreate(10, sizeof(int));

    gpio_install_isr_service(0);

    drv_gpio_configure(default_inputs, DEFAULT_INPUTS_COUNT, default_outputs, DEFAULT_OUTPUTS_COUNT);

    xTaskCreate(gpio_task, "gpio_task", 4096, NULL, 10, NULL);

    ESP_LOGI(TAG, "GPIO inicializado");
}

void drv_gpio_set(int gpio, int level)
{
    ESP_LOGI(TAG,"%s", __func__);

    for(size_t i = 0; i < output_count; i++)
    {
        if(output_pins[i] == gpio)
        {
            gpio_set_level(gpio, level);
            ESP_LOGI(TAG, "GPIO %d setado para %d", gpio, level);
            sdcard_log("GPIO %d setado para %d", gpio, level);
            return;
        }
    }

    ESP_LOGW(TAG, "GPIO %d não é saída válida!", gpio);
    sdcard_log("Tentativa inválida de setar GPIO %d", gpio);
}