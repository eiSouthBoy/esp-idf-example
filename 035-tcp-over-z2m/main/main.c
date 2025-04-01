#include <stdio.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "esp_wifi.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "esp_netif.h"
#include "lwip/netifapi.h"

#include "driver/uart.h"
#include "driver/gpio.h"

/* TCP Server 配置项 */
#define PORT                CONFIG_EXAMPLE_PORT // 监听的端口
#define KEEPALIVE_IDLE      CONFIG_EXAMPLE_KEEPALIVE_IDLE
#define KEEPALIVE_INTERVAL  CONFIG_EXAMPLE_KEEPALIVE_INTERVAL
#define KEEPALIVE_COUNT     CONFIG_EXAMPLE_KEEPALIVE_COUNT

/* 串口配置信息 */
#define ECHO_TEST_TXD (16)
#define ECHO_TEST_RXD (17)
#define ECHO_TEST_RTS (-1)
#define ECHO_TEST_CTS (-1)

#define ECHO_UART_PORT_NUM      (2)
#define ECHO_UART_BAUD_RATE     (115200)
#define ECHO_TASK_STACK_SIZE    (2048)

static const char *TAG = "tcp_over_z2m";
static int sock = -1;

static void echo_task(void *arg);

/* 数据接收和回传 */
static void do_retransmit(const int sock)
{
    int len = 0;
    char rx_buffer[128] = {0};

    do 
    {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) 
        {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } 
        else if (len == 0) 
        {
            ESP_LOGW(TAG, "Connection closed");
        } 
        else 
        {
            rx_buffer[len] = '\0'; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG, "TCP Recv %d bytes: %s", len, rx_buffer);

            // send() can return less bytes than supplied length.
            // Walk-around for robust implementation.
            // int to_write = len;

            uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) rx_buffer, len);

#if 0            
            while (to_write > 0) 
            {
                int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                if (written < 0) 
                {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                    // Failed to retransmit, giving up
                    return;
                }
                to_write -= written;
            }
#endif
        }
    } while (len > 0);
}

static void tcp_server_task(void *pvParameters)
{
    char addr_str[128] = {0};
    int addr_family = (int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;

// 设置IPv4 TCP Server
#ifdef CONFIG_EXAMPLE_IPV4
    if (addr_family == AF_INET) 
    {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(PORT);
        ip_protocol = IPPROTO_IP;
    }
#endif

// 设置IPv6 TCP Server
#ifdef CONFIG_EXAMPLE_IPV6
    if (addr_family == AF_INET6) 
    {
        struct sockaddr_in6 *dest_addr_ip6 = (struct sockaddr_in6 *)&dest_addr;
        bzero(&dest_addr_ip6->sin6_addr.un, sizeof(dest_addr_ip6->sin6_addr.un));
        dest_addr_ip6->sin6_family = AF_INET6;
        dest_addr_ip6->sin6_port = htons(PORT);
        ip_protocol = IPPROTO_IPV6;
    }
#endif

    // 创建套接字
    int listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (listen_sock < 0) 
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    // 设置套接字属性
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

#if defined(CONFIG_EXAMPLE_IPV4) && defined(CONFIG_EXAMPLE_IPV6)
    // Note that by default IPV6 binds to both protocols, it is must be disabled
    // if both protocols used at the same time (used in CI)
    setsockopt(listen_sock, IPPROTO_IPV6, IPV6_V6ONLY, &opt, sizeof(opt));
#endif

    ESP_LOGI(TAG, "Socket created");
    // 套接字绑定地址
    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) 
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", addr_family);
        goto CLEAN_UP;
    }
    ESP_LOGI(TAG, "Socket bound, port %d", PORT);

    // 监听客户端连接
    err = listen(listen_sock, 1);
    if (err != 0) 
    {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1)
    {
        ESP_LOGI(TAG, "Socket listening");

        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);

        // 阻塞在accept()，除非tcp server 与 tcp client 完成了三次握手。
        sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) 
        {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }   

        // Set tcp keepalive option
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
        setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
        // Convert ip address to string
#ifdef CONFIG_EXAMPLE_IPV4
        if (source_addr.ss_family == PF_INET) 
        {
            inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, 
                        addr_str, sizeof(addr_str) - 1);
        }
#endif

#ifdef CONFIG_EXAMPLE_IPV6
        if (source_addr.ss_family == PF_INET6) 
        {
            inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, 
                          addr_str, sizeof(addr_str) - 1);
        }
#endif

        ESP_LOGI(TAG, "Socket accepted ip address: %s", addr_str);

        
        do_retransmit(sock);

        // 关闭客户端套接字
        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);    
}

/**
 * This is an example which echos any data it receives on configured UART back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: configured UART
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below (See Kconfig)
 */

#define BUF_SIZE (1024)

static void echo_task(void *arg)
{
    // uint32_t count = 5000;
    // while (count)
    // {
    //     if (sock >= 0)
    //         break;
    //     vTaskDelay(pdMS_TO_TICKS(100));
    // }
    // if (sock < 0)
    // {
    //     ESP_LOGE(TAG, "Error: sock < 0");
    //     vTaskDelete(NULL);
    //     return;
    // }

    /* Configure parameters of an UART driver,
     * communication pins and install the driver */
    uart_config_t uart_config = {
        .baud_rate = ECHO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = 4,
    };
    int intr_alloc_flags = 0;

#if CONFIG_UART_ISR_IN_IRAM
    intr_alloc_flags = ESP_INTR_FLAG_IRAM;
#endif

    ESP_ERROR_CHECK(uart_driver_install(ECHO_UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(ECHO_UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(ECHO_UART_PORT_NUM, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS));

    // Configure a temporary buffer for the incoming data
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    if (NULL == data)
    {
        ESP_LOGE(TAG, "Error: Unable to malloc");
        vTaskDelete(NULL);
        return;
    }
    
    ESP_LOGI(TAG, "UART Listen...");
    while (1) 
    {
        // Read data from the UART
        int len = uart_read_bytes(ECHO_UART_PORT_NUM, data, (BUF_SIZE - 1), 20 / portTICK_PERIOD_MS);
        // Write data back to the UART

#if 0        
        uart_write_bytes(ECHO_UART_PORT_NUM, (const char *) data, len);
#endif
        if (len) 
        {
            data[len] = '\0';
            ESP_LOGI(TAG, "UART%d Recv %d bytes: %s", 
                    ECHO_UART_PORT_NUM, len, (char *) data);
            send(sock, data, len, 0);
        }
        
    }
    free(data);
}

void app_main(void)
{
    // FLASH 初始化
    ESP_ERROR_CHECK(nvs_flash_init());
    // TCP/IP 协议栈初始化
    ESP_ERROR_CHECK(esp_netif_init());
    // 创建默认事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 网络连接，通过STA连接到路由器 或 通过ETH连接到路由器。
    ESP_ERROR_CHECK(example_connect());

// 创建TCP Server 任务
#ifdef CONFIG_EXAMPLE_IPV4
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, NULL);
    xTaskCreate(echo_task, "uart_echo_task", ECHO_TASK_STACK_SIZE, NULL, 10, NULL);
    
#endif

#ifdef CONFIG_EXAMPLE_IPV6
    xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET6, 5, NULL);
#endif    
}