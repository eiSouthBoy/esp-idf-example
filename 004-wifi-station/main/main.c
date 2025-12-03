/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "iot_button.h"
#include "esp_common.h"

#define EXAMPLE_ESP_WIFI_SSID      CONFIG_ESP_WIFI_SSID // WiFI的SSID
#define EXAMPLE_ESP_WIFI_PASS      CONFIG_ESP_WIFI_PASSWORD // WiFi的密码
#define EXAMPLE_ESP_MAXIMUM_RETRY  CONFIG_ESP_MAXIMUM_RETRY // 重连最大次数
#define EXAMPLE_WIFI_RSSI_THRESHOLD CONFIG_EXAMPLE_WIFI_RSSI_THRESHOLD // RSSI

#define EXAMPLE_BUTTON_GPIO CONFIG_BUTTON_GPIO_NUM

/* 加密模式 */
#if CONFIG_ESP_WIFI_AUTH_OPEN
   #define EXAMPLE_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_OPEN
#elif CONFIG_ESP_WIFI_AUTH_WEP
   #define EXAMPLE_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WEP
#elif CONFIG_ESP_WIFI_AUTH_WPA_PSK
   #define EXAMPLE_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_PSK
   #define EXAMPLE_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA_WPA2_PSK
   #define EXAMPLE_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA_WPA2_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA3_PSK
   #define EXAMPLE_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WPA2_WPA3_PSK
   #define EXAMPLE_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WPA2_WPA3_PSK
#elif CONFIG_ESP_WIFI_AUTH_WAPI_PSK
   #define EXAMPLE_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD WIFI_AUTH_WAPI_PSK
#endif

/* WPA3 SAE Mode and Password Identifier */
#if CONFIG_ESP_WPA3_SAE_PWE_HUNT_AND_PECK
   #define EXAMPLE_ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HUNT_AND_PECK
   #define EXAMPLE_H2E_IDENTIFIER ""
#elif CONFIG_ESP_WPA3_SAE_PWE_HASH_TO_ELEMENT
   #define EXAMPLE_ESP_WIFI_SAE_MODE WPA3_SAE_PWE_HASH_TO_ELEMENT
   #define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#elif CONFIG_ESP_WPA3_SAE_PWE_BOTH
   #define EXAMPLE_ESP_WIFI_SAE_MODE WPA3_SAE_PWE_BOTH
   #define EXAMPLE_H2E_IDENTIFIER CONFIG_ESP_WIFI_PW_ID
#endif

/* 事件群组允许每个事件使用多个bit, 但是我们仅关注两个事件： 
   - 当连接到AP，并分配到IP地址
   - 当重连最大次数后，还是连接失败
*/
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t g_wifi_event_group;

static int g_retry_num = 0;
static uint8_t g_wifi_connected = 0;

#if EXAMPLE_WIFI_RSSI_THRESHOLD
static void event_handler_rssi_low(void* arg, 
                                   esp_event_base_t event_base,
		                             int32_t event_id, 
                                   void* event_data)
{
	wifi_event_bss_rssi_low_t *event = event_data;

	ESP_LOGI("WIFI_EVENT_STA_BSS_RSSI_LOW", "bss rssi: %" PRIi32 "", event->rssi);
   // esp_wifi_set_rssi_threshold(EXAMPLE_WIFI_RSSI_THRESHOLD);
}
#endif

static void event_handler(void *arg, 
                          esp_event_base_t event_base, 
                          int32_t event_id, 
                          void *event_data)
{
   if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
   {
      // WiFi 启动成功，STA 去连接 AP
      ESP_LOGI("WIFI_EVENT_STA_START", "esp_wifi_connect()");
      esp_wifi_connect();
   }
   else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
   {
      // WiFi 连接失败，尝试重连
      g_wifi_connected = 0;
      ESP_LOGI("WIFI_EVENT_STA_DISCONNECTED", "connect to the AP fail");
      if (g_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY)
      {
         esp_wifi_connect();
         g_retry_num++;
         ESP_LOGI(TAG, "retry to connect to the AP");
      }
      else
      {
         xEventGroupSetBits(g_wifi_event_group, WIFI_FAIL_BIT);
      }

      wifi_event_sta_disconnected_t *sta_disconnect_evt = (wifi_event_sta_disconnected_t *)event_data;
      ESP_LOGI(TAG, "---> wifi disconnect reason: %d <---", sta_disconnect_evt->reason);
   }
   else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED)
   {
#if EXAMPLE_WIFI_RSSI_THRESHOLD
		if (EXAMPLE_WIFI_RSSI_THRESHOLD) 
      {
			ESP_LOGI("WIFI_EVENT_STA_CONNECTED", "setting rssi threshold as %d", EXAMPLE_WIFI_RSSI_THRESHOLD);
			esp_wifi_set_rssi_threshold(EXAMPLE_WIFI_RSSI_THRESHOLD);
		}
#endif      
   }
   else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
   {
      ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
      ESP_LOGI("IP_EVENT_STA_GOT_IP", "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
      g_retry_num = 0;
      xEventGroupSetBits(g_wifi_event_group, WIFI_CONNECTED_BIT);
      g_wifi_connected = 1;
   }
}

