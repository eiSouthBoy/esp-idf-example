#include <stdio.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "driver/gpio.h"

// #include "protocol_examples_common.h"


#define UART0 0
#define UART1 1
#define UART2 2
#define BUF_SIZE (1024)

static const char *TAG = "tcp_over_z2m";

typedef struct {
    int from_uart;
    int to_uart;
    char tag[20];
} uart_forward_params_t;

void uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        // .rx_flow_ctrl_thresh = 122,
        .source_clk = 4,
    };

    // Configure UART0
    ESP_ERROR_CHECK(uart_driver_install(UART0, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART0, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART0, GPIO_NUM_1, GPIO_NUM_3, -1, -1));
    ESP_LOGI(TAG, "UART%d init completed, TX_GPIO_NUM: %d, RX_GPIO_NUM: %d",
             UART0, GPIO_NUM_1, GPIO_NUM_3);

    // Configure UART2
    ESP_ERROR_CHECK(uart_driver_install(UART2, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART2, GPIO_NUM_16, GPIO_NUM_17, -1, -1));
    // ESP_ERROR_CHECK(uart_set_pin(UART2, GPIO_NUM_17, GPIO_NUM_16, -1, -1));
    ESP_LOGI(TAG, "UART%d init completed, TX_GPIO_NUM: %d, RX_GPIO_NUM: %d",
             UART2, GPIO_NUM_16, GPIO_NUM_17);
}

void forward_data(void* parameters)
{
    uart_forward_params_t* params = (uart_forward_params_t*)parameters;
    // uint8_t data[BUF_SIZE] = {0};
    size_t len = 0;
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    ESP_LOGI(TAG, "task tag: %s", params->tag);
    while(1) 
    {
        // Read data from the source UART
        len = uart_read_bytes(params->from_uart, data, BUF_SIZE - 1, 20 / portTICK_PERIOD_MS);
        if(len > 0) 
        {
            // ESP_LOGI(params->tag, "UART%d Recv(%d): %s", params->from_uart, len, data);

            // Write data to the destination UART
            // ESP_LOGI(params->tag, "UART%d Send(%d): %s", params->to_uart, len, data);
            uart_write_bytes(params->to_uart, data, len);
        }
        // Small delay to prevent flooding the CPU
        // vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Create a parameter structure for each task
uart_forward_params_t uart0_to_uart2_params = {UART0, UART2, "UART0_to_UART2"};
uart_forward_params_t uart2_to_uart0_params = {UART2, UART0, "UART2_to_UART0"};

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());

    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());
    // ESP_ERROR_CHECK(example_connect());

    uart_init();

    // Create tasks for bidirectional forwarding
    xTaskCreate(forward_data, "UART0_to_UART2", 2048, (void*)&uart0_to_uart2_params, 10, NULL);
    xTaskCreate(forward_data, "UART2_to_UART0", 2048, (void*)&uart2_to_uart0_params, 10, NULL); 
}