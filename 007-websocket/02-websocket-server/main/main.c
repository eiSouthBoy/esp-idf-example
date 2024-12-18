#include <stdio.h>
#include <sys/param.h>

#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"

#include "esp_http_server.h"

static const char *TAG = "ws_echo_server";

static esp_err_t echo_handler(httpd_req_t *req);

struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};

static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = echo_handler,
        .user_ctx   = NULL,
        .is_websocket = true
};

/*首页HTML GET处理程序 */
static esp_err_t home_get_handler(httpd_req_t *req)
{
	/*获取脚本index.html的存放地址和大小，接受http请求时将脚本发出去*/
    extern const unsigned char upload_script_start[] asm("_binary_index_html_start");
    extern const unsigned char upload_script_end[]   asm("_binary_index_html_end");
    const size_t upload_script_size = (upload_script_end - upload_script_start);
    httpd_resp_send(req, (const char *)upload_script_start, upload_script_size);
    return ESP_OK;
}

/*首页HTML*/
static const httpd_uri_t home = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = home_get_handler,
    .user_ctx  = NULL
};

static void ws_async_send(void *arg)
{
    static const char * data = "Async data";
    struct async_resp_arg *resp_arg = arg;
    httpd_handle_t hd = resp_arg->hd;
    int fd = resp_arg->fd;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)data;
    ws_pkt.len = strlen(data);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;

    httpd_ws_send_frame_async(hd, fd, &ws_pkt);
    free(resp_arg);
}

static esp_err_t trigger_async_send(httpd_handle_t handle, httpd_req_t *req)
{
    struct async_resp_arg *resp_arg = malloc(sizeof(struct async_resp_arg));
    if (resp_arg == NULL) 
    {
        return ESP_ERR_NO_MEM;
    }
    resp_arg->hd = req->handle;
    resp_arg->fd = httpd_req_to_sockfd(req);
    esp_err_t ret = httpd_queue_work(handle, ws_async_send, resp_arg);
    if (ret != ESP_OK) 
    {
        free(resp_arg);
    }
    return ret;
}

static esp_err_t echo_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) 
    {
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        return ESP_OK;
    }
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len) 
    {
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) 
        {
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        }
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) 
        {
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        }
        ESP_LOGI(TAG, "Got packet with message: %s", ws_pkt.payload);
    }
    ESP_LOGI(TAG, "Packet type: %d", ws_pkt.type);
    if (ws_pkt.type == HTTPD_WS_TYPE_TEXT &&
        strcmp((char*)ws_pkt.payload,"Trigger async") == 0) 
    {
        free(buf);
        return trigger_async_send(req->handle, req);
    }

    ret = httpd_ws_send_frame(req, &ws_pkt);
    if (ret != ESP_OK) 
    {
        ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
    }
    free(buf);
    return ret;
}

/* http事件处理 */
static void ws_event_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    if (event_base == ESP_HTTP_SERVER_EVENT)
    {
        switch (event_id)
        {
        case HTTP_SERVER_EVENT_ERROR: // 当执行过程中出现错误时，将发生此事件
            break;
        case HTTP_SERVER_EVENT_START: // 此事件在HTTP服务器启动时发生
            break;
        case HTTP_SERVER_EVENT_ON_CONNECTED: // 一旦HTTP服务器连接到客户端，就不会执行任何数据交换
            break;
        case HTTP_SERVER_EVENT_ON_HEADER: // 在接收从客户端发送的每个报头时发生
            break;
        case HTTP_SERVER_EVENT_HEADERS_SENT: // 在将所有标头发送到客户端之后
            break;
        case HTTP_SERVER_EVENT_ON_DATA: // 从客户端接收数据时发生
            break;
        case HTTP_SERVER_EVENT_SENT_DATA: // 当ESP HTTP服务器会话结束时发生
            break;
        case HTTP_SERVER_EVENT_DISCONNECTED: // 连接已断开
            // esp_http_server_event_data *event = (esp_http_server_event_data *)event_data;
            // ws_client_list_delete(event->fd);
            break;
        case HTTP_SERVER_EVENT_STOP: // 当HTTP服务器停止时发生此事件
            break;
        }
    }
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) 
    {
        // Registering the ws handler
        ESP_LOGI(TAG, "Registering URI handlers");
        esp_event_handler_instance_register(ESP_HTTP_SERVER_EVENT, 
                                            ESP_EVENT_ANY_ID, 
                                            &ws_event_handler,
                                            NULL,
                                            NULL);
        httpd_register_uri_handler(server, &home);
        httpd_register_uri_handler(server, &ws);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

/* 监听 IP_EVENT_STA_GOT_IP 事件*/
static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) 
    {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    // Stop the httpd server
    return httpd_stop(server);
}

/* 监听 WIFI_EVENT_STA_DISCONNECTED 事件 */
static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) 
    {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) 
        {
            *server = NULL;
        } 
        else 
        {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

void app_main(void)
{
    static httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());

#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, 
                                               IP_EVENT_STA_GOT_IP, 
                                               &connect_handler, 
                                               &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, 
                                               WIFI_EVENT_STA_DISCONNECTED, 
                                               &disconnect_handler, 
                                               &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI

#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, 
                                               IP_EVENT_ETH_GOT_IP, 
                                               &connect_handler, 
                                               &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, 
                                               ETHERNET_EVENT_DISCONNECTED, 
                                               &disconnect_handler, 
                                               &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET


    server = start_webserver();
}