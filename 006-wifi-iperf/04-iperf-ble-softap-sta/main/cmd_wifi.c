/* Iperf Example - wifi commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "cmd_decl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "iperf.h"
#include "esp_coexist.h"
#include "wifi_cmd.h"
#include "esp_wifi_he.h"

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
// static int g_retry_num = 0;

typedef struct {
    struct arg_str *ip;
    struct arg_lit *server;
    struct arg_lit *udp;
    struct arg_lit *version;
    struct arg_int *port;
    struct arg_int *length;
    struct arg_int *interval;
    struct arg_int *time;
    struct arg_int *bw_limit;
    struct arg_lit *abort;
    struct arg_end *end;
} wifi_iperf_t;

typedef struct {
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} wifi_args_t;

typedef struct {
    struct arg_str *ssid;
    struct arg_end *end;
} wifi_scan_arg_t;

static wifi_iperf_t iperf_args;
static wifi_args_t sta_args;
static wifi_scan_arg_t scan_args;
static wifi_args_t ap_args;
static bool reconnect = true;
static const char *TAG = "cmd_wifi";
esp_netif_t *netif_ap = NULL;
esp_netif_t *netif_sta = NULL;

EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;
const int DISCONNECTED_BIT = BIT1;

static void scan_done_handler(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data)
{
    uint16_t sta_number = 0;
    uint8_t i;
    wifi_ap_record_t *ap_list_buffer;

    esp_wifi_scan_get_ap_num(&sta_number);
    if (!sta_number) {
        ESP_LOGE(TAG, "No AP found");
        return;
    }

    ap_list_buffer = malloc(sta_number * sizeof(wifi_ap_record_t));
    if (ap_list_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to malloc buffer to print scan results");
        esp_wifi_clear_ap_list();
        return;
    }

    if (esp_wifi_scan_get_ap_records(&sta_number, (wifi_ap_record_t *)ap_list_buffer) == ESP_OK) {
        for (i = 0; i < sta_number; i++) {
#if CONFIG_SOC_WIFI_HE_SUPPORT
            char ssid_rssi[46] = { 0, };
            sprintf(ssid_rssi, "[%s][rssi=%d]", ap_list_buffer[i].ssid, ap_list_buffer[i].rssi);
            if (ap_list_buffer[i].phy_11ax) {
                ESP_LOGW(TAG,
                         "[%2d]%45s authmode:0x%x, channel:%2d[%d], phymode:%4s, "MACSTR", bssid-index:%d, bss_color:%d, disabled:%d",
                         i, ssid_rssi, ap_list_buffer[i].authmode,
                         ap_list_buffer[i].primary, ap_list_buffer[i].second,
                         ap_list_buffer[i].phy_11ax ? "11ax" : (ap_list_buffer[i].phy_11n ? "11n" :
                                 (ap_list_buffer[i].phy_11g ? "11g" : (ap_list_buffer[i].phy_11b ? "11b" : ""))),
                         MAC2STR(ap_list_buffer[i].bssid), ap_list_buffer[i].he_ap.bssid_index,
                         ap_list_buffer[i].he_ap.bss_color, ap_list_buffer[i].he_ap.bss_color_disabled);
            } else {
                ESP_LOGI(TAG,
                         "[%2d]%45s authmode:0x%x, channel:%2d[%d], phymode:%4s, "MACSTR"",
                         i, ssid_rssi, ap_list_buffer[i].authmode,
                         ap_list_buffer[i].primary, ap_list_buffer[i].second,
                         ap_list_buffer[i].phy_11ax ? "11ax" : (ap_list_buffer[i].phy_11n ? "11n" :
                                 (ap_list_buffer[i].phy_11g ? "11g" : (ap_list_buffer[i].phy_11b ? "11b" : ""))),
                         MAC2STR(ap_list_buffer[i].bssid));
            }
#else
            ESP_LOGI(TAG, "[%s][rssi=%d]", ap_list_buffer[i].ssid, ap_list_buffer[i].rssi);
#endif
        }
    }
    free(ap_list_buffer);
    ESP_LOGI(TAG, "sta scan done");
}

static void got_ip_handler(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data)
{
    // DHCP Client成功获取IPv4地址
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP " IPSTR, IP2STR(&event->ip_info.ip));

    reconnect = false;
    xEventGroupClearBits(wifi_event_group, DISCONNECTED_BIT);
    xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{

    if (reconnect) 
    {
        ESP_LOGI(TAG, "sta disconnect, reconnect...");
        esp_wifi_connect();
    } 
    else 
    {
        ESP_LOGI(TAG, "sta disconnect");
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        xEventGroupSetBits(wifi_event_group, DISCONNECTED_BIT);
    }
}

static void connect_wifi_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    // WiFi 初始化完成，准备连接目标WiFi
    ESP_LOGI(TAG, "--> esp_wifi_connect()");
    esp_wifi_connect();
}

static void sta_connect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
    ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
}

static void sta_disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
    ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
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

    ESP_LOGI(TAG, "--> wifi_init_softap finished. SSID: %s, password: %s, channel: %d",
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

    ESP_LOGI(TAG, "--> wifi_init_sta finished.");

    return esp_netif_sta;    
}

/* WiFi 初始化 */
void initialise_wifi(void)
{
    esp_log_level_set("wifi", ESP_LOG_WARN);
    static bool initialized = false;

    if (initialized) {
        return;
    }

    // 初始化 TCP/IP 协议栈
    ESP_ERROR_CHECK(esp_netif_init());
    // 创建默认事件循环
    ESP_ERROR_CHECK( esp_event_loop_create_default());
    // 创建事件群组
    wifi_event_group = xEventGroupCreate();

    // 注册监听WIFI_EVENT 和 IP_EVENT 的事件回调函数
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        WIFI_EVENT_SCAN_DONE,
                                                        &scan_done_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        WIFI_EVENT_STA_START,
                                                        &connect_wifi_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        WIFI_EVENT_STA_DISCONNECTED,
                                                        &disconnect_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        WIFI_EVENT_AP_STACONNECTED,
                                                        &sta_connect_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        WIFI_EVENT_AP_STADISCONNECTED,
                                                        &sta_disconnect_handler,
                                                        NULL,
                                                        NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &got_ip_handler,
                                                        NULL,
                                                        NULL));

    // WiFi 初始化
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA)); // SoftAP + STA 共存模式
    
    // 分别创建AP、STA的 esp_netif 对象
    // netif_ap = esp_netif_create_default_wifi_ap();
    // assert(netif_ap);
    // netif_sta = esp_netif_create_default_wifi_sta();
    // assert(netif_sta);

    /* AP 初始化 */
    ESP_LOGI(TAG, "--> wifi_init_softap()");
    netif_ap = wifi_init_softap();

    /* STA 初始化 */
    ESP_LOGI(TAG, "--> wifi_init_sta()");
    netif_sta = wifi_init_sta();

    // 将WIFI信息存储到flash
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    // WiFi 启动
    ESP_ERROR_CHECK(esp_wifi_start());

    /* 等待 “连接建立”或 “连接失败” ，任务被阻塞在此处*/
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           CONNECTED_BIT | DISCONNECTED_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    /* 测试返回事件bits */
    if (bits & CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID: %s, password: %s",
                 EXAMPLE_ESP_WIFI_STA_SSID, EXAMPLE_ESP_WIFI_STA_PASSWD);
    }
    else if (bits & DISCONNECTED_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_STA_SSID, EXAMPLE_ESP_WIFI_STA_PASSWD);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        return;
    }

