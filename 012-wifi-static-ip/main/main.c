#include <string.h>
#include <netdb.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "lwip/ip_addr.h"

/* WiFi STA 配置 */
#define EXAMPLE_WIFI_SSID CONFIG_EXAMPLE_WIFI_SSID
#define EXAMPLE_WIFI_PASS CONFIG_EXAMPLE_WIFI_PASSWORD
#define EXAMPLE_MAXIMUM_RETRY CONFIG_EXAMPLE_MAXIMUM_RETRY
#define EXAMPLE_STATIC_IP_ADDR CONFIG_EXAMPLE_STATIC_IP_ADDR
#define EXAMPLE_STATIC_NETMASK_ADDR CONFIG_EXAMPLE_STATIC_NETMASK_ADDR
#define EXAMPLE_STATIC_GW_ADDR CONFIG_EXAMPLE_STATIC_GW_ADDR

/* DNS 服务器配置 */
#ifdef CONFIG_EXAMPLE_STATIC_DNS_AUTO
    #define EXAMPLE_MAIN_DNS_SERVER EXAMPLE_STATIC_GW_ADDR
    #define EXAMPLE_BACKUP_DNS_SERVER "0.0.0.0"
#else
    #define EXAMPLE_MAIN_DNS_SERVER CONFIG_EXAMPLE_STATIC_DNS_SERVER_MAIN
    #define EXAMPLE_BACKUP_DNS_SERVER CONFIG_EXAMPLE_STATIC_DNS_SERVER_BACKUP
#endif

/* 目标 DNS 域名解析 */
#ifdef CONFIG_EXAMPLE_STATIC_DNS_RESOLVE_TEST
    #define EXAMPLE_RESOLVE_DOMAIN CONFIG_EXAMPLE_STATIC_RESOLVE_DOMAIN
#endif

/* 连接事件结果 bit */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "static_ip";
static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static esp_err_t example_set_dns_server(esp_netif_t *netif, 
                                        uint32_t addr, 
                                        esp_netif_dns_type_t type)
{
    if (addr && (addr != IPADDR_NONE))
    {
        esp_netif_dns_info_t dns;
        dns.ip.u_addr.ip4.addr = addr;
        dns.ip.type = IPADDR_TYPE_V4;
        ESP_ERROR_CHECK(esp_netif_set_dns_info(netif, type, &dns));
    }
    return ESP_OK;
}

