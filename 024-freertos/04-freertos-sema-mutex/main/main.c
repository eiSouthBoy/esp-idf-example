#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "freertos_sema_bin";
static SemaphoreHandle_t xsema_mutex;
static int count = 0;

void taskA(void *pvPram)
{
    while (1)
    {
        xSemaphoreTake(xsema_mutex, portMAX_DELAY);

        ESP_LOGI(TAG, "task A, count: %d", count++);

        xSemaphoreGive(xsema_mutex);
        
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void taskB(void *pvPram)
{
    while (1)
    {
        xSemaphoreTake(xsema_mutex, portMAX_DELAY);

        ESP_LOGI(TAG, "task B, count: %d", count++);

        xSemaphoreGive(xsema_mutex);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void app_main(void)
{
    xsema_mutex = xSemaphoreCreateMutex();
    if (NULL == xsema_mutex)
    {
        ESP_LOGI(TAG, "Semaphore mutex create fail");
        return;
    }

    xTaskCreate(taskA, "task A", 2048, NULL, 10, NULL);
    xTaskCreate(taskB, "task B", 2048, NULL, 10, NULL);
}