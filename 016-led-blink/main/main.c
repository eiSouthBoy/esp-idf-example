#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "esp_log.h"
#include "led_strip.h"

#define BLINK_GPIO CONFIG_BLINK_GPIO

static const char *TAG = "led_blink";
static uint8_t s_led_state = 0;

#ifdef CONFIG_BLINK_LED_STRIP

static led_strip_handle_t led_strip;

struct hsv_t {
    uint16_t h; 
    uint8_t s; 
    uint8_t v;
};

static struct hsv_t hsv_array[] = {
    {0, 0, 50},     // 白色
    {0, 255, 46},  // 红色
    {0, 255, 125},
    {39, 255, 46},  // 橙色
    {39, 255, 125},
    {60, 255, 46},  // 黄色
    {60, 255, 125},
    {120, 255, 46},  // 绿色
    {120, 255, 125},
    {180, 255, 46}, // 青色
    {180, 255, 125},
    {210, 255, 46}, // 蓝色
    {210, 255, 125},
    {300, 255, 46}, // 紫色 
    {300, 255, 125}, 
};

static void blink_led(void)
{
    /* If the addressable LED is enabled */
    if (s_led_state) 
    {
        /* 设置 RGB */
        ESP_LOGI(TAG, "led rgb set: 16 16 16");
        led_strip_set_pixel(led_strip, 0, 16, 16, 16);
        led_strip_refresh(led_strip);

        uint8_t cnt = sizeof(hsv_array) / sizeof(hsv_array[0]);
        for (uint8_t i = 0; i < cnt; i++)
        {
            ESP_LOGI(TAG, "led num: %d, led hsv set: %d %d %d", 
                     CONFIG_BLINK_LED_NUM, hsv_array[i].h, hsv_array[i].s, hsv_array[i].v);
            for (uint8_t idx = 0; idx < CONFIG_BLINK_LED_NUM; idx++)
            {
                led_strip_set_pixel_hsv(led_strip, idx, hsv_array[i].h, hsv_array[i].s, hsv_array[i].v);
            }
            led_strip_refresh(led_strip);
            vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
        }
    } 
    else 
    {
        /* Set all LED off to clear all pixels */
        led_strip_clear(led_strip);
    }
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink addressable LED!");
    /* LED strip initialization with the GPIO and pixels number*/
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = CONFIG_BLINK_LED_NUM, // at least one LED on board
        .led_pixel_format = LED_PIXEL_FORMAT_GRB,
        .led_model = LED_MODEL_WS2812,
    };

#if CONFIG_BLINK_LED_STRIP_BACKEND_RMT
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, // 10MHz
        .flags.with_dma = false,
    };
    // 创建 rmt 设备
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
#elif CONFIG_BLINK_LED_STRIP_BACKEND_SPI
    led_strip_spi_config_t spi_config = {
        .spi_bus = SPI2_HOST,
        .flags.with_dma = true,
    };
    // 创建 spi 设备
    ESP_ERROR_CHECK(led_strip_new_spi_device(&strip_config, &spi_config, &led_strip));
#else
    #error "unsupported LED strip backend"
#endif
    /* Set all LED off to clear all pixels */
    led_strip_clear(led_strip);
}

#elif CONFIG_BLINK_LED_GPIO

static void blink_led(void)
{
    /* Set the GPIO level according to the state (LOW or HIGH)*/
    gpio_set_level(BLINK_GPIO, s_led_state);
}

static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!, GPIO%d", BLINK_GPIO);
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}

#else
    #error "unsupported LED type"
#endif

void app_main(void)
{
    /* Configure the peripheral according to the LED type */
    configure_led();

    while (1) 
    {
        ESP_LOGI(TAG, "Turning the LED %s!", s_led_state == true ? "ON" : "OFF");
        blink_led();
        /* Toggle the LED state */
        s_led_state = !s_led_state;
        vTaskDelay(CONFIG_BLINK_PERIOD / portTICK_PERIOD_MS);
    }
}
