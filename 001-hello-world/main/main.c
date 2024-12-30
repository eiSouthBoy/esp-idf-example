/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_idf_version.h"

#define BUTTON_GPIO_0 0

static const char *TAG = "hello-world";

static int esp_chip_model_get(esp_chip_model_t model_num, char model_name[20])
{
    switch(model_num)
    {
    case CHIP_ESP32: //!< ESP32
        strcpy(model_name, "ESP32");
        break;
    case CHIP_ESP32S2: //!< ESP32-S2
        strcpy(model_name, "ESP32S2");
        break;
    case CHIP_ESP32S3: //!< ESP32-S3
        strcpy(model_name, "ESP32S3");
        break;
    case CHIP_ESP32C3: //!< ESP32-C3
        strcpy(model_name, "ESP32C3");
        break;
    case CHIP_ESP32C2: //!< ESP32-C2
        strcpy(model_name, "ESP32C2");
        break;
    case CHIP_ESP32C6: //!< ESP32-C6
        strcpy(model_name, "ESP32C6");
        break;
    case CHIP_ESP32H2: //!< ESP32-H2
        strcpy(model_name, "ESP32H2");
        break;
    case CHIP_ESP32P4: //!< ESP32-P4
        strcpy(model_name, "ESP32P4");
        break;
    case CHIP_POSIX_LINUX:
        strcpy(model_name, "POSIX_LINUX");
        break;
    default:
        strcpy(model_name, "unknown");
        break;
    }
    return 0;
}

static int esp_chip_reset_reason(esp_reset_reason_t reason_num, char reason_name[20])
{
    switch(reason_num)
    {
    case ESP_RST_UNKNOWN:    //!< Reset reason can not be determined
        strcpy(reason_name, "ESP_RST_UNKNOWN");
        break;
    case ESP_RST_POWERON:    //!< Reset due to power-on event
        strcpy(reason_name, "ESP_RST_POWERON");
        break;
    case ESP_RST_EXT:        //!< Reset by external pin (not applicable for ESP32)
        strcpy(reason_name, "ESP_RST_EXT");
        break;
    case ESP_RST_SW:         //!< Software reset via esp_restart
        strcpy(reason_name, "ESP_RST_SW");
        break;
    case ESP_RST_PANIC:      //!< Software reset due to exception/panic
        strcpy(reason_name, "ESP_RST_PANIC");
        break;
    case ESP_RST_INT_WDT:    //!< Reset (software or hardware) due to interrupt watchdog
        strcpy(reason_name, "ESP_RST_INT_WDT");
        break;
    case ESP_RST_TASK_WDT:   //!< Reset due to task watchdog
        strcpy(reason_name, "ESP_RST_TASK_WDT");
        break;
    case ESP_RST_WDT:        //!< Reset due to other watchdogs
        strcpy(reason_name, "ESP_RST_WDT");
        break;
    case ESP_RST_DEEPSLEEP:  //!< Reset after exiting deep sleep mode
        strcpy(reason_name, "ESP_RST_DEEPSLEEP");
        break;
    case ESP_RST_BROWNOUT:   //!< Brownout reset (software or hardware)
        strcpy(reason_name, "ESP_RST_BROWNOUT");
        break;
    case ESP_RST_SDIO:       //!< Reset over SDIO
        strcpy(reason_name, "ESP_RST_SDIO");
        break;
    case ESP_RST_USB:        //!< Reset by USB peripheral
        strcpy(reason_name, "ESP_RST_USB");
        break;
    case ESP_RST_JTAG:       //!< Reset by JTAG
        strcpy(reason_name, "ESP_RST_JTAG");
       break;
    case ESP_RST_EFUSE:      //!< Reset due to efuse error
        strcpy(reason_name, "ESP_RST_EFUSE");
        break;
    case ESP_RST_PWR_GLITCH: //!< Reset due to power glitch detected
        strcpy(reason_name, "ESP_RST_PWR_GLITCH");
        break;
    case ESP_RST_CPU_LOCKUP: //!< Reset due to CPU lock up
        strcpy(reason_name, "ESP_RST_CPU_LOCKUP");
        break;
    default:
        break;
    }
    return 0;
}

static void button_init(void)
{
    gpio_config_t io_conf = {0};
    io_conf.intr_type = GPIO_INTR_DISABLE;          // 禁用中断
    io_conf.pin_bit_mask = BIT64(BUTTON_GPIO_0);    // 选择GPIO
    io_conf.mode = GPIO_MODE_INPUT;                 // 选择输出模式
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;        // 启用上拉
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;   // 禁用下拉
    gpio_config(&io_conf);                          // 配置使能
}

static void button_check(void)
{
    static bool old_level = true; // 静态局部变量
    bool new_level = gpio_get_level(BUTTON_GPIO_0);
    if (!new_level && old_level)
    {
        esp_restart(); // 软启动ESP32系统
    }
    old_level = new_level;
}

static void esp_button_reset_task(void *pvParameters)
{
    while (1)
    {
        button_check();
        vTaskDelay(50 / portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    /* reset reason */
    char reason_name[20] = {0};
    esp_reset_reason_t reason = esp_reset_reason();
    esp_chip_reset_reason(reason, reason_name);
    ESP_LOGI(TAG, "esp reset reason: %s", reason_name);

    /* idf information */
    ESP_LOGI(TAG, "ESP-IDF VERSION: %s", esp_get_idf_version());

    /* chip information */
    char model_name[20] = {0};
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    esp_chip_model_get(chip_info.model, model_name);
    unsigned major_rev = chip_info.revision / 100;
    unsigned minor_rev = chip_info.revision % 100;
    ESP_LOGI(TAG, "This is %s chip with %d CPU core(s)\n"
        "\t2.4G WiFi: %s\n"
        "\tBT: %s\n"
        "\tBLE: %s\n"
        "\t802.15.4 (Zigbee/Thread): %s\n"
        "\tFLASH TYPE: %s\n"
        "\tPSRAM TYPE: %s\n"
        "\tsilicon revision: %d.%d\n",
        model_name,
        // CONFIG_IDF_TARGET,
        chip_info.cores,
        (chip_info.features & CHIP_FEATURE_WIFI_BGN) ? "yes":"no",
        (chip_info.features & CHIP_FEATURE_BT) ? "yes":"no",
        (chip_info.features & CHIP_FEATURE_BLE) ? "yes":"no",
        (chip_info.features & CHIP_FEATURE_IEEE802154) ? "yes":"no",
        (chip_info.features & CHIP_FEATURE_EMB_FLASH) ? "embedded":"external",
        (chip_info.features & CHIP_FEATURE_EMB_PSRAM) ? "embedded":"external",
        major_rev, minor_rev
    );

    /* flash information */
    uint32_t flash_size;
    if (ESP_OK != esp_flash_get_size(NULL, &flash_size))
    {
        ESP_LOGE(TAG, "Get flash size failed");
        return;
    }
    ESP_LOGI(TAG, "%" PRIu32 " MB flash", flash_size / (uint32_t)(1024 * 1024));

    /* heap information */
    uint32_t heap_size;
    heap_size = esp_get_minimum_free_heap_size();
    ESP_LOGI(TAG, "Minium free heap size: %" PRIu32 " bytes", heap_size);

    /* print hello world */
    ESP_LOGI(TAG, "hello world, welcome to esp32");

    /* soft restart */
    button_init();
    xTaskCreate(&esp_button_reset_task, "button_reset_task", 2048, NULL, 5, NULL);

    for (int i = 0; i < 100; i++)
    {
        ESP_LOGI(TAG, "hello world");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}