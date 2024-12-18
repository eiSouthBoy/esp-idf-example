#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_log.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_main.h"

#include "smate.h"


static const char *TAG = "BLE_SCAN";

static void esp_gap_cb(esp_gap_ble_cb_event_t event, 
                       esp_ble_gap_cb_param_t *param);


static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30,
    .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
};

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event)
    {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: // BLE扫描参数设置完毕
    {
        ESP_LOGI(TAG, "--> ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT");
        // the unit of the duration is second
        // 开始扫描，需要传入参数：扫描时间
        uint32_t duration = 0;
        esp_ble_gap_start_scanning(duration);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT: // BLE扫描开始完毕
        ESP_LOGI(TAG, "--> ESP_GAP_BLE_SCAN_START_COMPLETE_EVT");
        // scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "scan start failed, error status = %x", param->scan_start_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "scan start success");

        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: // BLE扫描结果
    {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt)
        {
        case ESP_GAP_SEARCH_INQ_RES_EVT: // 获取扫描结果
            uint8_t *adv_data = scan_result->scan_rst.ble_adv;
            uint8_t adv_data_len = scan_result->scan_rst.adv_data_len;

            char bdaddr[18] = {0};
            uint8_t *bda = scan_result->scan_rst.bda;
            ba2str(bda, bdaddr);
            if (is_support_device(adv_data, adv_data_len))
            {
                char event_type[64] = {0};
                get_ble_event_type(scan_result->scan_rst.ble_evt_type, event_type);

                char adv_data_str[128] = {0};
                create_adv_data_str(adv_data, adv_data_len, adv_data_str);
                ESP_LOGI(TAG, "----> event_type: %s, bdaddr: %s, len: %d, data: %s", 
                         event_type, bdaddr, adv_data_len, adv_data_str);
            }

#if CONFIG_EXAMPLE_DUMP_ADV_DATA_AND_SCAN_RESP
            if (scan_result->scan_rst.adv_data_len > 0)
            {
                ESP_LOGI(TAG, "adv data:");
                esp_log_buffer_hex(TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len);
            }
            if (scan_result->scan_rst.scan_rsp_len > 0)
            {
                ESP_LOGI(TAG, "scan resp:");
                esp_log_buffer_hex(TAG, &scan_result->scan_rst.ble_adv[scan_result->scan_rst.adv_data_len], scan_result->scan_rst.scan_rsp_len);
            }
#endif
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT: // BLE扫描停止完毕
        ESP_LOGI(TAG, "--> ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT");
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "scan stop failed, error status = %x", param->scan_stop_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "stop scan successfully");
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT: // BLE广播停止完毕
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "adv stop failed, error status = %x", param->adv_stop_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "stop adv successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT: // BLE更新连接参数
        ESP_LOGI(TAG, "update connection params status = %d, min_int = %d, max_int = %d,conn_int = %d,latency = %d, timeout = %d",
                 param->update_conn_params.status,
                 param->update_conn_params.min_int,
                 param->update_conn_params.max_int,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    default:
        break;
    }
}

int ble_scan_init(void)
{
    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    // 蓝牙控制器 初始化
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret) 
    {
        ESP_LOGE(TAG, "esp_bt_controller_init() failed: %s", esp_err_to_name(ret));
        return -1;
    }

    // 选择蓝牙模式： BLE，并使能蓝牙控制器
    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret) 
    {
        ESP_LOGE(TAG, "esp_bt_controller_enable() failed: %s", esp_err_to_name(ret));
        return -2;
    }

    // bluedroid 蓝牙协议栈初始化
    ret = esp_bluedroid_init();
    if (ret) 
    {
        ESP_LOGE(TAG, "esp_bluedroid_init() failed: %s", esp_err_to_name(ret));
        return -4;
    }

    // 使能 bluedroid 蓝牙协议栈
    ret = esp_bluedroid_enable();
    if (ret) 
    {
        ESP_LOGE(TAG, "esp_bluedroid_enable() failed: %s", esp_err_to_name(ret));
        return -5;
    }

    // 注册 GAP 回调函数
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret)
    {
        ESP_LOGE(TAG, "esp_ble_gap_register_callback() failed, error code = %x", ret);
        return -6;
    }

    // 设置扫描参数
    esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (scan_ret)
    {
        ESP_LOGE(TAG, "set scan params error, error code = %x", scan_ret);
        return -7;
    }

    return 0;
}
