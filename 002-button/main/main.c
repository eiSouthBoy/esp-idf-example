#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/ledc.h"
#include "esp_err.h"

#include "iot_button.h"


#define BUTTON_GPIO_0 CONFIG_BUTTON_GPIO_NUM
#define LED_GPIO_4 CONFIG_LED_GPIO_NUM

static const char *TAG = "button-example";
static uint8_t g_led_state = 0;

/* Warning:
 * For ESP32, ESP32S2, ESP32S3, ESP32C3, ESP32C2, ESP32C6, ESP32H2, ESP32P4 targets,
 * when LEDC_DUTY_RES selects the maximum duty resolution (i.e. value equal to SOC_LEDC_TIMER_BIT_WIDTH),
 * 100% duty cycle is not reachable (duty cannot be set to (2 ** SOC_LEDC_TIMER_BIT_WIDTH)).
 */

#define LEDC_TIMER              LEDC_TIMER_0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE
#define LEDC_OUTPUT_IO          CONFIG_LED_GPIO_NUM // Define the output GPIO
#define LEDC_CHANNEL            LEDC_CHANNEL_0
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT // Set duty resolution to 13 bits
#define LEDC_DUTY               (4096) // Set duty to 50%. (2 ** 13) * 50% = 4096
#define LEDC_FREQUENCY          (4000) // Frequency in Hertz. Set frequency at 4 kHz


// //用于通知渐变完成
// static EventGroupHandle_t   s_ledc_ev = NULL;
// //关灯完成事件标志
// #define LEDC_OFF_EV  (1<<0)
// //开灯完成事件标志
// #define LEDC_ON_EV   (1<<1)
// //渐变完成回调函数
// bool IRAM_ATTR ledc_finish_cb(const ledc_cb_param_t *param, void *user_arg)
// {
//     BaseType_t xHigherPriorityTaskWoken;
//     if (param->duty)
//     {
//         xEventGroupSetBitsFromISR(s_ledc_ev, LEDC_ON_EV, &xHigherPriorityTaskWoken);
//     }
//     else
//     {
//         xEventGroupSetBitsFromISR(s_ledc_ev, LEDC_OFF_EV, &xHigherPriorityTaskWoken);
//     }
//     return xHigherPriorityTaskWoken;
// }

// // ledc 渐变任务
// void ledc_breath_task(void *param)
// {
//     EventBits_t ev;
//     while (1)
//     {
//         ev = xEventGroupWaitBits(s_ledc_ev, LEDC_ON_EV | LEDC_OFF_EV, pdTRUE, pdFALSE, pdMS_TO_TICKS(5000));
//         if (ev)
//         {
//             // 设置LEDC开灯渐变
//             if (ev & LEDC_OFF_EV)
//             {
//                 ledc_set_fade_with_time(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY, 2000);
//                 ledc_fade_start(LEDC_MODE, LEDC_CHANNEL, LEDC_FADE_NO_WAIT);
//             }
//             else if (ev & LEDC_ON_EV) // 设置LEDC关灯渐变
//             {
//                 ledc_set_fade_with_time(LEDC_MODE, LEDC_CHANNEL, 0, 2000);
//                 ledc_fade_start(LEDC_MODE, LEDC_CHANNEL, LEDC_FADE_NO_WAIT);
//             }
//             // 再次设置回调函数
//             ledc_cbs_t cbs = {
//                 .fade_cb = ledc_finish_cb,
//             };
//             ledc_cb_register(LEDC_MODE, LEDC_CHANNEL, &cbs, NULL);
//         }
//     }
// }

static void example_ledc_init(void)
{
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,  // Set output frequency at 4 kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = LEDC_OUTPUT_IO,
        .duty           = 0, // Set duty to 0%
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    // 开启硬件PWM
    ledc_fade_func_install(0);
    // 创建一个事件组，用于通知任务渐变完成
    // s_ledc_ev = xEventGroupCreate();
    // // 配置LEDC渐变
    // ledc_set_fade_with_time(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY, 2000);
    // // 启动渐变
    // ledc_fade_start(LEDC_MODE, LEDC_CHANNEL, LEDC_FADE_NO_WAIT);

    // ledc_set_fade_with_time(LEDC_MODE, LEDC_CHANNEL, 0, 2000);
    // ledc_fade_start(LEDC_MODE, LEDC_CHANNEL, LEDC_FADE_NO_WAIT);
    // 设置渐变完成回调函数
    // ledc_cbs_t cbs = {
    //     .fade_cb = ledc_finish_cb,
    // };
    // ledc_cb_register(LEDC_MODE, LEDC_CHANNEL, &cbs, NULL);
    // xTaskCreate(ledc_breath_task, "ledc", 2048, NULL, 3, NULL);
}

