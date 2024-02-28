#include "wifi.h"

#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "string.h"

// Interfaces
esp_netif_t *sta_netif;
esp_netif_t *ap_netif;

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(WIFI_TAG, "Connected to Station Network. Stopping AP Server...");
        // Stop the AP Mode since we're connected to the Station Network
        stop_wifi_ap();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        xTaskCreate(&wifi_reconnect, "wifi_reconnect", 4096, NULL, 5, NULL);
        if(get_wifi_mode() == WIFI_MODE_STA) {
            // Start the AP Mode since we're disconnected from the Station Network
            wifi_init_softap();
        }
    }
}

char** get_sta_ap_ip(void) {
    char** ip = (char**)malloc(2 * sizeof(char*));
    ip[0] = (char*)malloc(16 * sizeof(char));
    ip[1] = (char*)malloc(16 * sizeof(char));

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(sta_netif, &ip_info);
    esp_ip4addr_ntoa(&ip_info.ip, ip[0], 16);
    esp_netif_get_ip_info(ap_netif, &ip_info);
    esp_ip4addr_ntoa(&ip_info.ip, ip[1], 16);
    return ip;
}

char* get_mode(void) {
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    if (mode == WIFI_MODE_STA) {
        return "Station";
    } else if (mode == WIFI_MODE_AP) {
        return "Access Point";
    } else if (mode == WIFI_MODE_APSTA) {
        return "Station and Access Point";
    } else {
        return "Unknown";
    }
}

// Function to get all the Wi-Fi networks in the area. Return a char* array of SSIDs
char** get_wifi_networks(void) {
    // Get the Wi-Fi networks in the area
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    uint16_t ap_num = 0;
    esp_wifi_scan_get_ap_num(&ap_num);
    wifi_ap_record_t *ap_records = (wifi_ap_record_t *)malloc(ap_num * sizeof(wifi_ap_record_t));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));

    char** ssids = (char**)malloc(ap_num * sizeof(char*));
    for (int i = 0; i < ap_num; i++) {
        ssids[i] = (char*)malloc(33 * sizeof(char));
        strcpy(ssids[i], (char*)ap_records[i].ssid);
    }

    free(ap_records);
    return ssids;
}

void wifi_reconnect(void *pvParameters) {
    ESP_LOGI(WIFI_TAG, "Reconnecting to Station Network...");
    vTaskDelay(pdMS_TO_TICKS(10000)); 
    esp_wifi_connect();
    vTaskDelete(NULL); 
}

wifi_mode_t get_wifi_mode(void) {
    // Get the current Wi-Fi mode (STA or AP) from wifi driver
    wifi_mode_t mode;
    esp_wifi_get_mode(&mode);
    return mode;
}

void save_wifi_credentials_to_nvs(char *ssid, char *password) {
    // Save Wi-Fi Configuration to NVS and restart
    nvs_handle_t nvs_handle;
    esp_err_t err;
    err = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) opening NVS handle", esp_err_to_name(err));
        return;
    }

    // Save the Wi-Fi Configuration to NVS
    err = nvs_set_str(nvs_handle, "ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) saving ssid to NVS", esp_err_to_name(err));
    }

    err = nvs_set_str(nvs_handle, "password", password);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) saving password to NVS", esp_err_to_name(err));
    }

    // Commit the changes to NVS
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) committing changes to NVS", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
}

esp_err_t fetch_credentials_from_nvs(char *ssid, char* passphrase) {
    size_t required_size;
    nvs_handle_t nvs_handle;
    esp_err_t err;

    // Open the NVS Namespace
    ESP_LOGI(WIFI_TAG, "Opening NVS Namespace");
    err = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) opening NVS handle", esp_err_to_name(err));
        return ESP_FAIL;
    } 

    // Get the size of the SSID
    err = nvs_get_str(nvs_handle, "ssid", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(WIFI_TAG, "Error (%s) reading ssid from NVS", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // Allocate memory for the SSID
    ssid = malloc(required_size);

    // Read the stored Wi-Fi credentials from NVS
    err = nvs_get_str(nvs_handle, "ssid", ssid, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) reading ssid from NVS", esp_err_to_name(err));
    }

    ESP_LOGI(WIFI_TAG, "Retrieving Password");
    err = nvs_get_str(nvs_handle, "password", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(WIFI_TAG, "Error (%s) reading passphrase from NVS", esp_err_to_name(err));
    }

    passphrase = malloc(required_size);

    err = nvs_get_str(nvs_handle, "password", passphrase, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) reading passphrase from NVS", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
 
    ESP_LOGI(WIFI_TAG, "%s %s", ssid, passphrase);

    return ESP_OK;
}

void init_wifi() {
    // Get Wi-Fi Configuration from NVS
    wifi_mode_t mode;
    char* ssid = NULL;
    char* passphrase = NULL;

    if(fetch_credentials_from_nvs(ssid, passphrase) == ESP_OK) {
        ESP_LOGI(WIFI_TAG, "Retrieved Wi-Fi Configuration from NVS");
        mode = WIFI_MODE_STA;
    } else {
        ESP_LOGI(WIFI_TAG, "No Wi-Fi Configuration found in NVS. Starting AP Mode...");
        mode = WIFI_MODE_AP;
    }

    // Generic Wi-Fi Configuration
    ESP_LOGI(WIFI_TAG, "Initializing the TCP/IP stack...");
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    sta_netif = esp_netif_create_default_wifi_ap();
    ap_netif = esp_netif_create_default_wifi_sta();
    
    // Allocate resources for the default WiFi driver. Initiates a task for the WiFi driver
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    // If we're in AP mode, start the softAP
    if (mode == WIFI_MODE_AP) {
        wifi_init_softap();
    } else if (mode == WIFI_MODE_STA) {
        wifi_init_sta(ssid, passphrase);
    }

    free(ssid);
    free(passphrase);
}

char* get_mac_addr(void) {
    char* mac_addr_str = (char*)malloc(18 * sizeof(char));
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    sprintf(mac_addr_str, "%02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return mac_addr_str;

}

void wifi_init_sta(char* ssid, char* password) {
    ESP_LOGI(WIFI_TAG, "Initializing Station Mode...");

    // Wifi config with auto connect
    wifi_config_t wifi_sta_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };

    // Set the SSID and Password for the Station
    strcpy((char*)wifi_sta_config.sta.ssid, ssid);
    strcpy((char*)wifi_sta_config.sta.password, password);

    ESP_LOGI(WIFI_TAG, "Setting WiFi Station configuration SSID");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));

    ESP_LOGI(WIFI_TAG, "Starting WiFi...");
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");
}

void wifi_init_softap(void) {
    ESP_LOGI(WIFI_TAG, "Initializing SoftAP Mode...");

    // Initializer and register event handler for the default network interface
    // Creates a network interface instance by binding the Wi-Fi driver and TCP/IP stack

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

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_LOGI(WIFI_TAG, "wifi_init_softap finished.");
}

// Stop the Wi-Fi Access Point (AP) mode
void stop_wifi_ap(void) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
}