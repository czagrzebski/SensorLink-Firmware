#ifndef WIFI_H
#define WIFI_H

#include "esp_wifi.h"
#include "esp_event.h"

#define AP_SSID CONFIG_AP_SSID
#define AP_PASSPHRASE CONFIG_AP_PASS
#define WIFI_TAG "wifi"

void wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void wifi_init_sta(void);
void wifi_init_softap(void);

#endif