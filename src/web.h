#ifndef WEB_H
#define WEB_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_http_server.h"
#include "esp_adc_cal.h"
#include "driver/adc.h"

#define WEB_TAG "web"
#define WS_INTERVAL_MS 1000
#define JSON_BUFFER_SIZE 4096

// ADC calibration characteristics for ADC1
extern esp_adc_cal_characteristics_t adc1_chars;

// URI Handlers

/**
 * @brief Index handler (home page) "/index"
 * 
 * @param req 
 * @return esp_err_t 
 */
esp_err_t index_handler(httpd_req_t *req);

/**
 * @brief WebSocket Connection Handler. Handshakes with client. "/ws"
 * 
 * @param req 
 * @return esp_err_t 
 */
esp_err_t ws_handler(httpd_req_t *req);

/**
 * @brief Retrieves git commit hash "/version"
 * 
 * @param req 
 * @return esp_err_t 
 */
esp_err_t version_handler(httpd_req_t *req);

/**
 * @brief Dummy endpoint for testing I/O control
 * 
 * @param req 
 * @return esp_err_t 
 */
esp_err_t toggle_led_handler(httpd_req_t *req);

/**
 * @brief Serves chart.js file "/chartjs"
 * 
 * @param req 
 * @return esp_err_t 
 */
esp_err_t chart_js_handler(httpd_req_t *req);

/**
 * @brief Deprecated. Used for sending network configuration page. 
 * 
 * @param req 
 * @return esp_err_t 
 */
esp_err_t network_page_handler(httpd_req_t *req);

/**
 * @brief Configure the wifi
 * 
 * @param req 
 * @return esp_err_t 
 */
esp_err_t wifi_credential_handler(httpd_req_t *req);
esp_err_t get_all_networks_handler(httpd_req_t *req);
esp_err_t restart_esp_handler(httpd_req_t *req);

// Util Functions
char* replace_variable(char* source, char* placeholder, char* replacement);
void broadcast_adc_values(void* pvParameters);
esp_err_t httpd_ws_send_frame_to_all_clients(httpd_ws_frame_t *ws_pkt);
httpd_handle_t start_webserver(void);
esp_err_t wifi_ap_credential_handler(httpd_req_t *req);
esp_err_t wifi_ip_handler(httpd_req_t *req);

#endif