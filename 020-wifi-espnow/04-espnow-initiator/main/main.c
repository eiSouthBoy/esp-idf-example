/* Control Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <string.h>

#include "esp_log.h"
#include "esp_system.h"

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(4, 4, 0)
#include "esp_mac.h"
#endif

#include "esp_wifi.h"

#include "espnow.h"
#include "espnow_ctrl.h"
#include "espnow_utils.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
#include "driver/rmt.h"
#endif

#include "led_strip.h"
#include "iot_button.h"

/* 对于不同的乐鑫官方开发板，按键和LED的GPIO可能不一样，
 * 或者使用其他第三方开发板时，需要自定义的GPIO
 */ 
#ifdef CONFIG_IDF_TARGET_ESP32C2
    #define CONTROL_KEY_GPIO GPIO_NUM_9
    #define LED_RED_GPIO GPIO_NUM_0
    #define LED_GREEN_GPIO GPIO_NUM_1
    #define LED_BLUE_GPIO GPIO_NUM_8
#elif CONFIG_IDF_TARGET_ESP32C3
    #define CONTROL_KEY_GPIO GPIO_NUM_9
    #define LED_STRIP_GPIO GPIO_NUM_8
#elif CONFIG_IDF_TARGET_ESP32
    #define CONTROL_KEY_GPIO GPIO_NUM_0
    /* 使用ESP32-DevKitC上的GPIO4作为LED引脚 */
    #define LED_GPIO GPIO_NUM_4 
#elif CONFIG_IDF_TARGET_ESP32S2
    #define CONTROL_KEY_GPIO GPIO_NUM_0
    #define LED_STRIP_GPIO GPIO_NUM_18
#elif CONFIG_IDF_TARGET_ESP32S3
    #define CONTROL_KEY_GPIO GPIO_NUM_0
    #define LED_STRIP_GPIO GPIO_NUM_38
#elif CONFIG_IDF_TARGET_ESP32C6
    #define CONTROL_KEY_GPIO GPIO_NUM_9
    #define LED_STRIP_GPIO GPIO_NUM_8
#endif

static const char *TAG = "app_main";

typedef enum
{
    APP_ESPNOW_CTRL_INIT,
    APP_ESPNOW_CTRL_BOUND,
    APP_ESPNOW_CTRL_MAX
} app_espnow_ctrl_status_t;

/* espnow 控制状态标志 */
static app_espnow_ctrl_status_t s_espnow_ctrl_status = APP_ESPNOW_CTRL_INIT;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
// ESP32C2-DevKit Board uses RGB LED
#if !CONFIG_IDF_TARGET_ESP32C2
static led_strip_handle_t g_strip_handle = NULL;
#endif
#else
static led_strip_t *g_strip_handle = NULL;
#endif

#if CONFIG_IDF_TARGET_ESP32
    uint8_t g_led_state = 0;
#endif

static char *bind_error_to_string(espnow_ctrl_bind_error_t bind_error)
{
    char *err_str = NULL;
    switch (bind_error)
    {
    case ESPNOW_BIND_ERROR_NONE:
        err_str = "No error";
        break;
    case ESPNOW_BIND_ERROR_TIMEOUT:
        err_str = "bind timeout";
        break;
    case ESPNOW_BIND_ERROR_RSSI:
        err_str = "bind packet RSSI below expected threshold";
        break;
    case ESPNOW_BIND_ERROR_LIST_FULL:
        err_str = "bindlist is full";
        break;
    default:
        err_str = "bindlist is full";
        break;
    }
    return err_str;
}

static void app_wifi_init()
{
    // 创建默认事件循环
    esp_event_loop_create_default();

    // WiFi初始化
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // WiFi工作模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // WiFi配置存储位置（设置RAM，掉电不保存）
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    // WiFi省电类型
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    // WiFi 启动
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void app_led_init(void)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#if CONFIG_IDF_TARGET_ESP32C2
    gpio_reset_pin(LED_RED_GPIO);
    gpio_reset_pin(LED_GREEN_GPIO);
    gpio_reset_pin(LED_BLUE_GPIO);

    /* Set the GPIO as a push/pull output */
    gpio_set_direction(LED_RED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_GREEN_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED_BLUE_GPIO, GPIO_MODE_OUTPUT);

    gpio_set_level(LED_RED_GPIO, 1);
    gpio_set_level(LED_GREEN_GPIO, 1);
    gpio_set_level(LED_BLUE_GPIO, 1);
#elif CONFIG_IDF_TARGET_ESP32
    gpio_reset_pin(LED_GPIO);
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_GPIO, g_led_state);
#else
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = 1, // at least one LED on board
    };
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &g_strip_handle));
    /* Set all LED off to clear all pixels */
    led_strip_clear(g_strip_handle);
