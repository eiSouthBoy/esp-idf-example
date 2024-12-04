#include <stdio.h>
#include <cJSON.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "esp_log.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"


#include "protocol_examples_common.h"
#include "esp_websocket_client.h"

#define NO_DATA_TIMEOUT_SEC 10

static const char *TAG = "websocket";
static TimerHandle_t shutdown_signal_timer;
static SemaphoreHandle_t shutdown_sema;

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) 
    {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static void shutdown_signaler(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "No data received for %d seconds, signaling shutdown", 
             NO_DATA_TIMEOUT_SEC);
    xSemaphoreGive(shutdown_sema);
}

// 若URI不是以STRING传入，则需要用户通过stdin手动输入URI
#if CONFIG_WEBSOCKET_URI_FROM_STDIN
static void get_string(char *line, size_t size)
{
    int count = 0;
    while (count < size) 
    {
        int c = fgetc(stdin);
        if (c == '\n') 
        {
            line[count] = '\0';
            break;
        } else if (c > 0 && c < 127) 
        {
            line[count] = c;
            ++count;
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}
#endif /* CONFIG_WEBSOCKET_URI_FROM_STDIN */

/* websocket 客户端事件处理 */
static void websocket_event_handler(void *handler_args, 
                                    esp_event_base_t base, 
                                    int32_t event_id, 
                                    void *event_data)
{
    esp_websocket_event_data_t *data = (esp_websocket_event_data_t *)event_data;
    switch (event_id) 
    {
    case WEBSOCKET_EVENT_BEGIN: // 客户端线程正在运行
        ESP_LOGI(TAG, "--> WEBSOCKET_EVENT_BEGIN");
        break;
    case WEBSOCKET_EVENT_CONNECTED: // 客户端已连接
        ESP_LOGI(TAG, "--> WEBSOCKET_EVENT_CONNECTED");
        break;
    case WEBSOCKET_EVENT_DISCONNECTED: // 客户端已断开
        ESP_LOGI(TAG, "--> WEBSOCKET_EVENT_DISCONNECTED");
        log_error_if_nonzero("HTTP status code",  data->error_handle.esp_ws_handshake_status_code);
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) 
        {
            log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  
                                data->error_handle.esp_transport_sock_errno);
        }
        break;
    case WEBSOCKET_EVENT_DATA: // 客户端已成功接收并解析 WebSocket 帧
        ESP_LOGI(TAG, "--> WEBSOCKET_EVENT_DATA");
        ESP_LOGI(TAG, "Received opcode=%d", data->op_code);
        if (data->op_code == 0x2)  // Opcode 0x2 indicates binary data
        { 
            ESP_LOG_BUFFER_HEX("Received binary data", data->data_ptr, data->data_len);
        } else if (data->op_code == 0x08 && data->data_len == 2) 
        {
            ESP_LOGW(TAG, "Received closed message with code=%d", 
                    256 * data->data_ptr[0] + data->data_ptr[1]);
        } else 
        {
            ESP_LOGW(TAG, "Received=%.*s", data->data_len, (char *)data->data_ptr);
        }

        // If received data contains json structure it succeed to parse
        cJSON *root = cJSON_Parse(data->data_ptr);
        if (root) 
        {
            for (int i = 0 ; i < cJSON_GetArraySize(root) ; i++) 
            {
                cJSON *elem = cJSON_GetArrayItem(root, i);
                cJSON *id = cJSON_GetObjectItem(elem, "id");
                cJSON *name = cJSON_GetObjectItem(elem, "name");
                ESP_LOGW(TAG, "Json={'id': '%s', 'name': '%s'}", 
                        id->valuestring, name->valuestring);
            }
            cJSON_Delete(root);
        }

        ESP_LOGW(TAG, "Total payload length=%d, data_len=%d, current payload offset=%d", 
                data->payload_len, data->data_len, data->payload_offset);

        // 定时器重置
        xTimerReset(shutdown_signal_timer, portMAX_DELAY);
        break;
    case WEBSOCKET_EVENT_ERROR: // 客户端遇到错误
        ESP_LOGI(TAG, "--> WEBSOCKET_EVENT_ERROR");
        log_error_if_nonzero("HTTP status code",  data->error_handle.esp_ws_handshake_status_code);
        if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) 
        {
            log_error_if_nonzero("reported from esp-tls", data->error_handle.esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", data->error_handle.esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno", 
                                data->error_handle.esp_transport_sock_errno);
        }
        break;
    case WEBSOCKET_EVENT_FINISH: // 客户端线程即将退出
        ESP_LOGI(TAG, "--> WEBSOCKET_EVENT_FINISH");
        break;
    case WEBSOCKET_EVENT_CLOSED: // 会话已被清理
        ESP_LOGI(TAG, "--> WEBSOCKET_EVENT_CLOSED");
        break;
    }
}

