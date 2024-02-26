#ifndef WIFI_H
#define WIFI_H

#include "esp_event.h"

#define AP_SSID CONFIG_AP_SSID
#define AP_PASSPHRASE CONFIG_AP_PASS
#define WIFI_TAG "wifi"

typedef enum {
    AP_MODE = 1,
    STA_MODE = 2
} wifi_mode;

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void wifi_init_sta(char* ssid, char* password);
void wifi_init_softap(void);
void init_wifi(void);
void stop_wifi_ap(void);
void save_wifi_credentials(char *ssid, char *password);

#endif