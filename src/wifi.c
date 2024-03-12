#include "wifi.h"

#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "string.h"
#include "lwip/inet.h"

// Interfaces
esp_netif_t *sta_netif;
esp_netif_t *ap_netif;

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(WIFI_TAG, "STA started. Attempting to connect to a network.");
        esp_wifi_connect();
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(WIFI_TAG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_STA_CONNECTED) {
        ESP_LOGI(WIFI_TAG, "Connected to Station Network. Stopping AP Server...");
        // Stop the AP Mode since we're connected to the Station Network
        ip_config_t* ip_config = fetch_ip_info_from_nvs();
        if(ip_config->mode == STATIC) {
            set_ip_configuration(ip_config->ip, ip_config->gateway, ip_config->netmask);
        } else {
            ESP_LOGI(WIFI_TAG, "DHCP Mode. Not setting IP Configuration");
        }
        free(ip_config->ip);
        free(ip_config->gateway);
        free(ip_config->netmask);
        free(ip_config);
        stop_wifi_ap();
    } else if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* event = (wifi_event_sta_disconnected_t*) event_data;
        xTaskCreate(&wifi_reconnect, "wifi_reconnect", 4096, NULL, 5, NULL);
        ESP_LOGI(WIFI_TAG, "Disconnect Event");
        if(get_wifi_mode() == WIFI_MODE_STA) {
            start_wifi_ap();
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

// Set DHCP or Static IP Address
esp_err_t set_ip_configuration(char *ip, char* gateway, char* netmask) {
    esp_netif_ip_info_t ip_info;
    memset(&ip_info, 0, sizeof(esp_netif_ip_info_t));
    ip_info.ip.addr = ipaddr_addr(ip);
    ip_info.gw.addr = ipaddr_addr(gateway);
    ip_info.netmask.addr = ipaddr_addr(netmask);
    if(esp_netif_dhcpc_stop(sta_netif) != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Failed to stop DHCP Client");
        return ESP_FAIL;
    }
    if (esp_netif_set_ip_info(sta_netif, &ip_info) != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Failed to set IP Address");
        return ESP_FAIL;
    }

    ESP_LOGI(WIFI_TAG, "Successfully set IPv4 Configuration with IP: %s, Gateway: %s, Netmask: %s", ip, gateway, netmask);
    return ESP_OK;
}

// Function to get all the Wi-Fi networks in the area. Return a char* array of SSIDs
ssid_list_t* get_wifi_networks(void) {

    // Get the Wi-Fi networks in the area
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = false
    };
    
    // Start the scanning process
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    // Get the number of APs found by the scan
    uint16_t num_ap; 
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&num_ap));

    // Allocate a list of wifi_ap
    wifi_ap_record_t *ap_records = (wifi_ap_record_t*) malloc(sizeof(wifi_ap_record_t) * num_ap);

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&num_ap, ap_records));

    char** ssid_names = malloc(sizeof(char*) * num_ap);

    for(int i=0; i < num_ap; i++) {
        ESP_LOGI(WIFI_TAG, "SSID: %s", ap_records[i].ssid);
        ssid_names[i] = (char*)malloc(33 * sizeof(char)); 
        strcpy(ssid_names[i], (char*)ap_records[i].ssid);
    }

    ssid_list_t* ssid_list = (ssid_list_t*) malloc(sizeof(ssid_list_t));
    ssid_list->size = num_ap;
    ssid_list->ssid_list = ssid_names;
    free(ap_records);
    return ssid_list;
}

void wifi_reconnect(void *pvParameters) {
    ESP_LOGI(WIFI_TAG, "Network Error. Attempting to reconnect to Network...");
    vTaskDelay(pdMS_TO_TICKS(30000));  // wait for 1 minute
    esp_wifi_connect(); // attempt to connect to network. if this fails, it will fire a event to connect again. else, it will fire a success event.
    vTaskDelete(NULL);  // destroy current rtos task
}


