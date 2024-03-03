#ifndef WIFI_H
#define WIFI_H

#include "esp_event.h"
#include "esp_wifi.h"

#define AP_SSID CONFIG_AP_SSID
#define AP_PASSPHRASE CONFIG_AP_PASS
#define WIFI_TAG "wifi"
#define MAX_SSID_LEN 32
#define MAX_PASSWORD_LEN 64

// Used to store the list of available WiFi networks
typedef struct {
    char** ssid_list; 
    int size;
} ssid_list_t;

typedef struct {
    char* station_ssid;
    char* ap_ssid;
    char* station_ip;
    char* ap_ip;
    char* mac_address;
    char* mode;
    char* ap_passkey;
} network_info_t;

/**
 * @brief Event handler for WiFi events
 * 
 * @param arg - user data
 * @param event_base - event base
 * @param event_id - event id
 * @param event_data - event data
 */
void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

/**
 * @brief Initialize the WiFi station with the given SSID and password
 * 
 * @param ssid 
 * @param password 
 */
void wifi_init_sta(char* ssid, char* password);

/**
 * @brief Initialize the WiFi access point
 * 
 */
void wifi_init_softap(char* ssid, char* password);

/**
 * @brief Initialize the Network Stack, generate default WiFi configuration, and start the WiFi driver
 * 
 */
void init_wifi(void);

/**
 * @brief Stop broadcasting the WiFi access point
 * 
 */
void stop_wifi_ap(void);

/**
 * @brief Save the WiFi credentials to the Non-Volatile Storage (NVS)
 * 
 * @param ssid 
 * @param password 
 */
void save_wifi_credentials_to_nvs(char *ssid, char *password);

/**
 * @brief Reconnect to the WiFi network
 * 
 * @param pvParameters 
 */
void wifi_reconnect(void *pvParameters);

/**
 * @brief Get the wifi mode object
 * 
 * @return wifi_mode_t 
 */
wifi_mode_t get_wifi_mode(void);

/**
 * @brief Get the list of available WiFi networks
 * 
 * @return ssid_list_t* - list of available WiFi networks
*/
ssid_list_t* get_wifi_networks(void);

/**
 * @brief Get the MAC address of the ESP32
 * 
 * @return char* - MAC address of the ESP32
 */
char* get_mac_addr(void);

/**
 * @brief Get the IP address of the ESP32
 * 
 * @return char* - IP address of the ESP32
 */
char** get_sta_ap_ip(void);

/**
 * @brief Get the Wi-Fi mode of the ESP32
 * 
 * @return char* - mode of the ESP32
 */
char* get_mode(void);

network_info_t* get_network_info(void);
esp_err_t fetch_sta_credentials_from_nvs(char *ssid, char* passphrase);
void save_ap_wifi_credentials_to_nvs(char *ssid, char *password);

#endif