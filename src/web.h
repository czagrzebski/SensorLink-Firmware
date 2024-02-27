#ifndef WEB_H
#define WEB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_adc_cal.h"
#include "driver/adc.h"

#define WEB_TAG "web"

extern esp_adc_cal_characteristics_t adc1_chars;

// URI Handlers
esp_err_t index_handler(httpd_req_t *req);
esp_err_t ws_handler(httpd_req_t *req);
esp_err_t version_handler(httpd_req_t *req);
esp_err_t toggle_led_handler(httpd_req_t *req);
esp_err_t chart_js_handler(httpd_req_t *req);
esp_err_t network_page_handler(httpd_req_t *req);
esp_err_t wifi_config_handler(httpd_req_t *req);
esp_err_t get_all_networks_handler(httpd_req_t *req);

// Util Functions
char* replace_variable(const char* source, const char* placeholder, const char* replacement);
void broadcast_adc_values(void* pvParameters);
esp_err_t httpd_ws_send_frame_to_all_clients(httpd_ws_frame_t *ws_pkt);
httpd_handle_t start_webserver(void);

#endif