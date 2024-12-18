#ifndef __CHANNEL_H__
#define __CHANNEL_H__

// include
#include <inttypes.h> 
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif


// type
typedef enum
{
    CHANNEL_1 = 0,
    CHANNEL_2,
    CHANNEL_3,
    CHANNEL_4,
    CHANNEL_5,
    CHANNEL_6,
    CHANNEL_7,
    CHANNEL_8,
    CHANNEL_9,
    CHANNEL_10,
    CHANNEL_11,
    CHANNEL_12,
    CHANNEL_13,
    CHANNEL_14,
}channel_serial_t;

typedef struct
{
    float coefficient; // 信道拥堵系数
    uint32_t ap_sum; // 信道上所有AP的数量
    int32_t rssi_sum; // 信道上所有AP的RSSI之和
} ap_channel_t;

// function
extern int8_t wifi_scan_get_channel_info(wifi_ap_record_t *ap, 
                                         uint16_t ap_count,
                                         ap_channel_t *ap_channel);

extern int8_t wifi_scan_print_channel_info(ap_channel_t *ap_channel);

void wifi_channel_test(void);

#ifdef __cplusplus
}
#endif

#endif