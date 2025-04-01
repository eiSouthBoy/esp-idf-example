#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_flash.h"
#include "esp_psram.h"
#include "esp_heap_caps.h"

static const char *TAG = "psram-usage";

void sram_print(void)
{
    // 检查SRAM大小
    // uint32_t heap_size = esp_get_minimum_free_heap_size();
    // ESP_LOGI(TAG, "--> 1) ALLRAM free heap size: %" PRIu32 " bytes", heap_size);
    uint32_t heap_size = esp_get_free_heap_size();
    ESP_LOGI(TAG, "--> ALLRAM free heap size: %" PRIu32 " bytes", heap_size);
}

void psram_print(void)
{
    // 检查PSRAM是否启动
    if (esp_psram_is_initialized())
    {
        // size_t psram_szie = esp_psram_get_size();
        // ESP_LOGI(TAG, "--> PSRAM size: %u bytes", psram_szie);

        // 获取PSRAM总的可用大小
        // size_t psram_total_size = heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
        // printf("PSRAM total size: %zu bytes\n", psram_total_size);
        sram_print();

        size_t sram_free_size = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
        ESP_LOGI(TAG, "--> SRAM free heap size: %zu bytes", sram_free_size);

        // 获取PSRAM当前未使用的大小
        size_t psram_free_size = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
        ESP_LOGI(TAG, "--> PSRAM free heap size: %zu bytes", psram_free_size);
    }
    else
    {
        ESP_LOGI(TAG, "--> PSRAM is not enabled");
    }
}

void ram_malloc(void)
{
    psram_print();
    char *data = malloc(140000 * sizeof(char));
    if (NULL == data)
    {
        ESP_LOGI(TAG, "error: malloc() failed");
        return;
    }
    ESP_LOGI(TAG, "malloc 140000 B");
    psram_print();
    free(data);
    ESP_LOGI(TAG, "free 140000 B");

    printf("\n");

    psram_print();
    char *data1 = malloc(150000 * sizeof(char));
    if (NULL == data1)
    {
        ESP_LOGI(TAG, "error: malloc(150000) failed");
        return;
    }
    ESP_LOGI(TAG, "malloc 150000 B");
    psram_print();
    free(data1);
    ESP_LOGI(TAG, "free 150000 B");

    // printf("\n");

    // psram_print();
    // char *data2 = malloc(100 * 1000 * sizeof(char));
    // if (NULL == data2)
    // {
    //     ESP_LOGI(TAG, "error: malloc() failed");
    //     return;
    // }
    // ESP_LOGI(TAG, "malloc 100KB");
    // psram_print();
    // free(data2);
    // ESP_LOGI(TAG, "free 100KB");
}

void param_malloc(void)
{
    psram_print();
    char *data = heap_caps_malloc(150000, MALLOC_CAP_SPIRAM);
    if (NULL == data)
    {
        ESP_LOGI(TAG, "error: heap_caps_malloc failed");
        return;
    }
    ESP_LOGI(TAG, "heap_caps_malloc 150000 B");
    psram_print();
    heap_caps_free(data);
    // free(data);
    ESP_LOGI(TAG, "heap_caps_free 150000 B");
}

void app_main()
{
    ESP_LOGI(TAG, "@@@@@@ APP Start @@@@@@");

    // psram_print();
    ram_malloc();
    // printf("\n");
    // param_malloc();
}