static void example_set_static_ip(esp_netif_t *netif)
{
    // 禁用 DHCP
    if (esp_netif_dhcpc_stop(netif) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to stop dhcp client");
        return;
    }

    // 设置静态IP地址、子网掩码、网关
    esp_netif_ip_info_t ip;
    memset(&ip, 0, sizeof(esp_netif_ip_info_t));
    ip.ip.addr = ipaddr_addr(EXAMPLE_STATIC_IP_ADDR);
    ip.netmask.addr = ipaddr_addr(EXAMPLE_STATIC_NETMASK_ADDR);
    ip.gw.addr = ipaddr_addr(EXAMPLE_STATIC_GW_ADDR);
    if (esp_netif_set_ip_info(netif, &ip) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to set ip info");
        return;
    }
    ESP_LOGI(TAG, "Success to set static ip: %s, netmask: %s, gw: %s",
             EXAMPLE_STATIC_IP_ADDR, EXAMPLE_STATIC_NETMASK_ADDR,
             EXAMPLE_STATIC_GW_ADDR);
    
    // 设置 DNS 服务器（主&备）
    ESP_LOGI(TAG, "Set DNS Server: "
                  "Main DNS: %s, "
                  "Backup DNS: %s ", 
                  EXAMPLE_MAIN_DNS_SERVER, EXAMPLE_BACKUP_DNS_SERVER);
    ESP_ERROR_CHECK(example_set_dns_server(netif,
                                           ipaddr_addr(EXAMPLE_MAIN_DNS_SERVER), 
                                           ESP_NETIF_DNS_MAIN));
    ESP_ERROR_CHECK(example_set_dns_server(netif,
                                           ipaddr_addr(EXAMPLE_BACKUP_DNS_SERVER), 
                                           ESP_NETIF_DNS_BACKUP));
}

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        // WiFi 启动成功，WiFi STA连接指定的AP
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
    {
        // WiFi STA 已连接指定AP，设置静态IP
        example_set_static_ip(arg);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        // WiFi STA连接指定的AP失败，重试连接
        if (s_retry_num < EXAMPLE_MAXIMUM_RETRY)
        {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        }
        else
        {
            // 重试多次任然失败，在事件组设置连接失败 bit
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        // WiFi STA 获取IP地址
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "static ip:" IPSTR, IP2STR(&event->ip_info.ip));

        s_retry_num = 0; // 重置重连计数器
        // WiFI STA 成功获取IP地址，在事件组设置连接成功 bit
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void wifi_init_sta(void)
{
    // 创建事件组
    s_wifi_event_group = xEventGroupCreate();

    // TCP/IP 协议栈初始化
    ESP_ERROR_CHECK(esp_netif_init());
    // 创建默认的事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 创建默认的STA实例接口
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    // WiFi 初始化
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 设置 WiFi 模式：STA
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // 注册事件回调函数，用户监听事件
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        sta_netif,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        sta_netif,
                                                        &instance_got_ip));
    // 设置 WiFi 配置项
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_WIFI_SSID,
            .password = EXAMPLE_WIFI_PASS,
	        .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    // 启动 WiFi
    ESP_ERROR_CHECK(esp_wifi_start() );
    ESP_LOGI(TAG, "wifi_init_sta finished.");

    // 阻塞在此，等待WiFi STA 连接AP的结果
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);
    // 通过 bits 测试WiFi STA 连接AP的结果
    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID: %s, password: %s",
                 EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PASS);
    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID: %s, password:%s",
                 EXAMPLE_WIFI_SSID, EXAMPLE_WIFI_PASS);
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

/* DNS域名解析 */
#ifdef CONFIG_EXAMPLE_STATIC_DNS_RESOLVE_TEST
    struct addrinfo *address_info;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    ESP_LOGI(TAG, "DNS resolve test...");
    int res = getaddrinfo(EXAMPLE_RESOLVE_DOMAIN, NULL, &hints, &address_info);
    if (res != 0 || address_info == NULL)
    {
        ESP_LOGE(TAG, "couldn't get hostname for :%s: "
                      "getaddrinfo() returns %d, addrinfo=%p",
                 EXAMPLE_RESOLVE_DOMAIN, res, address_info);
    }
    else
    {
        if (address_info->ai_family == AF_INET)
        {
            struct sockaddr_in *p = (struct sockaddr_in *)address_info->ai_addr;
            ESP_LOGI(TAG, "Target DNS Domain: %s, Resolved IPv4 address: %s",
                    EXAMPLE_RESOLVE_DOMAIN,
                    ipaddr_ntoa((const ip_addr_t *)&p->sin_addr.s_addr));
        }
#if CONFIG_LWIP_IPV6
        else if (address_info->ai_family == AF_INET6)
        {
            struct sockaddr_in6 *p = (struct sockaddr_in6 *)address_info->ai_addr;
            ESP_LOGI(TAG, "Target DNS Domain: %s, Resolved IPv6 address: %s",
                    EXAMPLE_RESOLVE_DOMAIN,
                    ip6addr_ntoa((const ip6_addr_t *)&p->sin6_addr));
        }
#endif
    }
#endif

    // 取消事件的监听
    ESP_LOGI(TAG, "Unregister IP_EVENT and WIFI_EVENT");
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT,
                                                          IP_EVENT_STA_GOT_IP,
                                                          instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT,
                                                          ESP_EVENT_ANY_ID,
                                                          instance_any_id));
    // 删除事件组
    ESP_LOGI(TAG, "Delete event group");
    vEventGroupDelete(s_wifi_event_group);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    wifi_init_sta();
}