#if CONFIG_EXTERNAL_COEX_ENABLE
    esp_external_coex_gpio_set_t gpio_pin;
    gpio_pin.request = 1;
    gpio_pin.priority = 2;
    gpio_pin.grant = 3;
#if SOC_EXTERNAL_COEX_LEADER_TX_LINE
    gpio_pin.tx_line = 4;
#endif

    esp_external_coex_set_work_mode(EXTERNAL_COEX_LEADER_ROLE);
#if SOC_EXTERNAL_COEX_LEADER_TX_LINE
    ESP_ERROR_CHECK(esp_enable_extern_coex_gpio_pin(EXTERN_COEX_WIRE_4, gpio_pin));
#else
    ESP_ERROR_CHECK(esp_enable_extern_coex_gpio_pin(EXTERN_COEX_WIRE_3, gpio_pin));
#endif /* SOC_EXTERNAL_COEX_LEADER_TX_LINE */
#endif /* CONFIG_EXTERNAL_COEX_ENABLE */

#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_STATS
#if CONFIG_ESP_WIFI_ENABLE_WIFI_RX_MU_STATS
    esp_wifi_enable_rx_statistics(true, true);
#else
    esp_wifi_enable_rx_statistics(true, false);
#endif
#endif
#if CONFIG_ESP_WIFI_ENABLE_WIFI_TX_STATS
    esp_wifi_enable_tx_statistics(ESP_WIFI_ACI_BE, true);
