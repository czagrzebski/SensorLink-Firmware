/**
 * @file main.c
 * @author Creed Zagrzebski (czagrzebski@gmail.com)
 * @brief Core application logic for SensorLink ESP32
 * @version 0.1
 * @date 2023-12-23
 * 
 * @copyright Creed Zagrzebski (c) 2023
 * 
 */

#include "nvs_flash.h"
#include "sdkconfig.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "string.h"
#include "esp_mac.h"

#define AP_SSID "ESP32"
#define AP_PASSPHRASE "123456789"

const char* TAG = "main";

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void init_wifi_ap() {
    // Initialize the TCP/IP stack 
    // Serves as an intermediary between the IO driver and network protocol stacks  
    ESP_LOGI(TAG, "Initializing the TCP/IP stack...");
    ESP_ERROR_CHECK(esp_netif_init());
    esp_netif_create_default_wifi_ap();

    // Create an event loop to handle WiFi events
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize the WiFi stack in Access Point mode with the default configuration
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    // Register the WiFi event handler to handle WiFi events
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    // Union that contains the configuration of the WiFi Access Point
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASSPHRASE,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    ESP_LOGI(TAG, "Setting WiFi AP configuration SSID");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_LOGI(TAG, "Starting WiFi Access Point...");
    ESP_ERROR_CHECK(esp_wifi_start());
}

void app_main() {
    // Initialize the NVS flash memory
    ESP_LOGI(TAG, "Initializing NVS flash memory...");
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // If the flash memory is full, erase it and try again
        ESP_LOGI(TAG, "Erasing NVS flash memory...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    // Check if the NVS library was initialized successfully
    ESP_ERROR_CHECK(ret);

    // Initialize the WiFi Access Point
    init_wifi_ap();

}