static void websocket_app_start(void)
{
    // 创建定时器
    shutdown_signal_timer = xTimerCreate("Websocket shutdown timer", 
                                          NO_DATA_TIMEOUT_SEC * 1000 / portTICK_PERIOD_MS,
                                          pdFALSE, 
                                          NULL, 
                                          shutdown_signaler);
    shutdown_sema = xSemaphoreCreateBinary();

    esp_websocket_client_config_t websocket_cfg = {
        .reconnect_timeout_ms = 10000,
        .network_timeout_ms = 10000,
    };

#if CONFIG_WEBSOCKET_URI_FROM_STDIN
    char line[128];

    ESP_LOGI(TAG, "Please enter uri of websocket endpoint");
    get_string(line, sizeof(line));

    websocket_cfg.uri = line;
    ESP_LOGI(TAG, "Endpoint uri: %s\n", line);
#else
    websocket_cfg.uri = CONFIG_WEBSOCKET_URI;
#endif

#if CONFIG_WS_OVER_TLS_MUTUAL_AUTH
    /* Configuring client certificates for mutual authentification */
    extern const char cacert_start[] asm("_binary_ca_cert_pem_start"); // CA certificate
    extern const char cert_start[] asm("_binary_client_cert_pem_start"); // Client certificate
    extern const char cert_end[]   asm("_binary_client_cert_pem_end");
    extern const char key_start[] asm("_binary_client_key_pem_start"); // Client private key
    extern const char key_end[]   asm("_binary_client_key_pem_end");

    websocket_cfg.cert_pem = cacert_start;
    websocket_cfg.client_cert = cert_start;
    websocket_cfg.client_cert_len = cert_end - cert_start;
    websocket_cfg.client_key = key_start;
    websocket_cfg.client_key_len = key_end - key_start;
#elif CONFIG_WS_OVER_TLS_SERVER_AUTH
    // Using certificate bundle as default server certificate source
    websocket_cfg.crt_bundle_attach = esp_crt_bundle_attach;
    // If using a custom certificate it could be added to certificate bundle, 
    // added to the build similar to client certificates in this examples, or read from NVS.

    /* extern const char cacert_start[] asm("ADDED_CERTIFICATE"); */
    /* websocket_cfg.cert_pem = cacert_start; */
#endif

#if CONFIG_WS_OVER_TLS_SKIP_COMMON_NAME_CHECK
    websocket_cfg.skip_cert_common_name_check = true;
#endif

    ESP_LOGI(TAG, "Connecting to %s ...", websocket_cfg.uri);
    // 初始化 websocket client 实例
    esp_websocket_client_handle_t client;
    client = esp_websocket_client_init(&websocket_cfg);

    // 注册并监听所有的 websocket 事件
    esp_websocket_register_events(client, 
                                  WEBSOCKET_EVENT_ANY, 
                                  websocket_event_handler, 
                                  (void *)client);

    // 打开 websocket 会话
    esp_websocket_client_start(client);

    // 启动定时器
    xTimerStart(shutdown_signal_timer, portMAX_DELAY);

    /* 发送文本数据 */ 
    char data[32];
    int i = 0;
    while (i < 5) 
    {
        if (esp_websocket_client_is_connected(client)) 
        {
            int len = sprintf(data, "hello %04d", i++);
            ESP_LOGI(TAG, "Sending %s", data);
            // 将文本数据写入 websocket。opcode=0x01
            esp_websocket_client_send_text(client, data, len, portMAX_DELAY);
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    /* Sending text data */ 
    ESP_LOGI(TAG, "Sending fragmented text message");
    memset(data, 'a', sizeof(data));
    // 向 websocket 写入文本数据并发送，不设置 FIN 标志。opcode=0x01
    esp_websocket_client_send_text_partial(client, data, sizeof(data), portMAX_DELAY);
    memset(data, 'b', sizeof(data));
    // // 发送接下来的连续文本格式帧，最后通过设置 FIN 标志，标记连续数据的结束。
    esp_websocket_client_send_cont_msg(client, data, sizeof(data), portMAX_DELAY);
    esp_websocket_client_send_fin(client, portMAX_DELAY);
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    /* Sending binary data */ 
    ESP_LOGI(TAG, "Sending fragmented binary message");
    char binary_data[5];
    memset(binary_data, 0, sizeof(binary_data));
    // 向 websocket 连续写入二进制数据并发送，不设置 FIN 标志。opcode=0x02
    esp_websocket_client_send_bin_partial(client, binary_data, sizeof(binary_data), portMAX_DELAY);
    memset(binary_data, 1, sizeof(binary_data));
    // 发送接下来的连续二进制格式帧，最后通过设置 FIN 标志，标记连续数据的结束。
    esp_websocket_client_send_cont_msg(client, binary_data, sizeof(binary_data), portMAX_DELAY);
    esp_websocket_client_send_fin(client, portMAX_DELAY);
    vTaskDelay(1000 / portTICK_PERIOD_MS);  

    /* Sending text data longer than ws buffer (default 1024) */ 
    ESP_LOGI(TAG, "Sending text longer than ws buffer (default 1024)");
    const int size = 2000;
    char *long_data = malloc(size);
    memset(long_data, 'a', size);
    esp_websocket_client_send_text(client, long_data, size, portMAX_DELAY);
    free(long_data);


    xSemaphoreTake(shutdown_sema, portMAX_DELAY);
    // 以干净的方式关闭 websocket 连接
    esp_websocket_client_close(client, portMAX_DELAY);
    ESP_LOGI(TAG, "Websocket Stopped");
    // 释放 websocket client 实例资源
    esp_websocket_client_destroy(client);  
}

void app_main(void)
{
    // 打印堆栈可用大小和IDF版本
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    // 给指定的 TAG 设置日志输出级别
    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("websocket_client", ESP_LOG_DEBUG);
    esp_log_level_set("transport_ws", ESP_LOG_DEBUG);
    esp_log_level_set("trans_tcp", ESP_LOG_DEBUG);

    // FLASH初始化
    ESP_ERROR_CHECK(nvs_flash_init());
    // TCP/IP 协议栈初始化
    ESP_ERROR_CHECK(esp_netif_init());
    // 创建默认的事件循环
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 网络连接，通过STA连接到路由器 或 通过ETH连接到路由器。
    ESP_ERROR_CHECK(example_connect());

    // 启动websocket client app
    websocket_app_start();
}