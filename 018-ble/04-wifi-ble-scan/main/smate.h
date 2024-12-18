#ifndef __SMATE_H__
#define __SMATE_H__

// include
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// define
#define ADV_DATA_FIXED_7    0xee
#define ADV_DATA_FIXED_8    0x1b
#define ADV_DATA_FIXED_9    0xc8
#define ADV_DATA_FIXED_10   0x78
#define ADV_DATA_FIXED_11   0xf6
#define ADV_DATA_FIXED_12   0x4a

#define ESP_BD_ADDR_LEN     6


// struct
typedef uint8_t esp_bd_addr_t[ESP_BD_ADDR_LEN];


// function
extern void ba2str(esp_bd_addr_t bda, char bdaddr[18]);
extern bool is_support_device(uint8_t *data, uint8_t len);
extern void create_adv_data_str(uint8_t *data, uint8_t len, char adv_str[128]);
extern void get_ble_event_type(uint8_t evt, char evt_type[64]);


#ifdef __cplusplus
}
#endif

#endif