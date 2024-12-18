#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_wps.h"
#include "esp_event.h"
#include "nvs_flash.h"

/* AP 配置项 */
#define EXAMPLE_ESP_WIFI_AP_SSID    CONFIG_ESP_WIFI_AP_SSID // ESP AP的SSID
#define EXAMPLE_ESP_WIFI_AP_PASSWD  CONFIG_ESP_WIFI_AP_PASSWORD // ESP AP的密码
#define EXAMPLE_ESP_WIFI_CHANNEL    CONFIG_ESP_WIFI_AP_CHANNEL // ESP AP的信道
#define EXAMPLE_MAX_STA_CONN        CONFIG_ESP_MAX_STA_CONN_AP // ESP AP允许最大STA连接数


/* 设置 WPS 的模式 */
#if CONFIG_EXAMPLE_WPS_TYPE_PBC
    #define WPS_MODE WPS_TYPE_PBC
#elif CONFIG_EXAMPLE_WPS_TYPE_PIN
    #define WPS_MODE WPS_TYPE_PIN
#else
    #define WPS_MODE WPS_TYPE_DISABLE
#endif /*CONFIG_EXAMPLE_WPS_TYPE_PBC*/

#define MAX_RETRY_ATTEMPTS  5 // 最大重连次数

#ifndef PIN2STR
    #define PIN2STR(a) (a)[0], (a)[1], (a)[2], (a)[3], (a)[4], (a)[5], (a)[6], (a)[7]
    #define PINSTR "%c%c%c%c%c%c%c%c"
#endif

static const char *TAG = "example_wps";
static int s_retry_num = 0;
static int s_ap_creds_num = 0;
static wifi_config_t wps_ap_creds[MAX_WPS_AP_CRED];
static esp_wps_config_t config = WPS_CONFIG_INIT_DEFAULT(WPS_MODE);

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    static int ap_idx = 1;

    switch (event_id)
    {
    case WIFI_EVENT_STA_START:
        ESP_LOGI(TAG, "--> WIFI_EVENT_STA_START");
        break;
    case WIFI_EVENT_STA_DISCONNECTED:
        ESP_LOGI(TAG, "--> WIFI_EVENT_STA_DISCONNECTED");
        if (s_retry_num < MAX_RETRY_ATTEMPTS)
        {
            esp_wifi_connect();
            s_retry_num++;
        }
        else if (ap_idx < s_ap_creds_num)
        {
            /* Try the next AP credential if first one fails */

            if (ap_idx < s_ap_creds_num)
            {
                ESP_LOGI(TAG, "Connecting to SSID: %s, Passphrase: %s",
                         wps_ap_creds[ap_idx].sta.ssid, 
                         wps_ap_creds[ap_idx].sta.password);
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, 
                                &wps_ap_creds[ap_idx++]));
                
                esp_wifi_connect();
            }
            s_retry_num = 0;
        }
        else
        {
            ESP_LOGI(TAG, "Failed to connect!");
        }

        break;
    case WIFI_EVENT_STA_WPS_ER_SUCCESS:
        ESP_LOGI(TAG, "--> WIFI_EVENT_STA_WPS_ER_SUCCESS");
        {
            wifi_event_sta_wps_er_success_t *evt =
                (wifi_event_sta_wps_er_success_t *)event_data;
            int i;

            if (evt)
            {
                s_ap_creds_num = evt->ap_cred_cnt;
                for (i = 0; i < s_ap_creds_num; i++)
                {
                    memcpy(wps_ap_creds[i].sta.ssid, evt->ap_cred[i].ssid,
                           sizeof(evt->ap_cred[i].ssid));
                    memcpy(wps_ap_creds[i].sta.password, evt->ap_cred[i].passphrase,
                           sizeof(evt->ap_cred[i].passphrase));
                }
                /* If multiple AP credentials are received from WPS, connect with first one */
                ESP_LOGI(TAG, "Connecting to SSID: %s, Passphrase: %s",
                         wps_ap_creds[0].sta.ssid, wps_ap_creds[0].sta.password);
                ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wps_ap_creds[0]));
            }
            /*
             * If only one AP credential is received from WPS, there will be no event data and
             * esp_wifi_set_config() is already called by WPS modules for backward compatibility
             * with legacy apps. So directly attempt connection here.
             */
            ESP_ERROR_CHECK(esp_wifi_wps_disable());
            esp_wifi_connect();
        }
        break;
    case WIFI_EVENT_STA_WPS_ER_FAILED:
        ESP_LOGI(TAG, "--> WIFI_EVENT_STA_WPS_ER_FAILED");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));
        break;
    case WIFI_EVENT_STA_WPS_ER_TIMEOUT:
        ESP_LOGI(TAG, "--> WIFI_EVENT_STA_WPS_ER_TIMEOUT");
        ESP_ERROR_CHECK(esp_wifi_wps_disable());
        ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
        ESP_ERROR_CHECK(esp_wifi_wps_start(0));
        break;
    case WIFI_EVENT_STA_WPS_ER_PIN:
        ESP_LOGI(TAG, "--> WIFI_EVENT_STA_WPS_ER_PIN");
        /* display the PIN code */
        wifi_event_sta_wps_er_pin_t *event = (wifi_event_sta_wps_er_pin_t *)event_data;
        ESP_LOGI(TAG, "WPS_PIN = " PINSTR, PIN2STR(event->pin_code));
        break;
    default:
        break;
    }
}