#endif

    // 更新标志位
    initialized = true;
}

static bool wifi_cmd_sta_join(const char *ssid, const char *pass, bool enable_he_mcs9)
{
    int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);

    wifi_config_t wifi_config = { 0 };

    strlcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass) {
        strlcpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    }

    if (enable_he_mcs9 == true) {
        wifi_config.sta.he_mcs9_enabled = 1;
    }

    if (bits & CONNECTED_BIT) {
        reconnect = false;
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
        ESP_ERROR_CHECK( esp_wifi_disconnect() );
        xEventGroupWaitBits(wifi_event_group, DISCONNECTED_BIT, 0, 1, portTICK_PERIOD_MS);
    }

    reconnect = true;
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    esp_wifi_connect();

    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 5000 / portTICK_PERIOD_MS);

    return true;
}

static int wifi_cmd_sta(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &sta_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, sta_args.end, argv[0]);
        return 1;
    }

    ESP_LOGI(TAG, "sta connecting to '%s'", sta_args.ssid->sval[0]);
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    wifi_cmd_sta_join(sta_args.ssid->sval[0], sta_args.password->sval[0], false);
    return 0;
}

static int wifi_cmd_sta40(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &sta_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, sta_args.end, argv[0]);
        return 1;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(0, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(0, WIFI_BW_HT40));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "sta connecting to '%s'", sta_args.ssid->sval[0]);
    wifi_cmd_sta_join(sta_args.ssid->sval[0], sta_args.password->sval[0], false);
    return 0;
}

static int wifi_cmd_sta_mcs89(int argc, char **argv)
{
#if CONFIG_SOC_WIFI_HE_SUPPORT
    int nerrors = arg_parse(argc, argv, (void **) &sta_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, sta_args.end, argv[0]);
        return 1;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(0, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_11AX));
    ESP_ERROR_CHECK(esp_wifi_set_bandwidth(0, WIFI_BW_HT20));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_LOGI(TAG, "sta connecting to '%s'", sta_args.ssid->sval[0]);
    wifi_cmd_sta_join(sta_args.ssid->sval[0], sta_args.password->sval[0], true);
#else
    ESP_LOGW(TAG, "HE-MCS[0, 9] is not supported");
#endif
    return 0;
}

static bool wifi_cmd_sta_scan(const char *ssid)
{
    wifi_scan_config_t scan_config = { 0 };
    scan_config.ssid = (uint8_t *) ssid;

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    esp_wifi_scan_start(&scan_config, false);

    return true;
}

static int wifi_cmd_scan(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &scan_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, scan_args.end, argv[0]);
        return 1;
    }

    ESP_LOGI(TAG, "sta start to scan");
    if ( scan_args.ssid->count == 1 ) {
        wifi_cmd_sta_scan(scan_args.ssid->sval[0]);
    } else {
        wifi_cmd_sta_scan(NULL);
    }
    return 0;
}


static bool wifi_cmd_ap_set(const char *ssid, const char *pass)
{
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = "",
            .ssid_len = 0,
            .max_connection = 4,
            .password = "",
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    reconnect = false;
    strlcpy((char *) wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    if (pass) {
        if (strlen(pass) != 0 && strlen(pass) < 8) {
            reconnect = true;
            ESP_LOGE(TAG, "password less than 8");
            return false;
        }
        strlcpy((char *) wifi_config.ap.password, pass, sizeof(wifi_config.ap.password));
    }

    if (strlen(pass) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    return true;
}

static int wifi_cmd_ap(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &ap_args);

    if (nerrors != 0) {
        arg_print_errors(stderr, ap_args.end, argv[0]);
        return 1;
    }

    wifi_cmd_ap_set(ap_args.ssid->sval[0], ap_args.password->sval[0]);
    ESP_LOGI(TAG, "AP mode, %s %s", ap_args.ssid->sval[0], ap_args.password->sval[0]);
    return 0;
}

static int wifi_cmd_query(int argc, char **argv)
{
    wifi_config_t cfg;
    wifi_mode_t mode;

    esp_wifi_get_mode(&mode);
    if (WIFI_MODE_AP == mode) {
        esp_wifi_get_config(WIFI_IF_AP, &cfg);
        ESP_LOGI(TAG, "AP mode, %s %s", cfg.ap.ssid, cfg.ap.password);
    } else if (WIFI_MODE_STA == mode) {
        int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);
        if (bits & CONNECTED_BIT) {
            esp_wifi_get_config(WIFI_IF_STA, &cfg);
            ESP_LOGI(TAG, "sta mode, connected %s", cfg.ap.ssid);
        } else {
            ESP_LOGI(TAG, "sta mode, disconnected");
        }
    } else {
        ESP_LOGI(TAG, "NULL mode");
        return 0;
    }

    return 0;
}

