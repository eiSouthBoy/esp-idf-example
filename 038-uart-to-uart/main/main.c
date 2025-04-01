#include "esp_log.h"
#include "zb_uart.h"

static const char *TAG = "uart-to-uart";

void app_main(void)
{
    ESP_LOGI(TAG, "@@@@@@ APP Start @@@@@@");

    app_driver_uart_init();
}