static void got_ip_event_handler(void* arg, esp_event_base_t event_base,
                             int32_t event_id, void* event_data)
{
    ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
}

static esp_netif_t *wifi_init_softap(void)
{
    // 使用默认WiFi AP配置创建esp_netif对象，将netif连接到WiFi并注册默认WiFi处理程序
    esp_netif_t *esp_netif_ap = esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_ap_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_AP_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_AP_SSID),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = EXAMPLE_ESP_WIFI_AP_PASSWD,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA2_PSK, // AP 使用默认加密模式：WPA2-PSK
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_AP_PASSWD) == 0) 
    {
        // 若密码长度为0，则加密模式为 OEPN
        wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    // 设置 AP 的配置
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config));

    ESP_LOGI(TAG, "--> wifi_init_softap finished. SSID: %s, password: %s, channel: %d",
             EXAMPLE_ESP_WIFI_AP_SSID, EXAMPLE_ESP_WIFI_AP_PASSWD, EXAMPLE_ESP_WIFI_CHANNEL);

    return esp_netif_ap;
}

esp_netif_t *wifi_init_sta(void)
{
    // 使用默认WiFi STA配置创建esp_netif对象，将netif连接到WiFi并注册默认WiFi处理程序
    esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();
    ESP_LOGI(TAG, "--> wifi_init_sta finished.");

    return esp_netif_sta;    
}

static void start_wps(void)
{
    // TCP/IP 协议栈初始化
    ESP_ERROR_CHECK(esp_netif_init());
    // 创建默认的事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    //  WiFi 初始化
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));

    /* AP 初始化 */
    esp_netif_t *esp_netif_ap = wifi_init_softap();

    /* STA 初始化 */
    esp_netif_t *esp_netif_sta = wifi_init_sta();



    // 注册不同事件的回调函数
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, 
                                               ESP_EVENT_ANY_ID, 
                                               &wifi_event_handler, 
                                               NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, 
                                               IP_EVENT_STA_GOT_IP, 
                                               &got_ip_event_handler, 
                                               NULL));

    // 设置 WiFi 模式，接着启动 WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "start wps...");

    // 使能 WPS 配置
    ESP_ERROR_CHECK(esp_wifi_wps_enable(&config));
    // 启动 WPS
    ESP_ERROR_CHECK(esp_wifi_wps_start(0));
}

void app_main(void)
{
    // NVS 初始化
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    start_wps();
}