#endif
#else
    g_strip_handle = led_strip_init(RMT_CHANNEL_0, LED_STRIP_GPIO, 1);
#endif
}

void app_led_set_color(uint8_t red, uint8_t green, uint8_t blue)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#if CONFIG_IDF_TARGET_ESP32C2
    gpio_set_level(LED_RED_GPIO, red > 0 ? 0 : 1);
    gpio_set_level(LED_GREEN_GPIO, green > 0 ? 0 : 1);
    gpio_set_level(LED_BLUE_GPIO, blue > 0 ? 0 : 1);
#elif CONFIG_IDF_TARGET_ESP32
    g_led_state = !g_led_state;
    gpio_set_level(LED_GPIO, g_led_state);
#else
    led_strip_set_pixel(g_strip_handle, 0, red, green, blue);
    led_strip_refresh(g_strip_handle);
#endif
#else
    g_strip_handle->set_pixel(g_strip_handle, 0, red, green, blue);
    g_strip_handle->refresh(g_strip_handle, 100);
#endif
}

static void app_initiator_send_press_cb(void *arg, void *usr_data)
{
    static bool status = 0; // 静态本地变量

    ESP_ERROR_CHECK(!(BUTTON_SINGLE_CLICK == iot_button_get_event(arg)));

    if (s_espnow_ctrl_status == APP_ESPNOW_CTRL_BOUND)
    {
        // 若已绑定其他设备，单击按键行为：发送ESPNOW "广播（即目的MAC地址是 ff:ff:ff:ff:ff:ff") 控制数据帧
        // ESPNOW_ATTRIBUTE_KEY_1 是 initiator 的属性
        // ESPNOW_ATTRIBUTE_POWER 是 responder 的属性
        // status 是 initiator 发给 responder 的值
        ESP_LOGI(TAG, "initiator send press");
        espnow_ctrl_initiator_send(ESPNOW_ATTRIBUTE_KEY_1, ESPNOW_ATTRIBUTE_POWER, status);
        status = !status;
    }
    else
    {
        // 若未绑定其他设备，提示用户先双击按键
        ESP_LOGI(TAG, "please double click to bind the devices firstly");
    }
}

static void app_initiator_bind_press_cb(void *arg, void *usr_data)
{
    ESP_ERROR_CHECK(!(BUTTON_DOUBLE_CLICK == iot_button_get_event(arg)));

    if (s_espnow_ctrl_status == APP_ESPNOW_CTRL_INIT)
    {
        // 若未绑定其他设备，则执行绑定行为。发送ESPNOW广播绑定帧
        ESP_LOGI(TAG, "initiator bind press");
        espnow_ctrl_initiator_bind(ESPNOW_ATTRIBUTE_KEY_1, true);
        s_espnow_ctrl_status = APP_ESPNOW_CTRL_BOUND;
    }
    else
    {
        // 若已绑定，则提示用户
        ESP_LOGI(TAG, "this device is already in bound status");
    }
}

static void app_initiator_unbind_press_cb(void *arg, void *usr_data)
{
    ESP_ERROR_CHECK(!(BUTTON_LONG_PRESS_START == iot_button_get_event(arg)));

    if (s_espnow_ctrl_status == APP_ESPNOW_CTRL_BOUND)
    {
        // 若已绑定其他设备，则对设备进行解绑。发送ESPNOW广播解绑帧
        ESP_LOGI(TAG, "initiator unbind press");
        espnow_ctrl_initiator_bind(ESPNOW_ATTRIBUTE_KEY_1, false);
        s_espnow_ctrl_status = APP_ESPNOW_CTRL_INIT;
    }
    else
    {
        // 若未绑定设备，则提示用户当前设备没有绑定其他设备（即ESPNOW响应者）
        ESP_LOGI(TAG, "this device is not been bound");
    }
}

