
/*  WiFi softAP Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "esp_mac.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define EXAMPLE_ESP_WIFI_SSID CONFIG_ESP_WIFI_SSID // 设置WiFi的SSID
#define EXAMPLE_ESP_WIFI_PASS CONFIG_ESP_WIFI_PASSWORD // 设置WiFi的密码
#define EXAMPLE_ESP_WIFI_CHANNEL CONFIG_ESP_WIFI_CHANNEL // 设置WiFi的信道
#define EXAMPLE_MAX_STA_CONN CONFIG_ESP_MAX_STA_CONN // 设置Sta的最大连接数
#define EXAMPLE_ESP_WIFI_AUTH CONFIG_SECURITY_MODE   // 设置AP的加密方式


static const char *TAG = "wifi softAP";

// WiFi 事件回调函数
static void wifi_event_handler(void* arg, 
                               esp_event_base_t event_base, 
                               int32_t event_id, 
                               void* event_data)
{
   if (event_id == WIFI_EVENT_AP_STACONNECTED) // Sta 连接到 AP
   {
      wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
      ESP_LOGI(TAG, "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
   }
   else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) // Sta 与AP 断开
   {
      wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
      ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
   }
}

void wifi_init_softap(void)
{
   // 初始化 TCP/IP 协议栈
   ESP_ERROR_CHECK(esp_netif_init());
   // 创建默认事件循环
   ESP_ERROR_CHECK(esp_event_loop_create_default());

   // 使用默认WiFi AP配置创建esp_netif对象，将netif连接到WiFi并注册默认WiFi处理程序
   esp_netif_create_default_wifi_ap();

   // 创建默认WiFi配置，接着执行WiFi驱动初始化
   wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
   ESP_ERROR_CHECK(esp_wifi_init(&cfg));

   // 获取WiFi国家码
   wifi_country_t country = {0};
   ESP_ERROR_CHECK(esp_wifi_get_country(&country));
   ESP_LOGI(TAG, "Country Code: %c%c%c, Channel Range: %d~%d", 
            country.cc[0], country.cc[1], country.cc[2], country.schan, country.nchan);

   // 监听 WIFI_EVENT 的任意事件（ESP_EVENT_ANY_ID），触发事件后，进入回调函数
   ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                       ESP_EVENT_ANY_ID,
                                                       &wifi_event_handler,
                                                       NULL,
                                                       NULL));
   // 可以通过Kconfig配置，自定义WiFi的配置项
   wifi_config_t wifi_config = {
       .ap = {
           .ssid = EXAMPLE_ESP_WIFI_SSID,
           .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
           .channel = EXAMPLE_ESP_WIFI_CHANNEL,
           .password = EXAMPLE_ESP_WIFI_PASS,
           .max_connection = EXAMPLE_MAX_STA_CONN,
           .authmode = EXAMPLE_ESP_WIFI_AUTH,
           .pmf_cfg = {
               .required = true,
           },
       },
   };
   if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0)
   {
      // 若密码长度为0，这加密模式为 OPEN
      wifi_config.ap.authmode = WIFI_AUTH_OPEN;
   }

   // 设置 WiFi 工作模式为AP
   ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
   // 设置AP的配置
   ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
   // 启动WiFi
   ESP_ERROR_CHECK(esp_wifi_start());

   ESP_LOGI(TAG, "wifi_init_softap finished. SSID: %s, password: %s, channel: %d",
            EXAMPLE_ESP_WIFI_SSID, 
            EXAMPLE_ESP_WIFI_PASS, 
            EXAMPLE_ESP_WIFI_CHANNEL);
}

void app_main(void)
{
   // NVS初始化
   esp_err_t ret = nvs_flash_init();
   if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
   {
      // NVS檫除并重新初始化
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
   }
   ESP_ERROR_CHECK(ret);

   ESP_LOGI(TAG, "Auth mode: %d, Channel: %d", EXAMPLE_ESP_WIFI_AUTH, EXAMPLE_ESP_WIFI_CHANNEL);
   ESP_LOGI(TAG, "ESP_WIFI_MODE_AP");
   wifi_init_softap();
}