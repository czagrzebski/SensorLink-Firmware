/**
 * @file main.c
 * @author Creed Zagrzebski (czagrzebski@gmail.com)
 * @brief Core application logic for ESP32 SensorLink 
 * @version 0.1
 * @date 2023-12-23
 * 
 * @copyright Creed Zagrzebski (c) 2023
 * 
 */

#include "nvs_flash.h"
#include "sdkconfig.h"
#include "esp_log.h"
#include "string.h"
#include "stdlib.h"
#include "esp_event.h"
#include <esp_http_server.h>
#include "wifi.h"

#include "lwip/err.h"
#include "lwip/sys.h"

const char* TAG = "main";

esp_err_t hello_handler(httpd_req_t *req) {
    const char resp[] = "Hello World!";
    ESP_LOGI(TAG, "Request received!");
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* URI handler structure for GET /uri */
httpd_uri_t uri_get = {
    .uri      = "/uri",
    .method   = HTTP_GET,
    .handler  = hello_handler,
    .user_ctx = NULL
};

// Start the web server
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if(httpd_start(&server, &config) == ESP_OK) {
        // Register URI handlers
        httpd_register_uri_handler(server, &uri_get);
        return server;
    }

    return NULL;
}

void app_main() {
    // Initialize the NVS flash memory
    ESP_LOGI(TAG, "Initializing NVS flash memory...");
    esp_err_t ret = nvs_flash_init();
    if(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // If the flash memory is full, erase it and try again
        ESP_LOGE(TAG, "NVS flash memory is full, erasing and trying again...");
        nvs_flash_erase();
        ret = nvs_flash_init();
    }
    // Check if the NVS library was initialized successfully
    ESP_ERROR_CHECK(ret);

    // Initialize the WiFi Access Point
    wifi_init_softap();

    // Start the web server
    ESP_LOGI(TAG, "Starting the web server...");
    httpd_handle_t server = start_webserver();
    if(server == NULL) {
        ESP_LOGE(TAG, "Failed to start the web server!");
        return;
    }
    ESP_LOGI(TAG, "Web server started successfully!");

    // Wait for the web server to be stopped
    while(server != NULL) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}