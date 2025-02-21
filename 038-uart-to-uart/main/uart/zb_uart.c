#include "zb_uart.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "driver/gpio.h"

#define UART0 0
#define UART1 1
#define UART2 2
#define BUF_SIZE (1024)

#define EXAMPLE_UART_BAUD_RATE  CONFIG_UART_BAUD_RATE
#define EXAMPLE_UART0_TXD       CONFIG_UART0_TXD
#define EXAMPLE_UART0_RXD       CONFIG_UART0_RXD
#define EXAMPLE_UART2_TXD       CONFIG_UART2_TXD
#define EXAMPLE_UART2_RXD       CONFIG_UART2_RXD

typedef struct {
    int from_uart;
    int to_uart;
    char tag[20];
} uart_forward_params_t;

static const char *TAG = "uart";

// Create a parameter structure for each task
uart_forward_params_t uart0_to_uart2_params = {UART0, UART2, "UART0_to_UART2"};
uart_forward_params_t uart2_to_uart0_params = {UART2, UART0, "UART2_to_UART0"};

static void uart_init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = EXAMPLE_UART_BAUD_RATE,
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
    ESP_ERROR_CHECK(uart_set_pin(UART0, EXAMPLE_UART0_TXD, EXAMPLE_UART0_RXD, -1, -1));
    ESP_LOGI(TAG, "UART%d init completed, TX_GPIO_NUM: %d, RX_GPIO_NUM: %d",
             UART0, EXAMPLE_UART0_TXD, EXAMPLE_UART0_RXD);

    // Configure UART2
    ESP_ERROR_CHECK(uart_driver_install(UART2, BUF_SIZE * 2, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART2, EXAMPLE_UART2_TXD, EXAMPLE_UART2_RXD, -1, -1));
    ESP_LOGI(TAG, "UART%d init completed, TX_GPIO_NUM: %d, RX_GPIO_NUM: %d",
             UART2, EXAMPLE_UART2_TXD, EXAMPLE_UART2_RXD);
}

static void forward_data(void* parameters)
{
    uart_forward_params_t* params = (uart_forward_params_t*)parameters;
    size_t len = 0;
    uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    ESP_LOGI(TAG, "task tag: %s", params->tag);
    while(1) 
    {
        len = uart_read_bytes(params->from_uart, data, BUF_SIZE - 1, 20 / portTICK_PERIOD_MS);
        if(len > 0) 
        {
            uart_write_bytes(params->to_uart, data, len);
        }
    }
}

void app_driver_uart_init(void)
{
    uart_init();
    ESP_LOGI(TAG, "--> uart0 and uart2 init completed.");
    
    // Create tasks for bidirectional forwarding
    xTaskCreate(forward_data, "UART0_to_UART2", 2048, (void*)&uart0_to_uart2_params, 10, NULL);
    xTaskCreate(forward_data, "UART2_to_UART0", 2048, (void*)&uart2_to_uart0_params, 10, NULL); 
}