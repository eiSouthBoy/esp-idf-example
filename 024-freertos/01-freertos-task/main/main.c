#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "freertos_task";

void taskA(void *pvPram)
{
    while (1)
    {
        ESP_LOGI(TAG, "[%"PRIu32"] hello world", esp_log_timestamp());

        /* vTaskDelay(1000 / portTICK_PERIOD_MS); 
         * or vTaskDelay(pdMS_TO_TICKS(1000));
        */
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}


void app_main(void)
{
    xTaskCreate(taskA, "task A", 2048, NULL, 10, NULL);
}