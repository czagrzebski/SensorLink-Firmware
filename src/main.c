/**
 * @file main.c
 * @author Creed Zagrzebski (czagrzebski@gmail.com)
 * @brief Core application logic for ESP32 SensorLink 
 * @version 0.1
 * @date 2023-12-23
 * 
 * @copyright Creed Zagrzebski (c) 2023
 * 
 */

#include "main.h"
#include "nvs_flash.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "string.h"
#include "led_strip.h"
#include "stdlib.h"
#include "esp_event.h"
#include "esp_spiffs.h"
#include <esp_http_server.h>
#include "esp_adc_cal.h"
#include "driver/adc.h"
#include "wifi.h"
#include "web.h"

#include "lwip/err.h"
#include "lwip/sys.h"

const char* TAG = "main";

led_strip_handle_t configure_led(void) {
  
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,  
        .max_leds = 1,      
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, 
        .led_model = LED_MODEL_WS2812,            
        .flags.invert_out = false,                
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
#endif
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

void led_status_task(void *pvParameter) {
    led_strip_handle_t led_strip = (led_strip_handle_t) pvParameter;

    // Check network status
    while(1) {
        if(get_wifi_mode() == WIFI_MODE_STA) {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 0, 255, 0));
        } else {
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 0, 0, 255));
        }
        ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void init_spiffs() {
    ESP_LOGI(TAG, "Initializing NVS flash memory...");
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // If the flash memory is full, erase it and try again
        ESP_LOGE(TAG, "NVS flash memory is full, erasing and trying again...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    // Check if the NVS library was initialized successfully
    ESP_ERROR_CHECK(ret);

    // Initialize Serial Peripheral Interface Flash File System (SPIFFS)
    esp_vfs_spiffs_conf_t config = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    esp_vfs_spiffs_register(&config);
}


void heap_monitor_task(void *pvParameter) {
    while(1) {
        ESP_LOGI("Heapmon", "Free heap: %d", (int) esp_get_free_heap_size());
        vTaskDelay(30000 / portTICK_PERIOD_MS);
    }
}


void setup_io() {
    // Initialize the ADC with default configuration and calibrate it
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);

    // Configure the ADC channel with the default configuration width
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    
    // Configure the ADC0 channel with the default configuration attenuation
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11));

    // Configure the GPIO pin as an input/output pin
    ESP_ERROR_CHECK(gpio_set_direction(OUTPUT_GPIO_PIN, GPIO_MODE_INPUT_OUTPUT)); 
   
}

// Main application entry point
void app_main() {
    ESP_LOGI(TAG, "Hello, from ESP32 SensorLink!");
    
    // Initialize the Serial Peripheral Interface Flash File System (SPIFFS)
    init_spiffs();

    // Initialize the LED strip
    led_strip_handle_t led_strip = configure_led();

    // Set Color to Red for Initialization
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, 255, 0, 0));
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));

    // Initialize the event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default()); 
    
    // Initialize the WiFi connection and connect to the network
    init_wifi();

    // Start the web server
    ESP_LOGI(TAG, "Starting the web     // build entire json string firstserver...");
    httpd_handle_t server = start_webserver();
    if(server == NULL) {
        ESP_LOGE(TAG, "Failed to start the web server!");
        return;
    }
    ESP_LOGI(TAG, "Web server started successfully!");

    // Setup the GPIO pins
    setup_io();

    // Create a task to broadcast a random value every second 
    xTaskCreate(broadcast_adc_values, "broadcast_adc_values", 4096, NULL, 5, NULL);

    // Create a task to monitor the free heap size
    xTaskCreate(heap_monitor_task, "heap_monitor_task", 2048, NULL, 5, NULL);

    // Start LED status task
    xTaskCreate(led_status_task, "led_status_task", 2048, led_strip, 5, NULL);

    // Wait for the web server to be stopped
    while(server != NULL) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // Unmount the SPIFFS
    ESP_LOGI(TAG, "Unmounting the SPIFFS...");
    esp_vfs_spiffs_unregister(NULL);

    ESP_LOGI(TAG, "Goodbye!");
}