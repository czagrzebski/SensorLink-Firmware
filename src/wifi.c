#include "wifi.h"

#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_event.h"
#include "string.h"

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(void) {
    // Initialize the TCP/IP stack
    ESP_LOGI(WIFI_TAG, "Initializing the TCP/IP stack...");

    // Run IwIP initialization and create IwIP task (IPv4 Address Resolution Protocol)
    // Also known as the TCP/IP Task
    ESP_ERROR_CHECK(esp_netif_init());

    // Create an event loop to capture system events
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initializer and register event handler for the default network interface
    // Creates a network interface instance by binding the Wi-Fi driver and TCP/IP stack
    esp_netif_t *interface = esp_netif_create_default_wifi_ap();

    // Allocate resources for the default WiFi driver. Initiates a task for the WiFi driver
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

    ESP_LOGI(WIFI_TAG, "Setting WiFi AP configuration SSID");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_LOGI(WIFI_TAG, "Starting WiFi Access Point...");
    ESP_ERROR_CHECK(esp_wifi_start());
}