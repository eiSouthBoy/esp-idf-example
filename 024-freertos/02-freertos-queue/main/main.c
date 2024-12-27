#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

static const char *TAG = "freertos_queue";
static QueueHandle_t qhandle;

typedef struct 
{
    int value;
}queue_data_t;


void taskA(void *pvPram)
{
    int count = 0;
    queue_data_t qdata;
    memset(&qdata, 0, sizeof(queue_data_t));

    while (1)
    {
        qdata.value = count;
        // no block
        if (pdTRUE == xQueueSend(qhandle, (void *)&qdata, (TickType_t)0))
        {
            ESP_LOGI(TAG, "Queue send data: %d", qdata.value);
            count++;
        }
        else
        {
            ESP_LOGI(TAG, "Queue send fail");
        }
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void taskB(void *pvPram)
{
    queue_data_t qdata;
    memset(&qdata, 0, sizeof(queue_data_t));

    while (1)
    {
        if (pdTRUE == xQueueReceive(qhandle, &qdata, portTICK_PERIOD_MS))
        {
            ESP_LOGI(TAG, "Queue recv data: %d", qdata.value);
        }
        else
        {
            ESP_LOGI(TAG, "Queue recv fail");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void app_main(void)
{
    qhandle = xQueueCreate(10, sizeof(queue_data_t));
    if (NULL == qhandle)
    {
        ESP_LOGI(TAG, "Queue create fail");
        return;
    }

    xTaskCreate(taskA, "task A", 2048, NULL, 10, NULL);
    xTaskCreate(taskB, "task B", 2048, NULL, 10, NULL);
}