static uint32_t wifi_get_local_ip(void)
{
    int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);
    esp_netif_t *netif = netif_ap;
    esp_netif_ip_info_t ip_info;
    wifi_mode_t mode;

    esp_wifi_get_mode(&mode);
    if (WIFI_MODE_STA == mode) {
        bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT, 0, 1, 0);
        if (bits & CONNECTED_BIT) {
            netif = netif_sta;
        } else {
            ESP_LOGE(TAG, "sta has no IP");
            return 0;
        }
    }

    esp_netif_get_ip_info(netif, &ip_info);
    return ip_info.ip.addr;
}

static int wifi_cmd_iperf(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **) &iperf_args);
    iperf_cfg_t cfg;

    if (nerrors != 0) {
        arg_print_errors(stderr, iperf_args.end, argv[0]);
        return 0;
    }

    memset(&cfg, 0, sizeof(cfg));

    // now wifi iperf only support IPV4 address
    cfg.type = IPERF_IP_TYPE_IPV4;

    if ( iperf_args.abort->count != 0) {
        iperf_stop();
        return 0;
    }

    if ( ((iperf_args.ip->count == 0) && (iperf_args.server->count == 0)) ||
            ((iperf_args.ip->count != 0) && (iperf_args.server->count != 0)) ) {
        ESP_LOGE(TAG, "should specific client/server mode");
        return 0;
    }

    if (iperf_args.ip->count == 0) {
        cfg.flag |= IPERF_FLAG_SERVER;
    } else {
        cfg.destination_ip4 = esp_ip4addr_aton(iperf_args.ip->sval[0]);
        cfg.flag |= IPERF_FLAG_CLIENT;
    }

    cfg.source_ip4 = wifi_get_local_ip();
    if (cfg.source_ip4 == 0) {
        return 0;
    }

    if (iperf_args.udp->count == 0) {
        cfg.flag |= IPERF_FLAG_TCP;
    } else {
        cfg.flag |= IPERF_FLAG_UDP;
    }

    if (iperf_args.length->count == 0) {
        cfg.len_send_buf = 0;
    } else {
        cfg.len_send_buf = iperf_args.length->ival[0];
    }

    if (iperf_args.port->count == 0) {
        cfg.sport = IPERF_DEFAULT_PORT;
        cfg.dport = IPERF_DEFAULT_PORT;
    } else {
        if (cfg.flag & IPERF_FLAG_SERVER) {
            cfg.sport = iperf_args.port->ival[0];
            cfg.dport = IPERF_DEFAULT_PORT;
        } else {
            cfg.sport = IPERF_DEFAULT_PORT;
            cfg.dport = iperf_args.port->ival[0];
        }
    }

    if (iperf_args.interval->count == 0) {
        cfg.interval = IPERF_DEFAULT_INTERVAL;
    } else {
        cfg.interval = iperf_args.interval->ival[0];
        if (cfg.interval <= 0) {
            cfg.interval = IPERF_DEFAULT_INTERVAL;
        }
    }

    if (iperf_args.time->count == 0) {
        cfg.time = IPERF_DEFAULT_TIME;
    } else {
        cfg.time = iperf_args.time->ival[0];
        if (cfg.time <= cfg.interval) {
            cfg.time = cfg.interval;
        }
    }

    /* iperf -b */
    if (iperf_args.bw_limit->count == 0) {
        cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;
    } else {
        cfg.bw_lim = iperf_args.bw_limit->ival[0];
        if (cfg.bw_lim <= 0) {
            cfg.bw_lim = IPERF_DEFAULT_NO_BW_LIMIT;
        }
    }


    ESP_LOGI(TAG, "mode=%s-%s sip=%" PRId32 ".%" PRId32 ".%" PRId32 ".%" PRId32 ":%d,\
             dip=%" PRId32 ".%" PRId32 ".%" PRId32 ".%" PRId32 ":%d,\
             interval=%" PRId32 ", time=%" PRId32 "",
             cfg.flag & IPERF_FLAG_TCP ? "tcp" : "udp",
             cfg.flag & IPERF_FLAG_SERVER ? "server" : "client",
             cfg.source_ip4 & 0xFF, (cfg.source_ip4 >> 8) & 0xFF, (cfg.source_ip4 >> 16) & 0xFF,
             (cfg.source_ip4 >> 24) & 0xFF, cfg.sport,
             cfg.destination_ip4 & 0xFF, (cfg.destination_ip4 >> 8) & 0xFF,
             (cfg.destination_ip4 >> 16) & 0xFF, (cfg.destination_ip4 >> 24) & 0xFF, cfg.dport,
             cfg.interval, cfg.time);

    iperf_start(&cfg);

    return 0;
}

