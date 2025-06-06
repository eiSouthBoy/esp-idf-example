/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/*
 * MDNS-SD Query and advertise Example
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif_ip_addr.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "mdns.h"
#include "driver/gpio.h"
#include "netdb.h"

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 0)
/* CONFIG_LWIP_IPV4 was introduced in IDF v5.1, set CONFIG_LWIP_IPV4 to 1 by default for IDF v5.0 */
#ifndef CONFIG_LWIP_IPV4
#define CONFIG_LWIP_IPV4 1
#endif // CONFIG_LWIP_IPV4
#endif // ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 1, 0)

#define EXAMPLE_MDNS_INSTANCE CONFIG_MDNS_INSTANCE
#define EXAMPLE_BUTTON_GPIO   CONFIG_MDNS_BUTTON_GPIO

/* these strings match mdns_ip_protocol_t enumeration */
static const char *ip_protocol_str[] = {"V4", "V6", "MAX"};
static const char *TAG = "mdns-test";

#if CONFIG_MDNS_RESOLVE_TEST_SERVICES == 1
/**
 *  @brief Executes gethostbyname and displays list of resolved addresses.
 *  Note: This function is used only to test advertised mdns hostnames resolution
 */
static void  query_mdns_host_with_gethostbyname(char *host)
{
    struct hostent *res = gethostbyname(host);
    if (res) {
        unsigned int i = 0;
        while (res->h_addr_list[i] != NULL) {
            ESP_LOGI(TAG, "gethostbyname: %s resolved to: %s", host,
#if defined(CONFIG_LWIP_IPV6) && defined(CONFIG_LWIP_IPV4)
                     res->h_addrtype == AF_INET ? inet_ntoa(*(struct in_addr *) (res->h_addr_list[i])) :
                     inet6_ntoa(*(struct in6_addr *) (res->h_addr_list[i]))
#elif defined(CONFIG_LWIP_IPV6)
                     inet6_ntoa(*(struct in6_addr *) (res->h_addr_list[i]))
#else
                     inet_ntoa(*(struct in_addr *) (res->h_addr_list[i]))
#endif
                    );
            i++;
        }
    }
}

/**
 *  @brief Executes getaddrinfo and displays list of resolved addresses.
 *  Note: This function is used only to test advertised mdns hostnames resolution
 */
static void  query_mdns_host_with_getaddrinfo(char *host)
{
    struct addrinfo hints;
    struct addrinfo *res;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if (!getaddrinfo(host, NULL, &hints, &res)) {
        while (res) {
            char *resolved_addr;
#if defined(CONFIG_LWIP_IPV6) && defined(CONFIG_LWIP_IPV4)
            resolved_addr = res->ai_family == AF_INET ?
                            inet_ntoa(((struct sockaddr_in *) res->ai_addr)->sin_addr) :
                            inet6_ntoa(((struct sockaddr_in6 *) res->ai_addr)->sin6_addr);
#elif defined(CONFIG_LWIP_IPV6)
            resolved_addr = inet6_ntoa(((struct sockaddr_in6 *) res->ai_addr)->sin6_addr);
#else
            resolved_addr = inet_ntoa(((struct sockaddr_in *) res->ai_addr)->sin_addr);
#endif // CONFIG_LWIP_IPV6
            ESP_LOGI(TAG, "getaddrinfo: %s resolved to: %s", host, resolved_addr);
            res = res->ai_next;
        }
    }
}
#endif

/** Generate host name based on sdkconfig, optionally adding a portion of MAC address to it.
 *  @return host name string allocated from the heap
 */
static char *generate_hostname(void)
{
#ifndef CONFIG_MDNS_ADD_MAC_TO_HOSTNAME
    return strdup(CONFIG_MDNS_HOSTNAME);
#else
    uint8_t mac[6] = {0};
    char   *hostname = NULL;

    // 读取Wi-Fi STA的MAC地址
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (-1 == asprintf(&hostname, "%s-%02X%02X%02X", 
            CONFIG_MDNS_HOSTNAME, mac[3], mac[4], mac[5])) 
    {
        abort();
    }
    return hostname;
#endif
}