void save_ip_info_to_nvs(char* static_ip, char* gateway, char* subnet, uint8_t mode) {
    esp_err_t err;
    nvs_handle_t nvs_handle;
    err = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) opening NVS handle", esp_err_to_name(err));
        return;
    }

    err = nvs_set_str(nvs_handle, "static_ip", static_ip);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) saving static ip to NVS", esp_err_to_name(err));
    }

    err = nvs_set_str(nvs_handle, "gateway", gateway);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) saving gateway to NVS", esp_err_to_name(err));
    }

    err = nvs_set_str(nvs_handle, "netmask", subnet);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) saving netmask to NVS", esp_err_to_name(err));
    }

    err = nvs_set_u8(nvs_handle, "mode", mode);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) saving mode to NVS", esp_err_to_name(err));
    }

    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) committing changes to NVS", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
}

ip_config_t* fetch_ip_info_from_nvs(void) {
    esp_err_t err;
    nvs_handle_t nvs_handle;
    ip_config_t *ip_config = (ip_config_t*) malloc(sizeof(ip_config_t));

    memset(ip_config, 0, sizeof(ip_config_t));
    ip_config->mode = DHCP;
    ip_config->gateway = NULL;
    ip_config->ip = NULL;
    ip_config->netmask = NULL;


    err = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) opening NVS handle", esp_err_to_name(err));
    }

    size_t required_size;
    err = nvs_get_str(nvs_handle, "static_ip", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(WIFI_TAG, "Error (%s) reading ip from NVS", esp_err_to_name(err));
    }

    if (err == ESP_OK) {
        ip_config->ip = (char*)malloc(required_size * sizeof(char));
        err = nvs_get_str(nvs_handle, "static_ip", ip_config->ip, &required_size);
        if (err != ESP_OK) {
            ESP_LOGE(WIFI_TAG, "Error (%s) reading ip from NVS", esp_err_to_name(err));
        }
    }

    err = nvs_get_str(nvs_handle, "gateway", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(WIFI_TAG, "Error (%s) reading gateway from NVS", esp_err_to_name(err));
    }

    if (err == ESP_OK) {
        ip_config->gateway = (char*)malloc(required_size * sizeof(char));
        err = nvs_get_str(nvs_handle, "gateway", ip_config->gateway, &required_size);
        if (err != ESP_OK) {
            ESP_LOGE(WIFI_TAG, "Error (%s) reading gateway from NVS", esp_err_to_name(err));
        }
    }

    err = nvs_get_str(nvs_handle, "netmask", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(WIFI_TAG, "Error (%s) reading netmask from NVS", esp_err_to_name(err));
    }

    if (err == ESP_OK) {
        ip_config->netmask = (char*)malloc(required_size * sizeof(char));
        err = nvs_get_str(nvs_handle, "netmask", ip_config->netmask, &required_size);
        if (err != ESP_OK) {
            ESP_LOGE(WIFI_TAG, "Error (%s) reading netmask from NVS", esp_err_to_name(err));
        }
    }

    // Get Mode
    if(ip_config->ip == NULL || ip_config->gateway == NULL || ip_config->netmask == NULL) {
        ip_config->mode = DHCP;
    } else {
        err = nvs_get_u8(nvs_handle, "mode", (uint8_t*) &ip_config->mode);
        if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(WIFI_TAG, "Error (%s) reading mode from NVS", esp_err_to_name(err));
        }
    }

    nvs_close(nvs_handle);
    return ip_config;
}

