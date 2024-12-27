#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "freertos_sema_bin";
SemaphoreHandle_t xsema_bin;

void taskA(void *pvPram)
{
    while (1)
    {
        // 释放二值信号量
        xSemaphoreGive(xsema_bin);
        ESP_LOGI(TAG, "task A give semaphore binary success");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

void taskB(void *pvPram)
{
    while (1)
    {
        // 获取二值信号量
        if (pdTRUE == xSemaphoreTake(xsema_bin, portMAX_DELAY))
        {
            ESP_LOGI(TAG, "task B take semaphore binary success");
        }
    }
}

void app_main(void)
{
    xsema_bin = xSemaphoreCreateMutex();
    if (NULL == xsema_bin)
    {
        ESP_LOGI(TAG, "Semaphore binary create fail");
        return;
    }

    xTaskCreate(taskA, "task A", 2048, NULL, 10, NULL);
    xTaskCreate(taskB, "task B", 2048, NULL, 10, NULL);
}