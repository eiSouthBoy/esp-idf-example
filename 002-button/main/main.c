#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "iot_button.h"

// #define BUTTON_GPIO_0 0        // Button GPIO
// #define LED_GPIO_4 4           // LED  GPIO
#define BUTTON_ACTIVE_LEVEL 0  // 低电平有效

#define BUTTON_GPIO_0 CONFIG_BUTTON_GPIO_NUM
#define LED_GPIO_4 CONFIG_LED_GPIO_NUM

static const char *TAG = "button-example";
static uint8_t g_led_state = 0;

/* 按键事件类型表 */
const char *button_event_table[] = {
    "BUTTON_PRESS_DOWN",        // 按下
    "BUTTON_PRESS_UP",          // 弹起
    "BUTTON_PRESS_REPEAT",      // 按下弹起次数 >= 2次
    "BUTTON_PRESS_REPEAT_DONE", // 重复按下结束
    "BUTTON_SINGLE_CLICK",      // 按下弹起一次
    "BUTTON_DOUBLE_CLICK",      // 按下弹起两次
    "BUTTON_MULTIPLE_CLICK",    // 指定重复按下次数 N 次，达成时触发
    "BUTTON_LONG_PRESS_START",  // 按下时间达到阈值的瞬间
    "BUTTON_LONG_PRESS_HOLD",   // 长按期间一直触发
    "BUTTON_LONG_PRESS_UP",     // 长按弹起
    "BUTTON_PRESS_REPEAT_DONE", // 多次按下弹起结束
    "BUTTON_PRESS_END"          // 表示button此次检测已结束
};

/* 按键事件回调函数 */
static void button_event_cb(void *arg, void *data)
{
    char btn_event[64] = {0};
    snprintf(btn_event, 64, "%s", button_event_table[(button_event_t)data]);
    ESP_LOGI(TAG, "Button event %s", btn_event);
    iot_button_print_event((button_handle_t)arg);
   
    if (0 == strcmp(btn_event, "BUTTON_PRESS_DOWN"))
    {
        g_led_state = !g_led_state;
        ESP_LOGI(TAG, "Turning the LED %s!", g_led_state == true ? "ON" : "OFF");

        /* Set the GPIO level according to the state (LOW or HIGH)*/
        gpio_set_level(LED_GPIO_4, g_led_state);
    }
}

/* 按键初始化操作 */
static void app_driver_button_init(uint32_t button_num)
{
    esp_err_t err;
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = button_num,
            .active_level = BUTTON_ACTIVE_LEVEL,
        },
    };
    button_handle_t btn = iot_button_create(&btn_cfg);
    assert(btn);
    /* 根据不同的按键事件注册回调 */
    err = iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_PRESS_UP, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT_DONE, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_PRESS_END, button_event_cb, NULL);

    ESP_ERROR_CHECK(err);
}

static void app_driver_led_init(void)
{
    ESP_LOGI(TAG, "Example configured to GPIO LED(GPIO %d)!", LED_GPIO_4);
    gpio_reset_pin(LED_GPIO_4);
    gpio_set_direction(LED_GPIO_4, GPIO_MODE_OUTPUT); /* Set the GPIO as a push/pull output */
    gpio_set_level(LED_GPIO_4, 0);
}

void app_main(void)
{
    app_driver_led_init();
    app_driver_button_init(BUTTON_GPIO_0);
}