network_info_t* get_network_info(void) {
    network_info_t* net_info = (network_info_t*) malloc(sizeof(network_info_t));
    esp_netif_ip_info_t ip_info;
    wifi_config_t* sta_conf;
    wifi_config_t* ap_conf;
    esp_err_t err;

    sta_conf = (wifi_config_t*) malloc(sizeof(wifi_config_t));
    ap_conf = (wifi_config_t*) malloc(sizeof(wifi_config_t));

    err = esp_wifi_get_config(WIFI_IF_STA, sta_conf);
    if(err != ESP_OK) {
        ESP_LOGI(WIFI_TAG, "Failed to get configuration for station interface");
    }

    err = esp_wifi_get_config(WIFI_IF_AP, ap_conf);
    if(err != ESP_OK) {
        ESP_LOGI(WIFI_TAG, "Failed to get configuration for AP interface");
    }
    
    net_info->station_ip = (char*)malloc(16 * sizeof(char));
    net_info->ap_ip = (char*)malloc(16 * sizeof(char));

    // Get AP/Station IP Addresses
    esp_netif_get_ip_info(sta_netif, &ip_info);
    esp_ip4addr_ntoa(&ip_info.ip, net_info->station_ip, 16);
    esp_netif_get_ip_info(ap_netif, &ip_info);
    esp_ip4addr_ntoa(&ip_info.ip, net_info->ap_ip, 16);

    // Get MAC Address
    net_info->mac_address = get_mac_addr();

    // Get SSID for AP/STA
    net_info->station_ssid = (char*) malloc(MAX_SSID_LEN * sizeof(char));
    strcpy(net_info->station_ssid, (char*) sta_conf->sta.ssid);

    net_info->ap_ssid = (char*) malloc(MAX_SSID_LEN * sizeof(char));
    strcpy(net_info->ap_ssid, (char*) ap_conf->ap.ssid);

    // GET AP PASSWORD
    net_info->ap_passkey = (char*) malloc(MAX_SSID_LEN * sizeof(char));
    strcpy(net_info->ap_passkey, (char*) ap_conf->ap.password);

    // Get the network adapter mode
    net_info->mode = get_mode();

    free(sta_conf);
    free(ap_conf);

    return net_info;
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

void save_ap_wifi_credentials_to_nvs(char *ssid, char *password) {
    // Save Wi-Fi Configuration to NVS and restart
    nvs_handle_t nvs_handle;
    esp_err_t err;
    err = nvs_open("wifi", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) opening NVS handle", esp_err_to_name(err));
        return;
    }

    // Save the Wi-Fi Configuration to NVS
    err = nvs_set_str(nvs_handle, "ap_ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) saving ssid to NVS", esp_err_to_name(err));
    }

    err = nvs_set_str(nvs_handle, "ap_password", password);
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