static void mdns_print_results(mdns_result_t *results)
{
    // mdns_result_t 是一个链表
    mdns_result_t *r = results;
    mdns_ip_addr_t *a = NULL;
    int i = 1, t = 0;
    while (r) 
    {
        if (r->esp_netif) 
        {
            printf("%d: Interface: %s, Type: %s, TTL: %" PRIu32 "\n", 
                   i++, 
                   esp_netif_get_ifkey(r->esp_netif),
                   ip_protocol_str[r->ip_protocol], 
                   r->ttl);
        }
        if (r->instance_name) 
        {
            printf("  PTR : %s.%s.%s\n", r->instance_name, r->service_type, r->proto);
        }
        if (r->hostname) 
        {
            printf("  SRV : %s.local:%u\n", r->hostname, r->port);
        }
        if (r->txt_count) 
        {
            printf("  TXT : [%zu] ", r->txt_count);
            for (t = 0; t < r->txt_count; t++) 
            {
                printf("%s=%s(%d); ", 
                       r->txt[t].key, 
                       r->txt[t].value ? r->txt[t].value : "NULL", 
                       r->txt_value_len[t]);
            }
            printf("\n");
        }

        // mdns_ip_addr_t 是一个链表
        a = r->addr;
        while (a) 
        {
            if (a->addr.type == ESP_IPADDR_TYPE_V6) 
            {
                printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
            } 
            else 
            {
                printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
            }
            // 移到下一项
            a = a->next;
        }

        // 移到下一项
        r = r->next;
    }
}

static void initialise_mdns(void)
{
    // 创建主机名，使用完需要释放内存
    char *hostname = generate_hostname();

    // initialize mDNS
    ESP_ERROR_CHECK(mdns_init());

    // set mDNS hostname (required if you want to advertise services)
    ESP_ERROR_CHECK(mdns_hostname_set(hostname));

    ESP_LOGI(TAG, "mdns hostname set to: %s", hostname);
    //set default mDNS instance name
    ESP_ERROR_CHECK(mdns_instance_name_set(EXAMPLE_MDNS_INSTANCE));

    //structure with TXT records
    mdns_txt_item_t serviceTxtData[3] = {
        {"board", "esp32"},
        {"u", "user"},
        {"p", "password"}
    };

    //initialize service
    size_t num_items = sizeof(serviceTxtData) / sizeof(serviceTxtData[0]);
    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer", 
                                     "_http", 
                                     "_tcp", 
                                     80, 
                                     serviceTxtData, 
                                     num_items));
    ESP_ERROR_CHECK(mdns_service_subtype_add_for_host("ESP32-WebServer", 
                                                       "_http", 
                                                       "_tcp", 
                                                       NULL, 
                                                       "_server"));
#if CONFIG_MDNS_MULTIPLE_INSTANCE
    // 宏定义了多个实例，在实例上新建服务
    ESP_ERROR_CHECK(mdns_service_add("ESP32-WebServer1", 
                                      "_http", 
                                      "_tcp", 
                                      80, 
                                      NULL, 
                                      0));
#endif

#if CONFIG_MDNS_PUBLISH_DELEGATE_HOST
    char *delegated_hostname = NULL;
    if (-1 == asprintf(&delegated_hostname, "%s-delegated", hostname)) 
    {
        abort();
    }

    mdns_ip_addr_t addr4, addr6;
    esp_netif_str_to_ip4("10.0.0.1", &addr4.addr.u_addr.ip4);
    addr4.addr.type = ESP_IPADDR_TYPE_V4;
    esp_netif_str_to_ip6("fd11:22::1", &addr6.addr.u_addr.ip6);
    addr6.addr.type = ESP_IPADDR_TYPE_V6;
    addr4.next = &addr6;
    addr6.next = NULL;
    ESP_ERROR_CHECK( mdns_delegate_hostname_add(delegated_hostname, &addr4) );
    ESP_ERROR_CHECK( mdns_service_add_for_host("test0", 
                                               "_http", 
                                               "_tcp", 
                                               delegated_hostname, 
                                               1234, 
                                               serviceTxtData, 
                                               3));
    free(delegated_hostname);
#endif // CONFIG_MDNS_PUBLISH_DELEGATE_HOST

    /* 注意：以下两个TXT 会增加到 ESP32-WebServer1 实例 */
    //add another TXT item
    ESP_ERROR_CHECK(mdns_service_txt_item_set("_http", 
                                              "_tcp", 
                                              "path", 
                                              "/foobar"));
    //change TXT item value
    ESP_ERROR_CHECK(mdns_service_txt_item_set_with_explicit_value_len("_http", 
                                                                      "_tcp", 
                                                                      "u", 
                                                                      "admin", 
                                                                      strlen("admin")));
    free(hostname);
}

