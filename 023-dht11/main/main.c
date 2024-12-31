#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "dht.h"

static const char *TAG = "DHT11";

void app_main(void)
{
	int16_t humidity = 0; 
	int16_t temperature = 0;
	while (1)
	{
		dht_read_data(DHT_TYPE_DHT11, GPIO_NUM_4, &humidity, &temperature);
		ESP_LOGI(TAG, "humidity: %d.%d %%, temperature: %d.%d °C", 
				 humidity / 10, humidity % 10, 
				 temperature / 10, temperature % 10);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}