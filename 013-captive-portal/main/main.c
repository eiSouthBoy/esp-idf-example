#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "lwip/inet.h"

#include "esp_http_server.h"
#include "dns_server.h"


/* WiFI AP 配置项 */
#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD
#define EXAMPLE_MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN

static esp_err_t root_get_handler(httpd_req_t *req);

static const char *TAG = "captive_portal";

/* root.html */
extern const char root_start[] asm("_binary_root_html_start");
extern const char root_end[] asm("_binary_root_html_end");

/* 为指定的 URI 提供处理函数 */
static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler
};

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) // 有STA 连接 ESP32 AP
    {
        wifi_event_ap_staconnected_t *event = NULL;
        event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) // STA 断开ESP32 AP
    {
        wifi_event_ap_stadisconnected_t *event = NULL;
        event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

static void wifi_init_softap(void)
{
    // WiFi 初始化
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件回调函数，监听WiFI_EVENT
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, 
                                               ESP_EVENT_ANY_ID, 
                                               &wifi_event_handler, 
                                               NULL));
    // WiFi AP 配置项
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK}, // WiFi AP的默认加密模式设置：WPA-WPS2-PSK
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0)
    {
        // 若设置的密码为空，WIFi AP加密模式设置为：OPEN
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));

    // 启动 WiFi
    ESP_ERROR_CHECK(esp_wifi_start());

    // 通过 WiFi AP 默认的ifkey(即 WIFI_AP_DEF) 获取IP地址
    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), 
                          &ip_info);
    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID: %s, password: %s",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

static esp_err_t root_get_handler(httpd_req_t *req)
{
    const uint32_t root_len = root_end - root_start;

    ESP_LOGI(TAG, "Serve root");
    // 设置http content type
    httpd_resp_set_type(req, "text/html");
    // 发送 http header & body
    httpd_resp_send(req, root_start, root_len);

    return ESP_OK;
}

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // 设置 http status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // 设置 http 报头中Location字段
    httpd_resp_set_hdr(req, "Location", "/");
    // 发送 http 响应报文
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;

    // 使用默认http 配置项，并启动 http server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        // 在http server 上注册 URI
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);

        // 在http server 上注册 404 ERROR
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, 
                                   http_404_error_handler);
    }
    return server;
}

void app_main(void)
{
    // 为指定的模块设置日志级别
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);   

    // NVS 初始化
    ESP_ERROR_CHECK(nvs_flash_init());

    // TCP/IP 协议栈初始化
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // 创建默认的 AP 实例接口
    esp_netif_create_default_wifi_ap();

    // WiFi AP 初始化
    wifi_init_softap();

    // 启动 web server
    start_webserver();

    // 启动 dns server
    dns_server_config_t config = DNS_SERVER_CONFIG_SINGLE("*" , "WIFI_AP_DEF");
    start_dns_server(&config); 
}