void register_wifi(void)
{
    sta_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    sta_args.password = arg_str0(NULL, NULL, "<pass>", "password of AP");
    sta_args.end = arg_end(2);

    const esp_console_cmd_t sta_cmd = {
        .command = "sta",
        .help = "WiFi is station mode, join specified soft-AP",
        .hint = NULL,
        .func = &wifi_cmd_sta,
        .argtable = &sta_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&sta_cmd) );

    const esp_console_cmd_t sta40_cmd = {
        .command = "sta40",
        .help = "WiFi is station mode, set protocol to bgn and cbw to 40MHz, join a specified AP",
        .hint = NULL,
        .func = &wifi_cmd_sta40,
        .argtable = &sta_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&sta40_cmd) );

    const esp_console_cmd_t stamcs89_cmd = {
        .command = "stamcs89",
        .help = "WiFi is station mode, set protocol to ax and mcs set to HE-MCS[0,9], join a specified AP",
        .hint = NULL,
        .func = &wifi_cmd_sta_mcs89,
        .argtable = &sta_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&stamcs89_cmd) );

    scan_args.ssid = arg_str0(NULL, NULL, "<ssid>", "SSID of AP want to be scanned");
    scan_args.end = arg_end(1);

    const esp_console_cmd_t scan_cmd = {
        .command = "scan",
        .help = "WiFi is station mode, start scan ap",
        .hint = NULL,
        .func = &wifi_cmd_scan,
        .argtable = &scan_args
    };

    ap_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    ap_args.password = arg_str0(NULL, NULL, "<pass>", "password of AP");
    ap_args.end = arg_end(2);


    ESP_ERROR_CHECK( esp_console_cmd_register(&scan_cmd) );

    const esp_console_cmd_t ap_cmd = {
        .command = "ap",
        .help = "AP mode, configure ssid and password",
        .hint = NULL,
        .func = &wifi_cmd_ap,
        .argtable = &ap_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&ap_cmd) );

    const esp_console_cmd_t query_cmd = {
        .command = "query",
        .help = "query WiFi info",
        .hint = NULL,
        .func = &wifi_cmd_query,
    };
    ESP_ERROR_CHECK( esp_console_cmd_register(&query_cmd) );

    iperf_args.ip = arg_str0("c", "client", "<ip>", "run in client mode, connecting to <host>");
    iperf_args.server = arg_lit0("s", "server", "run in server mode");
    iperf_args.udp = arg_lit0("u", "udp", "use UDP rather than TCP");
    iperf_args.version = arg_lit0("V", "ipv6_domain", "use IPV6 address rather than IPV4");
    iperf_args.port = arg_int0("p", "port", "<port>", "server port to listen on/connect to");
    iperf_args.length = arg_int0("l", "len", "<length>", "Set read/write buffer size");
    iperf_args.interval = arg_int0("i", "interval", "<interval>", "seconds between periodic bandwidth reports");
    iperf_args.time = arg_int0("t", "time", "<time>", "time in seconds to transmit for (default 10 secs)");
    iperf_args.bw_limit = arg_int0("b", "bandwidth", "<bandwidth>", "bandwidth to send at in Mbits/sec");
    iperf_args.abort = arg_lit0("a", "abort", "abort running iperf");
    iperf_args.end = arg_end(1);
    const esp_console_cmd_t iperf_cmd = {
        .command = "iperf",
        .help = "iperf command",
        .hint = NULL,
        .func = &wifi_cmd_iperf,
        .argtable = &iperf_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&iperf_cmd) );

    register_wifi_cmd();
    register_wifi_stats();
}
