#include "wifi.h"

#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
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

void init_wifi() {
    // Check NVS for Wi-Fi Configuration (Username and Password)
    // If it doesn't exist, start softAP
    // If it does exist, start station mode

    // Get Wi-Fi Configuration from NVS
    wifi_mode mode = STA_MODE;
    nvs_handle_t nvs_handle;
    esp_err_t err;
    size_t required_size;

    err = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) opening NVS handle", esp_err_to_name(err));
        mode = AP_MODE;
    } else {
        ESP_LOGI(WIFI_TAG, "Reading Wi-Fi configuration from NVS");
    }

    // Failed to get size of ssid, so we can't read it
    err = nvs_get_str(nvs_handle, "ssid", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        mode = AP_MODE;
        ESP_LOGE(WIFI_TAG, "Error (%s) reading ssid from NVS", esp_err_to_name(err));
    }

    char* ssid = malloc(required_size);
    char* password = malloc(required_size);
    
    // Read the stored Wi-Fi credentials from NVS
    err = nvs_get_str(nvs_handle, "ssid", ssid, &required_size);
    if (err != ESP_OK) {
        mode = AP_MODE;
        ESP_LOGE(WIFI_TAG, "Error (%s) reading ssid from NVS", esp_err_to_name(err));
    }
  
    err = nvs_get_str(nvs_handle, "password", password, &required_size);
    if (err != ESP_OK) {
        mode = AP_MODE;
        ESP_LOGE(WIFI_TAG, "Error (%s) reading password from NVS", esp_err_to_name(err));
    }

    // If we're in AP mode, start the softAP
    if (mode == AP_MODE) {
        wifi_init_softap();
    } else if (mode == STA_MODE) {
        //wifi_init_sta(ssid, password);
    }

    free(ssid);
    free(password);
    nvs_close(nvs_handle);

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