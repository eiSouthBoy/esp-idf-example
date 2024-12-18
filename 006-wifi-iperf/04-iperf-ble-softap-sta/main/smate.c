#include "smate.h"



bool is_support_device(uint8_t *data, uint8_t len)
{
    return ((len == 31) && (data[7] == ADV_DATA_FIXED_7)
            && (data[8] == ADV_DATA_FIXED_8)
            && (data[9] == ADV_DATA_FIXED_9)
            && (data[10] == ADV_DATA_FIXED_10)
            && (data[11] == ADV_DATA_FIXED_11)
            && (data[12] == ADV_DATA_FIXED_12));
}

void create_adv_data_str(uint8_t *data, uint8_t len, char adv_str[128])
{
    for (int i = 0; i < len; i++)
    {
        char chunk[4] = {0};
        sprintf(chunk, "%02x ", data[i]);
        strcat(adv_str, chunk);
    }
    adv_str[128 - 1] = '\0';
}

void ba2str(esp_bd_addr_t bda, char bdaddr[18])
{
    for (int i = 0; i < 6; i++)
    {
        char chunk[4] = {0};
        if (i == 5)
            sprintf(chunk, "%02x", bda[i]);
        else
            sprintf(chunk, "%02x:", bda[i]);
        strcat(bdaddr, chunk);
    }
    bdaddr[18 - 1] = '\0';
}

void get_ble_event_type(uint8_t evt, char evt_type[64])
{
    switch (evt)
    {
    case 0x00:
        strcpy(evt_type, "ESP_BLE_EVT_CONN_ADV");
        break;
    case 0x01:
        strcpy(evt_type, "ESP_BLE_EVT_CONN_DIR_ADV");
        break;
    case 0x02:
        strcpy(evt_type, "ESP_BLE_EVT_DISC_ADV");
        break;
    case 0x03:
        strcpy(evt_type, "ESP_BLE_EVT_NON_CONN_ADV");
        break;
    case 0x04:
        strcpy(evt_type, "ESP_BLE_EVT_SCAN_RSP");
        break;
    default:
        strcpy(evt_type, "Unknown Event Type");
        break;
    }
}