esp_err_t fetch_sta_credentials_from_nvs(char *ssid, char* passphrase) {
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

    err = nvs_get_str(nvs_handle, "password", passphrase, &required_size);
    if (err != ESP_OK) {
        ESP_LOGE(WIFI_TAG, "Error (%s) reading passphrase from NVS", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
 
    ESP_LOGI(WIFI_TAG, "%s %s", passphrase, ssid);

    return ESP_OK;
}

esp_err_t fetch_ap_credentials_from_nvs(char *ssid, char* passphrase) {
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
    err = nvs_get_str(nvs_handle, "ap_ssid", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(WIFI_TAG, "Error (%s) reading ssid from NVS", esp_err_to_name(err));
        return ESP_FAIL;
    }

    // Read the stored Wi-Fi credentials from NVS
    err = nvs_get_str(nvs_handle, "ap_ssid", ssid, &required_size);
    if (err != ESP_OK) {
        strcpy(ssid, AP_SSID);
        ESP_LOGE(WIFI_TAG, "Error (%s) reading ssid from NVS", esp_err_to_name(err));
    }

    ESP_LOGI(WIFI_TAG, "Retrieving Password");
    err = nvs_get_str(nvs_handle, "ap_password", NULL, &required_size);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(WIFI_TAG, "Error (%s) reading passphrase from NVS", esp_err_to_name(err));
    }

    err = nvs_get_str(nvs_handle, "ap_password", passphrase, &required_size);
    if (err != ESP_OK) {
        strcpy(passphrase, AP_PASSPHRASE);
        ESP_LOGE(WIFI_TAG, "Error (%s) reading passphrase from NVS", esp_err_to_name(err));
    }

    nvs_close(nvs_handle);
 
    ESP_LOGI(WIFI_TAG, "%s %s", passphrase, ssid);

    return ESP_OK;
}

int is_wifi_connected(void) {
    wifi_ap_record_t ap_info;
    esp_err_t err = esp_wifi_sta_get_ap_info(&ap_info);
    if(err == ESP_OK) {
        return 1;
    } else {
        return 0;
    }
}

void init_wifi() {
    // Get Wi-Fi Configuration from NVS
    wifi_mode_t mode;
    char* sta_ssid = NULL;
    char* sta_passphrase = NULL;
    char* ap_ssid = NULL;
    char* ap_passphrase = NULL;

    // Allocate memory for the SSID and Password
    sta_ssid = (char*) malloc((MAX_SSID_LEN + 1) * sizeof(char));
    sta_passphrase = (char*) malloc((MAX_PASSWORD_LEN + 1) * sizeof(char));
    ap_ssid = (char*) malloc((MAX_SSID_LEN + 1) * sizeof(char));
    ap_passphrase = (char*) malloc((MAX_PASSWORD_LEN + 1) * sizeof(char));

    memset(sta_ssid, 0, MAX_SSID_LEN + 1);
    memset(sta_passphrase, 0, MAX_PASSWORD_LEN + 1);
    memset(ap_ssid, 0, MAX_SSID_LEN + 1);
    memset(ap_passphrase, 0, MAX_PASSWORD_LEN + 1);

    if(fetch_ap_credentials_from_nvs(ap_ssid, ap_passphrase) == ESP_OK) {
        ESP_LOGI(WIFI_TAG, "Retrieved Wi-Fi Configuration from NVS");
        mode = WIFI_MODE_AP;
    } else {
        ESP_LOGI(WIFI_TAG, "Failed to retrieve AP Wi-Fi Configuration from NVS. Aborting..");
        return;
    }

    if(fetch_sta_credentials_from_nvs(sta_ssid, sta_passphrase) == ESP_OK) {
        ESP_LOGI(WIFI_TAG, "Retrieved Wi-Fi Configuration from NVS");
        mode = WIFI_MODE_STA;
    } else {
        ESP_LOGI(WIFI_TAG, "No Wi-Fi Configuration found in NVS. Starting AP Mode...");
        mode = WIFI_MODE_AP;
    }

    // Initialize the netif stack
    ESP_LOGI(WIFI_TAG, "Initializing the TCP/IP stack...");
    ESP_ERROR_CHECK(esp_netif_init());
    
    // Initializer and register event handler for the default network interface
    // Creates a network interface instance by binding the Wi-Fi driver and TCP/IP stack
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ap_netif = esp_netif_create_default_wifi_ap();
    sta_netif = esp_netif_create_default_wifi_sta();
    
    // Allocate resources for the default WiFi driver. Initiates a task for the WiFi driver
    wifi_init_config_t wifi_init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_config));

    wifi_init_softap(ap_ssid, ap_passphrase);
    wifi_init_sta(sta_ssid, sta_passphrase);

    // If we're in AP mode, start the softAP
    if (mode == WIFI_MODE_AP) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    } else if (mode == WIFI_MODE_STA) {
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    }

    ESP_ERROR_CHECK(esp_wifi_start());

    free(sta_ssid);
    free(sta_passphrase);
    free(ap_ssid);
    free(ap_passphrase);
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
    
    // Log length of SSID and Password
    ESP_LOGI(WIFI_TAG, "SSID Length: %d", strlen(ssid));
    ESP_LOGI(WIFI_TAG, "Password Length: %d", strlen(password));

    // If the SSID or Password is empty, set the SSID and Password to "NoNetwork"
    // Otherwise, an empty SSID or Password causes STA to hang
    if(strlen(ssid) == 0 || strlen(password) == 0) {
        strcpy((char*)wifi_sta_config.sta.ssid, "NoNetwork");
        strcpy((char*)wifi_sta_config.sta.password, "NoNetwork");
    } else {
        // Set the SSID and Password for the Station
        strcpy((char*)wifi_sta_config.sta.ssid, ssid);
        strcpy((char*)wifi_sta_config.sta.password, password);
    }

    ESP_LOGI(WIFI_TAG, "Setting WiFi Station configuration SSID");
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    ESP_LOGI(WIFI_TAG, "wifi_init_sta finished.");
}

void wifi_init_softap(char* ssid, char* password) {
    ESP_LOGI(WIFI_TAG, "Initializing SoftAP Mode...");

    // Union that contains the configuration of the WiFi Access Point
    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };

    // Set the SSID and Password for the Access Point
    strcpy((char*)wifi_config.ap.ssid, ssid);
    strcpy((char*)wifi_config.ap.password, password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_LOGI(WIFI_TAG, "wifi_init_softap finished.");
}

void start_wifi_ap(void) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
}

// Stop the Wi-Fi Access Point (AP) mode
void stop_wifi_ap(void) {
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
}