/* 按键和LED灯 初始化操作 */
static void app_driver_init(void)
{
    // LED灯初始化
    app_led_init();

    // 创建button实例
    button_config_t button_config = {
        .type = BUTTON_TYPE_GPIO,
        .gpio_button_config = {
            .gpio_num = CONTROL_KEY_GPIO, // 按键GPIO引脚
            .active_level = 0,
        },
    };
    button_handle_t button_handle = iot_button_create(&button_config);

    // 注册按键单击事件回调
    iot_button_register_cb(button_handle, 
                           BUTTON_SINGLE_CLICK, 
                           app_initiator_send_press_cb, 
                           NULL);
    // 注册双击事件回调
    iot_button_register_cb(button_handle, 
                           BUTTON_DOUBLE_CLICK, 
                           app_initiator_bind_press_cb, 
                           NULL);
    // 注册长按事件回调
    iot_button_register_cb(button_handle, 
                           BUTTON_LONG_PRESS_START, 
                           app_initiator_unbind_press_cb, 
                           NULL);
}

static void app_responder_ctrl_data_cb(espnow_attribute_t initiator_attribute,
                                       espnow_attribute_t responder_attribute,
                                       uint32_t status)
{
    ESP_LOGI(TAG, "app_responder_ctrl_data_cb, initiator_attribute: %d, "
            "responder_attribute: %d, value: %" PRIu32 "",
             initiator_attribute, responder_attribute, status);
    
    // 根据接收的 ESPNOW 数据，控制LED的状态（ON/OFF）
    if (status)
    {
        app_led_set_color(255, 255, 255);
    }
    else
    {
        app_led_set_color(0, 0, 0);
    }
}

static void app_responder_init(void)
{
    // responder创建一个Task用来处理接收 bind 帧，包括绑定和解绑
    // 等待绑定数据帧的超时时间为 30s, 接收绑定数据帧的RSSI需要大于 -55dBm
    ESP_ERROR_CHECK(espnow_ctrl_responder_bind(30 * 1000, -55, NULL));

    // 注册回调函数，对于接收ESPNOW数据的处理
    espnow_ctrl_responder_data(app_responder_ctrl_data_cb);
}

static void app_espnow_event_handler(void *handler_args, 
                                     esp_event_base_t base, 
                                     int32_t id, 
                                     void *event_data)
{
    // 判断事件类型是否为 ESP_EVENT_ESPNOW
    if (base != ESP_EVENT_ESPNOW)
    {
        return;
    }

    espnow_ctrl_bind_info_t *info = NULL;
    switch (id)
    {
    case ESP_EVENT_ESPNOW_CTRL_BIND: // 接收到ESPNOW数据，成功绑定对方设备
        info = (espnow_ctrl_bind_info_t *)event_data;
        ESP_LOGI(TAG, "bind, uuid: " MACSTR ", initiator_type: %d", 
                 MAC2STR(info->mac), info->initiator_attribute);
        // 绿灯提示
        app_led_set_color(0, 255, 0); 
        break;
    case ESP_EVENT_ESPNOW_CTRL_BIND_ERROR: // 绑定失败，提示原因
        espnow_ctrl_bind_error_t *bind_error = (espnow_ctrl_bind_error_t *)event_data;
        ESP_LOGW(TAG, "bind error: %s", bind_error_to_string(*bind_error));
        break;
    case ESP_EVENT_ESPNOW_CTRL_UNBIND: // 接收到ESPNOW数据，解绑对方设备
        info = (espnow_ctrl_bind_info_t *)event_data;
        ESP_LOGI(TAG, "unbind, uuid: " MACSTR ", initiator_type: %d", 
                 MAC2STR(info->mac), info->initiator_attribute);
        // 红灯提示
        app_led_set_color(255, 0, 0); 
        break;
    default:
        break;
    }
}

void app_main(void)
{
    // NVS 初始化
    espnow_storage_init();

    // WiFi 初始化
    app_wifi_init();
    // Button 和 LED 初始化
    app_driver_init();

    // ESPNOW 初始化
    espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();
    espnow_init(&espnow_config);

    // 注册响ESPNOW回调函数（响应者）
    esp_event_handler_register(ESP_EVENT_ESPNOW,
                               ESP_EVENT_ANY_ID,
                               app_espnow_event_handler,
                               NULL);
    // ESPNOW响应者初始化
    app_responder_init();
}