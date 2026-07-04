#include "protocol.h"
#include "http.h"
#include "mqtt.h"
#include "drv_gpio.h"
#include "sdcard.h"
#include "esp_log.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdlib.h>

#define DEVICE_ID "device1"
#define HTTP_QUEUE_LEN 20

static const char *TAG = "PROTOCOL";

static QueueHandle_t http_queue = NULL;
static void http_worker_task(void *arg);

typedef struct {
    protocol_t *proto;
    int gpio;
    int state;
    char *message_id;
} http_task_args_t;

void protocol_init(protocol_t *proto)
{
   strncpy(proto->device_id, DEVICE_ID, sizeof(proto->device_id) - 1);
   proto->device_id[sizeof(proto->device_id) - 1] = '\0';

    if (!http_queue) 
    {
        http_queue = xQueueCreate(HTTP_QUEUE_LEN, sizeof(void *));
        if(http_queue) 
        {
            xTaskCreate(http_worker_task, "http_worker", 4096, NULL, 5, NULL);
        } 
        else 
        {
            ESP_LOGW(TAG, "Falha ao criar fila HTTP em init");
        }
    }
}

void protocol_start(protocol_t *proto)
{
    app_interface_start_mqtt(proto);
}

void protocol_send_gpio(protocol_t *proto, int gpio, int state, const char *message_id)
{
    http_send_gpio(proto, gpio, state, message_id);
}

static void http_worker_task(void *arg)
{
    (void)arg;
    for (;;)
     {
        http_task_args_t *t = NULL;
        if (xQueueReceive(http_queue, &t, portMAX_DELAY) == pdPASS) {
            if (t) 
            {
                http_send_gpio(t->proto, t->gpio, t->state, t->message_id);
                if (t->message_id) free(t->message_id);
                free(t);
            }
        }
    }
    vTaskDelete(NULL);
}

void protocol_handle_gpio_command(protocol_t *proto, int gpio, int state, const char *message_id)
{
    //ESP_LOGI(TAG, "Executando comando: GPIO %d -> %d (msg_id=%s)", gpio, state, message_id?message_id:"-");
    sdcard_log("Executando comando: GPIO %d -> %d (msg_id=%s)", gpio, state, message_id?message_id:"-");

    if (drv_gpio_set(gpio, state)) 
    {
        http_task_args_t *args = malloc(sizeof(*args));
        if (args) 
        {
            args->proto = proto;
            args->gpio = gpio;
            args->state = state;
            args->message_id = message_id ? strdup(message_id) : NULL;

            if(http_queue && xQueueSend(http_queue, &args, pdMS_TO_TICKS(200)) == pdPASS) 
            {
                // queued successfully
            } 
            else 
            {
                //ESP_LOGW(TAG, "Fila HTTP cheia ou não criada, enviando diretamente");
                if (args->message_id) 
                {
                    http_send_gpio(proto, gpio, state, args->message_id);
                    free(args->message_id);
                }
                else 
                {
                    http_send_gpio(proto, gpio, state, NULL);
                }
                free(args);
            }
        } 
        else 
        {
            //ESP_LOGW(TAG, "Sem memória para task HTTP, enviando diretamente");
            protocol_send_gpio(proto, gpio, state, message_id);
        }
    }
    
}