#include "esp_log.h"
// #include "nvs_flash.h"
// #include "esp_psram.h"
// #include "esp_system.h"

#include "zb_uart.h"

static const char *TAG = "uart-to-uart";

void app_main(void)
{
    ESP_LOGI(TAG, "@@@@@@ APP Start @@@@@@");
    // // NVS 初始化
    // esp_err_t err = nvs_flash_init();
    // if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) 
    // {
    //     ESP_ERROR_CHECK(nvs_flash_erase());
    //     err = nvs_flash_init();
    // }
    // ESP_ERROR_CHECK(err);

    app_driver_uart_init();
}