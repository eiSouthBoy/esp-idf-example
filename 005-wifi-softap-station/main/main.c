#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_netif_net_stack.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#if IP_NAPT
    #include "lwip/lwip_napt.h"
#endif

/* STA 配置项 */
#define EXAMPLE_ESP_WIFI_STA_SSID   CONFIG_ESP_WIFI_REMOTE_AP_SSID // Remote AP的SSID
#define EXAMPLE_ESP_WIFI_STA_PASSWD CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD // Remote AP的密码
#define EXAMPLE_ESP_MAXIMUM_RETRY   CONFIG_ESP_MAXIMUM_STA_RETRY // STA 最大重连次数

/* 加密模式 */
#if CONFIG_ESP_WIFI_AUTH_OPEN
    #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
    #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
    #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
    #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
    #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
    #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
    #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
    #define ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WAPI_PSK
#endif

/* AP 配置项 */
#define EXAMPLE_ESP_WIFI_AP_SSID    CONFIG_ESP_WIFI_AP_SSID // ESP AP的SSID
#define EXAMPLE_ESP_WIFI_AP_PASSWD  CONFIG_ESP_WIFI_AP_PASSWORD // ESP AP的密码
#define EXAMPLE_ESP_WIFI_CHANNEL    CONFIG_ESP_WIFI_AP_CHANNEL // ESP AP的信道
#define EXAMPLE_MAX_STA_CONN        CONFIG_ESP_MAX_STA_CONN_AP // ESP AP允许最大STA连接数


/* 事件群组允许每个事件使用多个bit, 但是我们仅关注两个事件： 
   - 当连接到AP，并分配到IP地址
   - 当重连最大次数后，还是连接失败
*/
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "SoftAP-STA";
static const char *TAG_AP = "SoftAP";
static const char *TAG_STA = "STA";

static int g_retry_num = 0; // 重试次数计数器

/* 当我们已连接/已断开时，FreeRTOS事件群组会发送信号 */
static EventGroupHandle_t g_wifi_event_group;

static void wifi_event_handler(void *arg, 
                               esp_event_base_t event_base,
                               int32_t event_id, 
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        // STA 成功连接 ESP32 AP
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI("WIFI_EVENT", "WIFI_EVENT_AP_STACONNECTED, Station " MACSTR " joined, AID=%d", 
                MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        // STA 断开连接 ESP32 AP
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI("WIFI_EVENT", "WIFI_EVENT_AP_STADISCONNECTED, Station " MACSTR " left, AID=%d", 
                MAC2STR(event->mac), event->aid);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {   
        // WiFi 初始化完成，准备连接目标WiFi
        ESP_LOGI("WIFI_EVENT", "WIFI_EVENT_STA_START, connect target wifi");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        // STA 已连接成功目标AP
        ESP_LOGI("WIFI_EVENT", "WIFI_EVENT_STA_CONNECTED");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // DHCP Client成功获取IPv4地址
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI("IP_EVENT", "IP_EVENT_STA_GOT_IP " IPSTR, IP2STR(&event->ip_info.ip));
        g_retry_num = 0; // 重连计数器置零

        // STA 连接成功并获取IP，修改g_wifi_event_group相关的位
        xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_netif_t *wifi_init_softap(void)
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

    ESP_LOGI(TAG_AP, "--> wifi_init_softap finished. SSID: %s, password: %s, channel: %d",
             EXAMPLE_ESP_WIFI_AP_SSID, EXAMPLE_ESP_WIFI_AP_PASSWD, EXAMPLE_ESP_WIFI_CHANNEL);

    return esp_netif_ap;
}

esp_netif_t *wifi_init_sta(void)
{
    // 使用默认WiFi STA配置创建esp_netif对象，将netif连接到WiFi并注册默认WiFi处理程序
    esp_netif_t *esp_netif_sta = esp_netif_create_default_wifi_sta();

    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_STA_SSID,
            .password = EXAMPLE_ESP_WIFI_STA_PASSWD,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .failure_retry_cnt = EXAMPLE_ESP_MAXIMUM_RETRY,
            .threshold.authmode = ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH, 
        },
    };

    // 设置 STA 的配置
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config) );

    ESP_LOGI(TAG_STA, "--> wifi_init_sta finished.");

    return esp_netif_sta;    
}

void app_main(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // NVS 初始化，因为 WiFi 需要使用 NVS 
    ESP_LOGI(TAG, "nvs flash init");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW(TAG, "nvs flash erase and init");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 初始化事件群组
    g_wifi_event_group = xEventGroupCreate();

    // 监听 WIFI_EVENT 所有的事件
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    // 监听 IP_EVENT 的 IP_EVENT_STA_GOT_IP 事件
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
    /* WiFi 初始化 */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA)); // SoftAP + STA 共存模式

    /* AP 初始化 */
    ESP_LOGI(TAG, "ESP WIFI MODE: AP");
    esp_netif_t *esp_netif_ap = wifi_init_softap();

    /* STA 初始化 */
    ESP_LOGI(TAG, "ESP WIFI MODE: STA");
    esp_netif_t *esp_netif_sta = wifi_init_sta();

    /* WiFi 启动 */
    ESP_ERROR_CHECK(esp_wifi_start());


    /* 等待 “连接建立”或 “连接失败” ，任务被阻塞在此处*/
    EventBits_t bits = xEventGroupWaitBits(g_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    /* 测试返回事件bits */
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG_STA, "connected to ap SSID: %s, password: %s",
                 EXAMPLE_ESP_WIFI_STA_SSID, EXAMPLE_ESP_WIFI_STA_PASSWD);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG_STA, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_STA_SSID, EXAMPLE_ESP_WIFI_STA_PASSWD);
    }
    else
    {
        ESP_LOGE(TAG_STA, "UNEXPECTED EVENT");
        return;
    }

    /* 设置STA作为默认的接口 */
    esp_netif_set_default_netif(esp_netif_sta);

    /* 在 AP netif 上使能napt，ESP STA 连接路由器(互联网)，其他 STA 终端连接到 ESP AP， 
     * 其他 STA 终端通过 ESP AP访问互联网。该功能属于实验性功能，需要再lwip组件打开
    */
    if (ESP_OK != esp_netif_napt_enable(esp_netif_ap))
    {
        ESP_LOGI(TAG_STA, "NAPT not enabled on the netif: %p", esp_netif_ap);
    }
}