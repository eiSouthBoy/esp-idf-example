/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

static const char *TAG = "mqtt_example";
static bool g_mqtt_connected = false;
// #define _SUSPEND_GPIO GPIO_NUM_4
#define _SUSPEND_GPIO GPIO_NUM_32

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;
    int msg_id = 0;

    switch ((esp_mqtt_event_id_t)event_id) 
    {
    case MQTT_EVENT_CONNECTED: // 连接
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        g_mqtt_connected = true;

        /* 向Topic: /topic/qos1 发布一条消息。qos=0,retained=1 */
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos1", "data_3", 0, 1, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

        /* 订阅Topic: /topic/qos0 和 /topic/qos1 */
        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos1", 1);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

        /* 取消订阅Topic: /topic/qos1 */
        // msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos1");
        // ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED: // 断开
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        g_mqtt_connected = false;
        break;

    case MQTT_EVENT_SUBSCRIBED: // 订阅
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);

        /* 向Topic: /topic/qos0 发布一条消息 */
        // msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        // ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED: // 取消订阅
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED: // 发布
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA: // 消息
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("topic: %.*s | qos: %d | retained: %s \r\n", 
                event->topic_len, 
                event->topic, 
                event->qos, 
                event->retain ? "true":"false");
        printf("payload: %.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR: // 错误
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) 
        {
            log_error_if_nonzero("reported from esp-tls", 
                    event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", 
                    event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  
                    event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", 
                    strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

void task_check_gpio_level(void *pvPram)
{
    int _suspend_level = 0;
    int msg_id = 0;
    esp_mqtt_client_handle_t client = (esp_mqtt_client_handle_t)pvPram;
    gpio_set_level(_SUSPEND_GPIO, GPIO_MODE_INPUT);
    while (1)
    {
        _suspend_level = gpio_get_level(_SUSPEND_GPIO);
        ESP_LOGI(TAG, "gpio%d level: %d", _SUSPEND_GPIO, _suspend_level);
        if (g_mqtt_connected)
        {
            char topic[64] = {0};
            char payload[64] = {0};
            snprintf(payload, sizeof(payload), "{\"gpio%d\": %d}",
                     _SUSPEND_GPIO, _suspend_level);
            snprintf(topic, sizeof(topic), "/gpio/level%d", _SUSPEND_GPIO);
            msg_id = esp_mqtt_client_publish(client, topic, payload, strlen(payload), 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d, topic: %s, payload: %s", 
                     msg_id, topic, payload);
        }
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}

static void mqtt_app_start(void)
{
    /* 从Kconfig配置文件获取 broker url */
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,
    };
    /* 或者由用户从 stdin 获取 broker url */
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.broker.address.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    /* 使用默认配置项，初始化MQTT客户端 */
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    /* 注册MQTT事件回调函数，最后一个参数可以作为回调函数mqtt_event_handler()的参数传入 */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);

    // 启动MQTT客户端
    esp_mqtt_client_start(client);

    xTaskCreate(task_check_gpio_level, "task A", 2048, (void *)client, 10, NULL);
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    mqtt_app_start();
}
