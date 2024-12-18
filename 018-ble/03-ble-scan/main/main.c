#include "esp_log.h"
#include "ble_scan.h"


static const char *TAG = "EXAMPLE_BLE_SCAN";

void app_main(void)
{
    int ret = ble_scan_init();
    if (ret)
    {
        ESP_LOGI(TAG, "ble_scan_init() failed");
    }
}
