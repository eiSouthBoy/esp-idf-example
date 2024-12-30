/* WireGuard demo example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <inttypes.h>
#include <time.h>
#include <sys/time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_idf_version.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "lwip/netdb.h"
#include "ping/ping_sock.h"

#include "esp_wireguard.h"
#include "sync_time.h"

#include "protocol_examples_common.h"

#if defined(CONFIG_IDF_TARGET_ESP8266)
#include <esp_netif.h>
#elif ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 2, 0)
#include <tcpip_adapter.h>
#else
#include "esp_netif.h"
#endif

static const char *TAG = "demo";
static wireguard_config_t wg_config = ESP_WIREGUARD_CONFIG_DEFAULT();

static esp_err_t wireguard_setup(wireguard_ctx_t* ctx)
{
    esp_err_t err = ESP_FAIL;

    ESP_LOGI(TAG, "Initializing WireGuard.");
    wg_config.private_key = CONFIG_WG_PRIVATE_KEY; // 本地WireGuard私钥
    wg_config.listen_port = CONFIG_WG_LOCAL_PORT;  // 本地WireGuard端口
    wg_config.public_key = CONFIG_WG_PEER_PUBLIC_KEY; // 对等方WireGuard公钥
    if (strcmp(CONFIG_WG_PRESHARED_KEY, "") != 0) // 判断是否有WireGuard共享秘钥
    {
        wg_config.preshared_key = CONFIG_WG_PRESHARED_KEY;
    }
    else
    {
        wg_config.preshared_key = NULL;
    }
    wg_config.allowed_ip = CONFIG_WG_LOCAL_IP_ADDRESS;  // 本地WireGuard使用的IP地址
    wg_config.allowed_ip_mask = CONFIG_WG_LOCAL_IP_NETMASK; // 本地WireGuard使用的掩码
    wg_config.endpoint = CONFIG_WG_PEER_ADDRESS;    // 对等方WireGuard使用的IP地址或域名
    wg_config.port = CONFIG_WG_PEER_PORT;   // 对等放WireGuard使用的端口
    wg_config.persistent_keepalive = CONFIG_WG_PERSISTENT_KEEP_ALIVE;  // 向对等体发送经过身份验证的空数据包间隔

    // WireGuard 初始化
    err = esp_wireguard_init(&wg_config, ctx);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wireguard_init: %s", esp_err_to_name(err));
        goto fail;
    }

    // WireGuard 连接对等方
    ESP_LOGI(TAG, "Connecting to the peer.");
    err = esp_wireguard_connect(ctx);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "esp_wireguard_connect: %s", esp_err_to_name(err));
        goto fail;
    }

    err = ESP_OK;
fail:
    return err;
}

static void test_on_ping_success(esp_ping_handle_t hdl, void *args)
{
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time, sizeof(elapsed_time));
    ESP_LOGI(TAG, "%" PRIu32 " bytes from %s icmp_seq=%" PRIu16 " ttl=%" PRIi8 " time=%" PRIu32 " ms",
           recv_len, ipaddr_ntoa(&target_addr), seqno, ttl, elapsed_time);
}

static void test_on_ping_timeout(esp_ping_handle_t hdl, void *args)
{
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr, sizeof(target_addr));
    ESP_LOGI(TAG, "From %s icmp_seq=%" PRIu16 " timeout", ipaddr_ntoa(&target_addr), seqno);
}

static void test_on_ping_end(esp_ping_handle_t hdl, void *args)
{
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted, sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms, sizeof(total_time_ms));
    ESP_LOGI(TAG, "%" PRIu32 " packets transmitted, %" PRIu32 " received, time %" PRIu32 "ms", transmitted, received, total_time_ms);
}

void start_ping()
{
    ESP_LOGI(TAG, "Initializing ping...");
    /* convert URL to IP address */
    ip_addr_t target_addr;
    struct addrinfo *res = NULL;
    struct addrinfo hint;
    memset(&hint, 0, sizeof(hint));
    memset(&target_addr, 0, sizeof(target_addr));
    ESP_ERROR_CHECK(lwip_getaddrinfo(CONFIG_EXAMPLE_PING_ADDRESS, 
                                     NULL, &hint, &res) == 0 ? ESP_OK : ESP_FAIL);
    struct in_addr addr4 = ((struct sockaddr_in *) (res->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
    lwip_freeaddrinfo(res);
    ESP_LOGI(TAG, "ICMP echo target: %s", CONFIG_EXAMPLE_PING_ADDRESS);
    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr = target_addr;          // target IP address
    ping_config.count = ESP_PING_COUNT_INFINITE;    // ping in infinite mode, esp_ping_stop can stop it

    /* set callback functions */
    esp_ping_callbacks_t cbs;
    cbs.on_ping_success = test_on_ping_success;
    cbs.on_ping_timeout = test_on_ping_timeout;
    cbs.on_ping_end = test_on_ping_end;
    cbs.cb_args = NULL;

    // 创建一个ping会话，接着开始ping
    esp_ping_handle_t ping;
    ESP_ERROR_CHECK(esp_ping_new_session(&ping_config, &cbs, &ping));
    esp_ping_start(ping);
}

void app_main(void)
{
    esp_err_t err;
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    wireguard_ctx_t ctx = {0};

    esp_log_level_set("esp_wireguard", ESP_LOG_DEBUG);
    esp_log_level_set("wireguardif", ESP_LOG_DEBUG);
    esp_log_level_set("wireguard", ESP_LOG_DEBUG);

    // NVS初始化
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    // TCP/IP 协议栈初始化
    ESP_ERROR_CHECK(esp_netif_init());
    // 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 网络连接，通过STA连接到路由器 或 通过ETH连接到路由器。
    ESP_ERROR_CHECK(example_connect());

    // 同步时间
    obtain_time();

    // 设置时区，中国上海
    time(&now);
    setenv("TZ", "CST-8", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in Beijing is: %s", strftime_buf);

    // WireGuard 初始化，并连接到对等方
    err = wireguard_setup(&ctx);
    if (err != ESP_OK) 
    {
        ESP_LOGE(TAG, "wireguard_setup: %s", esp_err_to_name(err));
        goto fail;
    }

    while (1) 
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        // 检查对等方WireGuard是否启动
        err = esp_wireguardif_peer_is_up(&ctx);
        if (err == ESP_OK) 
        {
            ESP_LOGI(TAG, "Peer is up");
            break;
        } 
        else 
        {
            ESP_LOGI(TAG, "Peer is down");
        }
    }
    start_ping();

    while (1) 
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

fail:
    ESP_LOGE(TAG, "Halting due to error");
    while (1) 
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}
