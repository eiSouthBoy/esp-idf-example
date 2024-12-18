#include <stdio.h>
#include <math.h>
#include "esp_log.h"
#include "channel.h"

#define WIFI_MAX_CHANNEL 14

// 边带瓣影响频宽
#ifdef CONFIG_WIFI_SIDEBAND_WIDTH
#define WIFI_SIDEBAND_WIDTH CONFIG_WIFI_SIDEBAND_WIDTH
#else
#define WIFI_SIDEBAND_WIDTH 0
#endif

#define ZIGBEE_MAX_CHANNEL 16 // Zigbee通道数量

static const char *TAG = "Channel Info";

/* 不同信道边带瓣影响 */
static void wifi_channel_sideband_effect(channel_serial_t channel)
{
    uint16_t w_channel_middle = 2412 + channel * 5;
    uint16_t w_curr_channel_start = w_channel_middle - 10 - WIFI_SIDEBAND_WIDTH;
    uint16_t w_curr_channel_end = w_channel_middle + 10 + WIFI_SIDEBAND_WIDTH;
    ESP_LOGI(TAG, "wifi channel %d, %d, %u %u", channel + 1, 
        w_channel_middle, w_curr_channel_start, w_curr_channel_end);

    uint8_t cnt = 0;
    uint8_t valid[ZIGBEE_MAX_CHANNEL] = {0};
    for (uint8_t i = 0; i < ZIGBEE_MAX_CHANNEL; i++)
    {
        uint16_t z_channel_middle = 2405 + i * 5;
        float z_channel_start = z_channel_middle - 2.5;
        float z_channel_end = z_channel_middle + 2.5;
        if (z_channel_start > w_curr_channel_end || z_channel_end < w_curr_channel_start)
        {
            valid[cnt++] = i + 11; // zigbee 在ISM频段是从 11 信道开始 
        }
    }

    for (int i = 0; i < cnt; i++)
    {
        ESP_LOGI(TAG, "Zigbee Channel %"PRIu8"", valid[i]);
    }
    
}

void wifi_channel_test(void)
{
    for (int i = 0; i < WIFI_MAX_CHANNEL; i++)
    {
        ESP_LOGI(TAG, "Wi-Fi %d Channel, No effect zigbee channel:", i + 1);
        wifi_channel_sideband_effect(i);
        ESP_LOGI(TAG, "-----------------------------");
    }
}

static int8_t wifi_scan_calculate_channel_info(wifi_ap_record_t *ap_info, ap_channel_t *ap_channel)
{
    if (NULL == ap_info)
    {
        return -1;
    }
    if (NULL == ap_channel)
    {
        return -2;
    }
    if (ap_info->primary > CHANNEL_14)
    {
        return -3;
    }
    uint8_t index = ap_info->primary - 1;
    ap_channel[index].rssi_sum += ap_info->rssi;
    ap_channel[index].ap_sum++;

    return 0;
}

int8_t wifi_scan_print_channel_info(ap_channel_t *ap_channel)
{
    if (NULL == ap_channel)
    {
        return -1;
    }

    ESP_LOGI(TAG, "All Channel Info: ");
    for (int i = 0; i < CHANNEL_14; i++)
    {
        ESP_LOGI(TAG, "Channel %d: ap_sum: %" PRIu32 ", rssi_sum: %" PRId32 ", coefficient: %.2f", 
                 i+1, ap_channel[i].ap_sum, ap_channel[i].rssi_sum, ap_channel[i].coefficient);
    }

    return 0;
}

int8_t wifi_scan_calculate_channel_coefficient(ap_channel_t *ap_channel)
{
    if (NULL == ap_channel)
    {
        return -1;
    }

    for (int i = 0; i < CHANNEL_14; i++)
    {
        // 间隔0个信道
        int8_t n_left0 = i - 1;
        int8_t n_right0 = i + 1;
        // 间隔1个信道
        int8_t n_left1 = i - 2;
        int8_t n_right1 = i + 2;

        // 信道拥堵指数
        float c_left = 0;
        float c_left0 = 0;
        float c_right0 = 0;
        float c_left1 = 0;
        float c_right1 = 0;
        c_left = 200 * ap_channel[i].ap_sum + 0.2 * abs(ap_channel[i].rssi_sum);
        if (n_left0 > 0 && n_left0 < CHANNEL_14 - 1)
            c_left0 = 0.5 * (200 * ap_channel[n_left0].ap_sum + 0.2 * abs(ap_channel[n_left0].rssi_sum));
        if (n_right0 > 0 && n_right0 < CHANNEL_14 - 1)
            c_right0 = 0.5 * (200 * ap_channel[n_right0].ap_sum + 0.2 * abs(ap_channel[n_right0].rssi_sum));
        
        if (n_left1 > 0 && n_left1 < CHANNEL_14 - 1)
            c_left1 = 0.25 * (200 * ap_channel[n_left1].ap_sum + 0.2 * abs(ap_channel[n_left1].rssi_sum));
        if (n_right1 > 0 && n_right1 < CHANNEL_14 - 1)
            c_right1 = 0.25 * (200 * ap_channel[n_right1].ap_sum + 0.2 * abs(ap_channel[n_right1].rssi_sum));
        
        ap_channel[i].coefficient = c_left + c_left0 + c_right0 + c_left1 + c_right1;

    }

    return 0;
}

int8_t wifi_scan_get_channel_info(wifi_ap_record_t *ap_info, uint16_t ap_count, ap_channel_t *ap_channel)
{
    if (NULL == ap_info || 0 == ap_count)
    {
        return -1;
    }
    if (NULL == ap_channel)
    {
        return -2;
    }

    for (int i = 0; i < ap_count; i++)
    {
        wifi_scan_calculate_channel_info(&ap_info[i], ap_channel);
    }
    wifi_scan_calculate_channel_coefficient(ap_channel);

    return 0;
}

