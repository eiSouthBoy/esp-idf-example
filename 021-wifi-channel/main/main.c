#include <string.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "channel.h"

// #define DEFAULT_SCAN_LIST_SIZE CONFIG_EXAMPLE_SCAN_LIST_SIZE
#define DEFAULT_SCAN_LIST_SIZE 40
#define WIFI_MAX_CNANNEL 13
#define ZIGBEE_MAX_CHANNEL 16

static const char *TAG = "scan";

// static uint16_t wifi_middle_freq[WIFI_MAX_CNANNEL] = {2412, 2417, 2422, 2427, 
//     2432, 2442, 2447, 2452, 2457, 2462, 2467, 2472, 2484
// };

// static uint16_t zigbee_middle_freq[ZIGBEE_MAX_CHANNEL] = {2405, 2410, 

// };

static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode: WIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_OWE:
        ESP_LOGI(TAG, "Authmode: WIFI_AUTH_OWE");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode: WIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode: WIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode: WIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode: WIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode: WIFI_AUTH_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode: WIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode: WIFI_AUTH_WPA2_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA3_ENT_192:
        ESP_LOGI(TAG, "Authmode: WIFI_AUTH_WPA3_ENT_192");
        break;
    default:
        ESP_LOGI(TAG, "Authmode: WIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Pairwise Cipher: WIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Pairwise Cipher: WIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Pairwise Cipher: WIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Pairwise Cipher: WIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher: WIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher: WIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    case WIFI_CIPHER_TYPE_AES_CMAC128:
        ESP_LOGI(TAG, "Pairwise Cipher: WIFI_CIPHER_TYPE_AES_CMAC128");
        break;
    case WIFI_CIPHER_TYPE_SMS4:
        ESP_LOGI(TAG, "Pairwise Cipher: WIFI_CIPHER_TYPE_SMS4");
        break;
    case WIFI_CIPHER_TYPE_GCMP:
        ESP_LOGI(TAG, "Pairwise Cipher: WIFI_CIPHER_TYPE_GCMP");
        break;
    case WIFI_CIPHER_TYPE_GCMP256:
        ESP_LOGI(TAG, "Pairwise Cipher: WIFI_CIPHER_TYPE_GCMP256");
        break;
    default:
        ESP_LOGI(TAG, "Pairwise Cipher: WIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }

    switch (group_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Group Cipher: WIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Group Cipher: WIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Group Cipher: WIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Group Cipher: WIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Group Cipher: WIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Group Cipher: WIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    case WIFI_CIPHER_TYPE_SMS4:
        ESP_LOGI(TAG, "Group Cipher: WIFI_CIPHER_TYPE_SMS4");
        break;
    case WIFI_CIPHER_TYPE_GCMP:
        ESP_LOGI(TAG, "Group Cipher: WIFI_CIPHER_TYPE_GCMP");
        break;
    case WIFI_CIPHER_TYPE_GCMP256:
        ESP_LOGI(TAG, "Group Cipher: WIFI_CIPHER_TYPE_GCMP256");
        break;
    default:
        ESP_LOGI(TAG, "Group Cipher: WIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}

static void print_channel_bandwidth(wifi_second_chan_t second)
{
    switch (second)
    {
    case WIFI_SECOND_CHAN_NONE:  /**< the channel width is HT20 */
        ESP_LOGI(TAG, "Channel width: 20 MHz");
        break;
    case WIFI_SECOND_CHAN_ABOVE:     /**< the channel width is HT40 and the secondary channel is above the primary channel */
        ESP_LOGI(TAG, "Channel width: 40 MHz(above)");
        break;
    case WIFI_SECOND_CHAN_BELOW:     /**< the channel width is HT40 and the secondary channel is below the primary channel */
        ESP_LOGI(TAG, "Channel width: 40 MHz(below)");
        break;
    default:
        ESP_LOGI(TAG, "Unknown channel width");
        break;
    }

}

/* Initialize Wi-Fi as sta and set scan method */
static void wifi_scan(void)
{
    // TCP/IP 协议栈初始化
    ESP_ERROR_CHECK(esp_netif_init());

    // 创建默认的事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    // 创建 sta 接口实例
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    // WiFi 初始化
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    // 设置WiFi工作模式
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // WiFi 启动
    ESP_ERROR_CHECK(esp_wifi_start());

    // WiFi 扫描启动，阻塞等待扫描完成
    ESP_LOGI(TAG, "wifi scan start ... ");
    wifi_scan_config_t scan_cfg = {
        .show_hidden = true,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 0,
        .scan_time.active.max = 120,
        .scan_time.passive = 360,

    };
    esp_wifi_scan_start(&scan_cfg, true);


    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    uint16_t number = ap_count;
    if (ap_count > 0)
    {
        wifi_ap_record_t *ap_info = malloc(ap_count * sizeof(wifi_ap_record_t));
        if (NULL == ap_info)
        {
            ESP_LOGE(TAG, "malloc() failed");
            return;
        }
        memset(ap_info, 0, ap_count * sizeof(wifi_ap_record_t));
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
        ESP_LOGI(TAG, "Total APs scanned = %u, actual AP number ap_info holds = %u", ap_count, number);
        for (int i = 0; i < number; i++)
        {
            ESP_LOGI(TAG, "SSID: %s", ap_info[i].ssid);
            ESP_LOGI(TAG, "RSSI: %d", ap_info[i].rssi);
            ESP_LOGI(TAG, "Channel: %d", ap_info[i].primary);
            print_channel_bandwidth(ap_info[i].second);
            print_auth_mode(ap_info[i].authmode);
            if (ap_info[i].authmode != WIFI_AUTH_WEP)
            {
                print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
            }
            ESP_LOGI(TAG, "---------------------------------\n");
        }

        ap_channel_t ap_channel[CHANNEL_14] = {0};
        wifi_scan_get_channel_info(ap_info, ap_count, ap_channel);
        wifi_scan_print_channel_info(ap_channel);

        wifi_channel_test();
    }

}

void app_main(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_scan();
}