static void query_mdns_service(const char *service_name, const char *proto)
{
    ESP_LOGI(TAG, "Query PTR: %s.%s.local", service_name, proto);

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_query_ptr(service_name, proto, 3000, 20,  &results);
    if (err) 
    {
        ESP_LOGE(TAG, "Query Failed: %s", esp_err_to_name(err));
        return;
    }
    if (!results) 
    {
        ESP_LOGW(TAG, "No results found!");
        return;
    }

    mdns_print_results(results);
    mdns_query_results_free(results); // 需要用户释放内存
}

#if CONFIG_MDNS_PUBLISH_DELEGATE_HOST
static void lookup_mdns_delegated_service(const char *service_name, const char *proto)
{
    ESP_LOGI(TAG, "Lookup delegated service: %s.%s.local", service_name, proto);

    mdns_result_t *results = NULL;
    esp_err_t err = mdns_lookup_delegated_service(NULL, service_name, proto, 20, &results);
    if (err) {
        ESP_LOGE(TAG, "Lookup Failed: %s", esp_err_to_name(err));
        return;
    }
    if (!results) {
        ESP_LOGW(TAG, "No results found!");
        return;
    }

    mdns_print_results(results);
    mdns_query_results_free(results);
}
#endif // CONFIG_MDNS_PUBLISH_DELEGATE_HOST

static void lookup_mdns_selfhosted_service(const char *service_name, const char *proto)
{
    ESP_LOGI(TAG, "Lookup selfhosted service: %s.%s.local", service_name, proto);
    mdns_result_t *results = NULL;
    esp_err_t err = mdns_lookup_selfhosted_service(NULL, service_name, proto, 20, &results);
    if (err) {
        ESP_LOGE(TAG, "Lookup Failed: %s", esp_err_to_name(err));
        return;
    }
    if (!results) {
        ESP_LOGW(TAG, "No results found!");
        return;
    }
    mdns_print_results(results);
    mdns_query_results_free(results);
}

static bool check_and_print_result(mdns_search_once_t *search)
{
    // Check if any result is available
    mdns_result_t *result = NULL;
    if (!mdns_query_async_get_results(search, 0, &result, NULL)) {
        return false;
    }

    if (!result) {   // search timeout, but no result
        return true;
    }

    // If yes, print the result
    mdns_ip_addr_t *a = result->addr;
    while (a) {
        if (a->addr.type == ESP_IPADDR_TYPE_V6) {
            printf("  AAAA: " IPV6STR "\n", IPV62STR(a->addr.u_addr.ip6));
        } else {
            printf("  A   : " IPSTR "\n", IP2STR(&(a->addr.u_addr.ip4)));
        }
        a = a->next;
    }
    // and free the result
    mdns_query_results_free(result);
    return true;
}