static void wifi_init_sta(void)
{
   g_wifi_event_group = xEventGroupCreate();

   // 初始化 TCP/IP 协议栈
   ESP_ERROR_CHECK(esp_netif_init());
   // 创建默认事件循环
   ESP_ERROR_CHECK(esp_event_loop_create_default());

   // 使用默认WiFi STA配置创建 esp_netif 对象，将netif连接到WiFi，并注册默认WiFi处理程序
   esp_netif_create_default_wifi_sta();

   // 创建默认WiFi配置，接着执行WiFi驱动初始化
   wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
   ESP_ERROR_CHECK(esp_wifi_init(&cfg));

   // 获取MAC地址
   uint8_t mac_addr[6];
   ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA, mac_addr));
   ESP_LOGI(TAG, "MAC Address: %02x:%02x:%02x:%02x:%02x:%02x",
      mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);

   esp_event_handler_instance_t instance_any_id;
   esp_event_handler_instance_t instance_got_ip;

   // 监听 WIFI_EVENT 的任意事件（ESP_EVENT_ANY_ID），触发事件后，进入回调函数
   ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
   // 监听 IP_EVENT 的 “IP_EVENT_STA_GOT_IP” 事件，触发事件后，进入回调函数
   ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));
   #if EXAMPLE_WIFI_RSSI_THRESHOLD
   ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, 
                                                       WIFI_EVENT_STA_BSS_RSSI_LOW,
				                                           &event_handler_rssi_low, 
                                                       NULL,
                                                       NULL));
   #endif

   // 通过Kconfig 配置，自定义 WIFI 的配置项
   wifi_config_t wifi_config = {
      .sta = {
           .ssid = EXAMPLE_ESP_WIFI_SSID,
           .password = EXAMPLE_ESP_WIFI_PASS,
           .threshold.authmode = EXAMPLE_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD,
           .sae_pwe_h2e = EXAMPLE_ESP_WIFI_SAE_MODE,
           .sae_h2e_identifier = EXAMPLE_H2E_IDENTIFIER,
      },
   };

   // 设置 WiFi 工作模式为 STA
   ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
   // 设置 STA 的配置
   ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
   // 启动 WiFi
   ESP_ERROR_CHECK(esp_wifi_start());

   ESP_LOGI(TAG, "wifi_init_sta finished");

   EventBits_t bits = xEventGroupWaitBits(g_wifi_event_group,
                                          WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                          pdFALSE,
                                          pdFALSE,
                                          portMAX_DELAY);
   
   if (bits & WIFI_CONNECTED_BIT)
   {
      ESP_LOGI(TAG, "connected to ap SSID: %s password: %s",
               EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
   }
   else if (bits & WIFI_FAIL_BIT)
   {
      ESP_LOGI(TAG, "Failed to connect to SSID: %s, password: %s",
               EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
   }
   else
   {
      ESP_LOGE(TAG, "UNEXPECTED EVENT");
   }
}

/* 按键事件（BUTTON_PRESS_DOWN）回调函数 */
static void button_press_down_event_cb(void *arg, void *data)
{
   iot_button_print_event((button_handle_t)arg);
   ESP_LOGI(TAG, "Button Presss Down");
   if (!g_wifi_connected)
   {
      ESP_LOGI(TAG, "wifi sta don't connect target ap");
      return;
   }
   
   // 获取已连接AP信息
   wifi_ap_record_t ap_info = {0};
   esp_wifi_sta_get_ap_info(&ap_info);
   ESP_LOGI(TAG, "get ap info, ssid: %s, rssi: %d", ap_info.ssid, ap_info.rssi);
}

/* 按键初始化操作 */
static void app_driver_button_init(void)
{
   // create gpio button
   esp_err_t err;
   button_config_t btn_cfg = {
       .type = BUTTON_TYPE_GPIO,
       .long_press_time = 5000,
       .gpio_button_config = {
           .gpio_num = EXAMPLE_BUTTON_GPIO,
           .active_level = 0,
       },
   };
   button_handle_t btn = iot_button_create(&btn_cfg);
   assert(btn);
   /* 根据不同的按键事件注册回调 */
   err = iot_button_register_cb(btn, BUTTON_PRESS_DOWN, button_press_down_event_cb, NULL);

   ESP_ERROR_CHECK(err);
}

void app_main(void)
{
   // NVS 初始化
   esp_err_t ret = nvs_flash_init();
   if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
   {
      // NVS 全部檫除，接着重新初始化
      ESP_LOGI(TAG, "nvs flash erase and init");
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
   }
   ESP_ERROR_CHECK(ret);

   // Button 初始化
   app_driver_button_init();

   char automode[34] = {0};
   esp_wifi_authmode_get(EXAMPLE_ESP_WIFI_SCAN_AUTH_MODE_THRESHOLD, automode);
   ESP_LOGI(TAG, "ESP WIFI MODE: Station");
   ESP_LOGI(TAG, "SSID: %s, Password: %s, Auth Mode: %s",
            EXAMPLE_ESP_WIFI_SSID, 
            EXAMPLE_ESP_WIFI_PASS, 
            automode);
   wifi_init_sta();
}