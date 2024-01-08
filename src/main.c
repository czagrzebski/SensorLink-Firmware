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

void app_main() {
    // Initialize the Serial Peripheral Interface Flash File System (SPIFFS)
    init_spiffs();

    // Initialize the WiFi Access Point
    wifi_init_softap();

    // Start the web server
    ESP_LOGI(TAG, "Starting the web server...");
    httpd_handle_t server = start_webserver();
    if(server == NULL) {
        ESP_LOGE(TAG, "Failed to start the web server!");
        return;
    }
    ESP_LOGI(TAG, "Web server started successfully!");

    // Initialize the ADC with default configuration and calibrate it
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);

    // Configure the ADC channel with the default configuration width
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    
    // Configure the ADC0 channel with the default configuration attenuation
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11));

    // Create a task to broadcast a random value every second
    xTaskCreate(broadcast_adc_values, "broadcast_adc_values", 4096, NULL, 5, NULL);

    // Wait for the web server to be stopped
    while(server != NULL) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // Unmount the SPIFFS
    ESP_LOGI(TAG, "Unmounting the SPIFFS...");
    esp_vfs_spiffs_unregister(NULL);
}