static void query_mdns_hosts_async(const char *host_name)
{
    ESP_LOGI(TAG, "Query both A and AAA: %s.local", host_name);

    mdns_search_once_t *s_a = NULL;
    mdns_search_once_t *s_aaaa = NULL;

    // 异步查询主机名的A记录和AAAA记录，即IPv4地址和IPv6地址
    s_a = mdns_query_async_new(host_name, NULL, NULL, MDNS_TYPE_A, 1000, 1, NULL);
    s_aaaa = mdns_query_async_new(host_name, NULL, NULL, MDNS_TYPE_AAAA, 1000, 1, NULL);
    while (s_a || s_aaaa) 
    {
        if (s_a && check_and_print_result(s_a)) 
        {
            ESP_LOGI(TAG, "Query A %s.local finished", host_name);
            mdns_query_async_delete(s_a); // 需要用户释放内存
            s_a = NULL;
        }
        if (s_aaaa && check_and_print_result(s_aaaa)) 
        {
            ESP_LOGI(TAG, "Query AAAA %s.local finished", host_name);
            mdns_query_async_delete(s_aaaa);
            s_aaaa = NULL;
        }
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

#ifdef CONFIG_LWIP_IPV4
static void query_mdns_host(const char *host_name)
{
    ESP_LOGI(TAG, "Query A: %s.local", host_name);

    struct esp_ip4_addr addr;
    addr.addr = 0;

    // 查询mdns的A记录，即IPv4地址
    esp_err_t err = mdns_query_a(host_name, 2000, &addr);
    if (err) 
    {
        if (err == ESP_ERR_NOT_FOUND) 
        {
            ESP_LOGW(TAG, "%s: Host was not found!", esp_err_to_name(err));
            return;
        }
        ESP_LOGE(TAG, "Query Failed: %s", esp_err_to_name(err));
        return;
    }

    ESP_LOGI(TAG, "Query A: %s.local resolved to: " IPSTR, host_name, IP2STR(&addr));
}
#endif // CONFIG_LWIP_IPV4

static void initialise_button(void)
{
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_DISABLE; // 禁用中断
    io_conf.pin_bit_mask = BIT64(EXAMPLE_BUTTON_GPIO);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = 1;
    io_conf.pull_down_en = 0;
    gpio_config(&io_conf);
}

static void check_button(void)
{
    static bool old_level = true;
    bool new_level = gpio_get_level(EXAMPLE_BUTTON_GPIO);
    if (!new_level && old_level) 
    {
        query_mdns_hosts_async("esp32-mdns");

#ifdef CONFIG_LWIP_IPV4
        // query_mdns_host("caojun-NMH-WCX9");
        query_mdns_host("esp32");
#endif
        // 查询服务
        query_mdns_service("_arduino", "_tcp");
        query_mdns_service("_http", "_tcp");
        query_mdns_service("_printer", "_tcp");
        query_mdns_service("_ipp", "_tcp");
        query_mdns_service("_afpovertcp", "_tcp");
        query_mdns_service("_smb", "_tcp");
        query_mdns_service("_ftp", "_tcp");
        query_mdns_service("_nfs", "_tcp");
        query_mdns_service("_home-assistant", "_tcp");

#if CONFIG_MDNS_PUBLISH_DELEGATE_HOST
        lookup_mdns_delegated_service("_http", "_tcp");
#endif // CONFIG_MDNS_PUBLISH_DELEGATE_HOST

        lookup_mdns_selfhosted_service("_http", "_tcp");
    }
    old_level = new_level;
}

static void mdns_example_task(void *pvParameters)
{
#if CONFIG_MDNS_RESOLVE_TEST_SERVICES == 1
    /* Send initial queries that are started by CI tester */
#ifdef CONFIG_LWIP_IPV4
    query_mdns_host("tinytester");
#endif
    query_mdns_host_with_gethostbyname("tinytester-lwip.local");
    query_mdns_host_with_getaddrinfo("tinytester-lwip.local");
#endif // CONFIG_MDNS_RESOLVE_TEST_SERVICES

    while (1) 
    {
        check_button();
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    initialise_mdns();

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());
    

#if defined(CONFIG_MDNS_ADD_CUSTOM_NETIF) && \
    !defined(CONFIG_MDNS_PREDEF_NETIF_STA) && \
    !defined(CONFIG_MDNS_PREDEF_NETIF_ETH)
    /* Demonstration of adding a custom netif to mdns service, but we're adding the default example one,
     * so we must disable all predefined interfaces (PREDEF_NETIF_STA, AP and ETH) first
     */
    ESP_ERROR_CHECK(mdns_register_netif(EXAMPLE_INTERFACE));
    /* It is not enough to just register the interface, we have to enable is manually.
     * This is typically performed in "GOT_IP" event handler, but we call it here directly
     * since the `EXAMPLE_INTERFACE` netif is connected already, to keep the example simple.
     */
    ESP_ERROR_CHECK(mdns_netif_action(EXAMPLE_INTERFACE, 
                        MDNS_EVENT_ENABLE_IP4 | MDNS_EVENT_ENABLE_IP6));
    ESP_ERROR_CHECK(mdns_netif_action(EXAMPLE_INTERFACE, 
                        MDNS_EVENT_ANNOUNCE_IP4 | MDNS_EVENT_ANNOUNCE_IP6));

#if defined(CONFIG_MDNS_RESPOND_REVERSE_QUERIES)
    ESP_ERROR_CHECK(mdns_netif_action(EXAMPLE_INTERFACE, 
                        MDNS_EVENT_IP4_REVERSE_LOOKUP | MDNS_EVENT_IP6_REVERSE_LOOKUP));
#endif

#endif // CONFIG_MDNS_ADD_CUSTOM_NETIF

    initialise_button();
    xTaskCreate(&mdns_example_task, "mdns_example_task", 2048, NULL, 5, NULL);
}