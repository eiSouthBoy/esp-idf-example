#include <string.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_adc/adc_continuous.h"

#define EXAMPLE_ADC_UNIT                    ADC_UNIT_1

/* C预处理器的字符串化操作符 #，它可以将宏参数转换为字符串常量。
 * 如果传递 ADC_UNIT 给 _EXAMPLE_ADC_UNIT_STR，它会生成字符串 "ADC_UNIT" */
#define _EXAMPLE_ADC_UNIT_STR(unit)         #unit
#define EXAMPLE_ADC_UNIT_STR(unit)          _EXAMPLE_ADC_UNIT_STR(unit)

#define EXAMPLE_ADC_CONV_MODE               ADC_CONV_SINGLE_UNIT_1
#define EXAMPLE_ADC_ATTEN                   ADC_ATTEN_DB_12
#define EXAMPLE_ADC_BIT_WIDTH               SOC_ADC_DIGI_MAX_BITWIDTH

/* 用于从 adc_digi_output_data_t 结构体中提取通道号和数据值 */
#define EXAMPLE_ADC_OUTPUT_TYPE             ADC_DIGI_OUTPUT_FORMAT_TYPE1
#define EXAMPLE_ADC_GET_CHANNEL(p_data)     ((p_data)->type1.channel)
#define EXAMPLE_ADC_GET_DATA(p_data)        ((p_data)->type1.data)


#define EXAMPLE_READ_LEN    256


/* ADC通道 
 * - ADC_CHANNEL_6 对应 IO34
 * - ADC_CHANNEL_7 对应 IO35
*/
// static adc_channel_t channel[2] = {ADC_CHANNEL_6, ADC_CHANNEL_7};
static adc_channel_t channel[] = {ADC_CHANNEL_6};


static TaskHandle_t s_task_handle;
static const char *TAG = "continuous_read";

// ADC连续模式的事件回调(一个转换帧完成时)
static bool IRAM_ATTR s_conv_done_cb(adc_continuous_handle_t handle, 
                                     const adc_continuous_evt_data_t *edata, 
                                     void *user_data)
{
    BaseType_t mustYield = pdFALSE;
    //Notify that ADC continuous driver has done enough number of conversions
    vTaskNotifyGiveFromISR(s_task_handle, &mustYield);

    return (mustYield == pdTRUE);
}

// ADC初始化
static void continuous_adc_init(adc_channel_t *channel, 
                                uint8_t channel_num, 
                                adc_continuous_handle_t *out_handle)
{
    adc_continuous_handle_t handle = NULL;

    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = 1024,  // 最大存储缓冲区大小
        .conv_frame_size = EXAMPLE_READ_LEN,  // 转换帧大小
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&adc_config, &handle));

    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000,  // 采样频率
        .conv_mode = EXAMPLE_ADC_CONV_MODE, // 转换模式
        .format = EXAMPLE_ADC_OUTPUT_TYPE,  // 输出格式
    };

    adc_digi_pattern_config_t adc_pattern[SOC_ADC_PATT_LEN_MAX] = {0};
    dig_cfg.pattern_num = channel_num; // 通道数量
    for (int i = 0; i < channel_num; i++) 
    {
        adc_pattern[i].atten = EXAMPLE_ADC_ATTEN;  // 衰减系数
        adc_pattern[i].channel = channel[i] & 0x7; // 通道
        adc_pattern[i].unit = EXAMPLE_ADC_UNIT;    // ADC单元
        adc_pattern[i].bit_width = EXAMPLE_ADC_BIT_WIDTH; // 位宽

        // 打印配置信息
        ESP_LOGI(TAG, "adc_pattern[%d].atten is :%"PRIx8, i, adc_pattern[i].atten);
        ESP_LOGI(TAG, "adc_pattern[%d].channel is :%"PRIx8, i, adc_pattern[i].channel);
        ESP_LOGI(TAG, "adc_pattern[%d].unit is :%"PRIx8, i, adc_pattern[i].unit);
    }
    dig_cfg.adc_pattern = adc_pattern;
    ESP_ERROR_CHECK(adc_continuous_config(handle, &dig_cfg));

    *out_handle = handle;
}

void app_main(void)
{
    esp_err_t ret;
    uint32_t ret_num = 0;
    uint8_t result[EXAMPLE_READ_LEN] = {0};
    memset(result, 0xcc, EXAMPLE_READ_LEN);

    s_task_handle = xTaskGetCurrentTaskHandle();

    // ADC初始化
    adc_continuous_handle_t handle = NULL;
    continuous_adc_init(channel, sizeof(channel) / sizeof(adc_channel_t), &handle);

    adc_continuous_evt_cbs_t cbs = {
        /* 当一个转换帧完成时，触发此事件:s_conv_done_cb */
        .on_conv_done = s_conv_done_cb,
    };

    // 注册事件回调
    ESP_ERROR_CHECK(adc_continuous_register_event_callbacks(handle, &cbs, NULL));

    // 启动ADC连续模式
    ESP_ERROR_CHECK(adc_continuous_start(handle));

    while (1) 
    {

        /**
         * This is to show you the way to use the ADC continuous mode driver event callback.
         * This `ulTaskNotifyTake` will block when the data processing in the task is fast.
         * However in this example, the data processing (print) is slow, so you barely block here.
         *
         * Without using this event callback (to notify this task), you can still just call
         * `adc_continuous_read()` here in a loop, with/without a certain block timeout.
         */
        // ulTaskNotifyTake() 等待通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        char unit[] = EXAMPLE_ADC_UNIT_STR(EXAMPLE_ADC_UNIT);

        while (1) 
        {
            // 读取ADC数据
            ret = adc_continuous_read(handle, result, EXAMPLE_READ_LEN, &ret_num, 0);
            if (ret == ESP_OK) 
            {
                ESP_LOGI("TASK", "ret is %x, ret_num is %"PRIu32" bytes", ret, ret_num);

                // 循环遍历读取到的数据，解析每个ADC数据项，并打印出来
                for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_RESULT_BYTES) 
                {
                    adc_digi_output_data_t *p = (adc_digi_output_data_t*)&result[i];
                    uint32_t chan_num = EXAMPLE_ADC_GET_CHANNEL(p);
                    uint32_t data = EXAMPLE_ADC_GET_DATA(p);
                    /* Check the channel number validation, the data is invalid 
                       if the channel num exceed the maximum channel */
                    if (chan_num < SOC_ADC_CHANNEL_NUM(EXAMPLE_ADC_UNIT)) 
                    {
                        ESP_LOGI(TAG, "Unit: %s, Channel: %"PRIu32", Value: %"PRIu32, 
                                 unit, chan_num, data);
                    } 
                    else 
                    {
                        ESP_LOGW(TAG, "Invalid data [%s_%"PRIu32"_%"PRIu32"]", unit, chan_num, data);
                    }
                }
                /**
                 * Because printing is slow, so every time you call `ulTaskNotifyTake`, 
                 * it will immediately return. To avoid a task watchdog timeout, 
                 * add a delay here. When you replace the way you process the data,
                 * usually you don't need this delay (as this task will block for a while).
                 */
                vTaskDelay(1);
            } 
            else if (ret == ESP_ERR_TIMEOUT) 
            {
                /* We try to read `EXAMPLE_READ_LEN` until API returns timeout, 
                 * which means there's no available data */
                break;
            }
        }
    }

    ESP_ERROR_CHECK(adc_continuous_stop(handle));
    ESP_ERROR_CHECK(adc_continuous_deinit(handle));
}
