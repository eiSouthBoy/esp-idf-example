#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "gpio_example";  

#define _SUSPEND_GPIO GPIO_NUM_4

void task_check_gpio_level(void *pvPram)
{
    int _suspend_level = 0;
    gpio_set_level(_SUSPEND_GPIO, GPIO_MODE_INPUT);
    while (1)
    {
        _suspend_level = gpio_get_level(_SUSPEND_GPIO);
        ESP_LOGI(TAG, "gpio%d level: %d", _SUSPEND_GPIO, _suspend_level);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    // xTaskCreate(task_check_gpio_level, "task A", 2048, NULL, 10, NULL);
    for (int i = 0; i < 10; i++)
    {
        ESP_LOGI(TAG, "hello world");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
