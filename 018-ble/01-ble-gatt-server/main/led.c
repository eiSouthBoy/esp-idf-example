#include "esp_log.h"
#include "driver/gpio.h"
#include "led.h"

const char *TAG = "LED";

extern void led_init(void)
{
    ESP_LOGI(TAG, "Example configured to GPIO LED(GPIO %d)!", EXAMPLE_LED_GPIO);
    gpio_reset_pin(EXAMPLE_LED_GPIO);
    gpio_set_direction(EXAMPLE_LED_GPIO, GPIO_MODE_OUTPUT); // 选择输出模式
}

extern void led_set_level(uint32_t level)
{
    gpio_set_level(EXAMPLE_LED_GPIO, level);
}