// static void app_driver_led_init(void)
// {
//     ESP_LOGI(TAG, "Example configured to GPIO LED(GPIO %d)!", LED_GPIO_4);
//     gpio_reset_pin(LED_GPIO_4); // 重启GPIO_4的电平状态
//     gpio_set_direction(LED_GPIO_4, GPIO_MODE_OUTPUT); // 选择输出模式
//     gpio_set_level(LED_GPIO_4, 0);  // 设置 GPIO_4 为低电平（ESP32启动时，LED为关闭状态）
// }

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

/* 单击按键事件（BUTTON_PRESS_DOWN）回调函数 */
static void button_single_click_event_cb(void *arg, void *data)
{
    iot_button_print_event((button_handle_t)arg);
   
    ESP_LOGI(TAG, "the LED blink once!");

    
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
    vTaskDelay(pdMS_TO_TICKS(500));

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 8192));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
    vTaskDelay(pdMS_TO_TICKS(500));
}

/* 双击按键事件 */
static void button_double_click_event_cb(void *arg, void *data)
{
    iot_button_print_event((button_handle_t)arg);
   
    g_led_state = !g_led_state;
    ESP_LOGI(TAG, "the LED blink twice!");

    // app_driver_led_init();
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    for (int i = 0; i < 2; i++)
    {
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
        vTaskDelay(pdMS_TO_TICKS(160));

        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 8192));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
        vTaskDelay(pdMS_TO_TICKS(160));
    }
}

/* 长按开始按键事件（BUTTON_LONG_PRESS_START）回调函数 */
static void button_long_press_start_event_cb(void *arg, void *data)
{
    iot_button_print_event((button_handle_t)arg);
   
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));

    // 开灯渐变，有灭亮
    ledc_set_fade_with_time(LEDC_MODE, LEDC_CHANNEL, 0, 2000);
    ledc_fade_start(LEDC_MODE, LEDC_CHANNEL, LEDC_FADE_NO_WAIT);

    // 关灯渐变，由亮到灭
    ledc_set_fade_with_time(LEDC_MODE, LEDC_CHANNEL, 8192, 2000);
    ledc_fade_start(LEDC_MODE, LEDC_CHANNEL, LEDC_FADE_NO_WAIT);


    ledc_set_fade_with_time(LEDC_MODE, LEDC_CHANNEL, 0, 2000);
    ledc_fade_start(LEDC_MODE, LEDC_CHANNEL, LEDC_FADE_NO_WAIT);
}

/* 按键初始化操作 */
static void app_driver_button_init(uint32_t button_num)
{
    // create gpio button
    esp_err_t err;
    button_config_t btn_cfg = {
        .type = BUTTON_TYPE_GPIO,
        .long_press_time = 5000,
        .gpio_button_config = {
            .gpio_num = button_num,
            .active_level = 0,
        },
    };
    button_handle_t btn = iot_button_create(&btn_cfg);
    assert(btn);
    /* 根据不同的按键事件注册回调 */
    err = iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_single_click_event_cb, NULL);
    err |= iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, button_double_click_event_cb, NULL);
    err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_long_press_start_event_cb, NULL);

    /* 自定义其他按键事件的回调实现 */
    // err |= iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_press_down_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_START, button_long_press_start_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_PRESS_UP, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_PRESS_REPEAT_DONE, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_SINGLE_CLICK, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_DOUBLE_CLICK, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_HOLD, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_LONG_PRESS_UP, button_event_cb, NULL);
    // err |= iot_button_register_cb(btn, BUTTON_PRESS_END, button_event_cb, NULL);

    ESP_ERROR_CHECK(err);
}



void app_main(void)
{
    // app_driver_led_init();
    example_ledc_init();
    app_driver_button_init(BUTTON_GPIO_0);

    for (int i = 0; i < 5; i++)
    {
        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 8192));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
        vTaskDelay(pdMS_TO_TICKS(200));

        ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0));
        ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
