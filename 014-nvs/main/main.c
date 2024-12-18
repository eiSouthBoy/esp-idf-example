#include <stdio.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"

#include "iot_button.h"

#define EXAMPLE_BUTTON_GPIO CONFIG_BUTTON_GPIO_NUM
#define EXAMPLE_LED_GPIO CONFIG_LED_GPIO_NUM

static const char *TAG = "nvs_readwrite";
static uint8_t g_led_state;
static nvs_handle_t g_handle;

static void app_driver_led_init(void)
{
    ESP_LOGI(TAG, "Example configured to GPIO LED(GPIO %d)!", EXAMPLE_LED_GPIO);
    gpio_reset_pin(EXAMPLE_LED_GPIO); // 重启GPIO_4的电平状态
    gpio_set_direction(EXAMPLE_LED_GPIO, GPIO_MODE_OUTPUT); // 选择输出模式
}

/* 按键事件（BUTTON_PRESS_DOWN）回调函数 */
static void button_press_down_event_cb(void *arg, void *data)
{
    iot_button_print_event((button_handle_t)arg);
    ESP_LOGI(TAG, "Button Presss Down");

    g_led_state = !g_led_state;
    gpio_set_level(EXAMPLE_LED_GPIO, g_led_state);

    // NVS 写操作
    ESP_LOGI(TAG, "Updating LED State in NVS ...");
    esp_err_t err = nvs_set_u8(g_handle, "led_state", g_led_state);
    ESP_LOGI(TAG, "LED State: %s, nvs set %s", 
             g_led_state ? "On" : "Off", 
             err ? "Failed" : "Done");

    // NVS 设置值后，必须调用 nvs_commit()
    ESP_LOGI(TAG, "Committing updates in NVS ... ");
    err = nvs_commit(g_handle);
    ESP_LOGI(TAG, "nvs commit %s", (err != ESP_OK) ? "Failed!\n" : "Done\n");
}

/* 按键初始化操作 */
static void app_driver_button_init(void)
{
   // create gpio button
   esp_err_t err;
   button_config_t btn_cfg = {
       .type = BUTTON_TYPE_GPIO,
       .long_press_time = 5000,
       .gpio_button_config = {
           .gpio_num = EXAMPLE_BUTTON_GPIO,
           .active_level = 0,
       },
   };
   button_handle_t btn = iot_button_create(&btn_cfg);
   assert(btn);
   /* 根据不同的按键事件注册回调 */
   err = iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_press_down_event_cb, NULL);

   ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    // NVS 初始化
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // LED 初始化
    app_driver_led_init();

    // Button 初始化
    app_driver_button_init();

    // NVS 打开句柄
    ESP_LOGI(TAG, "Opening Non-Volatile Storage (NVS) handle...");
    err = nvs_open("storage", NVS_READWRITE, &g_handle);
    if (err != ESP_OK) 
    {
        ESP_LOGI(TAG, "Error (%s) opening NVS handle!\n", esp_err_to_name(err));
    } 
    else 
    {
        ESP_LOGI(TAG, "nvs open Done");
        // NVS 读操作
        ESP_LOGI(TAG, "Reading LED State from NVS ...");
        err = nvs_get_u8(g_handle, "led_state", &g_led_state);
        switch (err)
        {
        case ESP_OK:
            ESP_LOGI(TAG, "nvs get Done");
            ESP_LOGI(TAG, "led_state = %" PRIu8 "", g_led_state);
            gpio_set_level(EXAMPLE_LED_GPIO, g_led_state);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            ESP_LOGI(TAG, "The value is not initialized yet!");
            break;
        default:
            ESP_LOGI(TAG, "Error (%s) reading!", esp_err_to_name(err));
        }
    }

}
