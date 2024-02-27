#ifndef WIFI_H
#define WIFI_H

#include "esp_event.h"
#include "esp_wifi.h"

#define AP_SSID CONFIG_AP_SSID
#define AP_PASSPHRASE CONFIG_AP_PASS
#define WIFI_TAG "wifi"

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void wifi_init_sta(char* ssid, char* password);
void wifi_init_softap(void);
void init_wifi(void);
void stop_wifi_ap(void);
void save_wifi_credentials_to_nvs(char *ssid, char *password);
void wifi_reconnect(void *pvParameters);
wifi_mode_t get_wifi_mode(void);
char** get_wifi_networks(void);

#endif