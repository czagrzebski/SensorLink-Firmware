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
#include "esp_spiffs.h"
#include <esp_http_server.h>
#include "esp_adc_cal.h"
#include "driver/adc.h"
#include "wifi.h"

#include "lwip/err.h"
#include "lwip/sys.h"

const char* TAG = "main";
static esp_adc_cal_characteristics_t adc1_chars;
httpd_handle_t server_handle = NULL;

// Define GIT_COMMIT_HASH if it's not already defined
#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "undefined"
#endif

esp_err_t ws_handler(httpd_req_t *req);
char* replace_variable(const char* source, const char* placeholder, const char* replacement);

/*
 * Structure holding server handle
 * and internal socket fd in order
 * to use out of request send
 */
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
};


// Web server handle
esp_err_t httpd_ws_send_frame_to_all_clients(httpd_ws_frame_t *ws_pkt) {
    size_t max_clients = CONFIG_LWIP_MAX_LISTENING_TCP;
    size_t fds = max_clients;
    int* client_fds = (int*)malloc(sizeof(int) * max_clients);

    esp_err_t ret = httpd_get_client_list(server_handle, &fds, client_fds);

    if (ret != ESP_OK) {
        return ret;
    }

    // Send the frame to all connected websocket clients
    for (int i = 0; i < fds; i++) {
        httpd_ws_client_info_t client_info = httpd_ws_get_fd_info(server_handle, client_fds[i]);

        if (client_info == HTTPD_WS_CLIENT_WEBSOCKET) {
            httpd_ws_send_frame_async(server_handle, client_fds[i], ws_pkt);
        }
    }

    // Free the client list
    free(client_fds);

    return ESP_OK;
}

esp_err_t index_handler(httpd_req_t *req) {
    // Open file from SPIFFS
    ESP_LOGI(TAG, "Request received!");

    FILE* file = fopen("/spiffs/index.html", "r");
    if (file == NULL) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Read the line by line, check for placeholders and replace them
    char line[1024];
    while(fgets(line, sizeof(line), file)) {
        char* newLine = replace_variable(line, "{{GIT_COMMIT_HASH}}", GIT_COMMIT_HASH);
        httpd_resp_send_chunk(req, newLine, strlen(newLine));
        free(newLine);
    }

    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0); // Finalize the response
    return ESP_OK;
}

esp_err_t chart_js_handler(httpd_req_t *req) {
    // Open file from SPIFFS
    ESP_LOGI(TAG, "Request received!");

    FILE* file = fopen("/spiffs/chart.js", "r");
    if (file == NULL) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Read the line by line, check for placeholders and replace them
    char line[1024];
    while(fgets(line, sizeof(line), file)) {
        httpd_resp_send_chunk(req, line, strlen(line));
    }

    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0); // Finalize the response
    return ESP_OK;
}

esp_err_t version_handler(httpd_req_t *req) {
    // Open file from SPIFFS
    ESP_LOGI(TAG, "Request received!");

    httpd_resp_send(req, GIT_COMMIT_HASH, strlen(GIT_COMMIT_HASH));
    return ESP_OK;
}

// WebSocket handler
esp_err_t ws_handler(httpd_req_t *req) {
    ESP_LOGI(TAG, "Websocket request received!");
    ESP_LOGI(TAG, "Data: %s", req->uri);
   
    // Send back acknowledge
    httpd_ws_frame_t ws_pkt;
    uint8_t buf[128] = { 0 };
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // Put ack into buffer
    sprintf((char*)buf, "ACK");
    ws_pkt.payload =  buf;
    ws_pkt.len = strlen("ACK");
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    httpd_ws_send_frame(req, &ws_pkt);

    return ESP_OK;
}

/* URI handler structure for GET /uri */
httpd_uri_t uri_get = {
    .uri      = "/",
    .method   = HTTP_GET,
    .handler  = index_handler,
    .user_ctx = NULL
};

httpd_uri_t uri_version = {
    .uri      = "/version",
    .method   = HTTP_GET,
    .handler  = version_handler,
    .user_ctx = NULL
};

 httpd_uri_t ws_uri = {
    .uri          = "/ws",
    .method       = HTTP_GET,
    .handler      = ws_handler,
    .user_ctx     = NULL,
    .is_websocket = true
};

httpd_uri_t chart_js_uri = {
    .uri      = "/chartjs",
    .method   = HTTP_GET,
    .handler  = chart_js_handler,
    .user_ctx = NULL
};

// Start the web server
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    if(httpd_start(&server_handle, &config) == ESP_OK) {
        // Register URI handlers
        httpd_register_uri_handler(server_handle, &uri_get);
        httpd_register_uri_handler(server_handle, &ws_uri);
        httpd_register_uri_handler(server_handle, &uri_version);
        httpd_register_uri_handler(server_handle, &chart_js_uri);
        return server_handle;
    }

    return NULL;
}

char* replace_variable(const char* source, const char* placeholder, const char* replacement) {
    const char *p = source;
    int count = 0;
    int placeholderLen = strlen(placeholder);
    int replacementLen = strlen(replacement);
    
    // Counting the number of times the placeholder occurs in the source
    while ((p = strstr(p, placeholder))) {
        p += placeholderLen;
        count++;
    }
    
    // Allocating memory for the new string
    int newSize = strlen(source) + count * (replacementLen - placeholderLen) + 1;
    char *result = malloc(newSize);
    if (result == NULL) return NULL;
    
    char *newStr = result;
    while (*source) {
        if (strstr(source, placeholder) == source) {
            memcpy(newStr, replacement, replacementLen);
            newStr += replacementLen;
            source += placeholderLen;
        } else {
            *newStr++ = *source++;
        }
    }
    *newStr = '\0';

    return result;
}

// Broadcast a random value every second
void broadcast_rand_value(void* pvParameters) {
    while(true) {

        // Get 64 samples from the ADC
        int adc_reading = 0;
        for(int i = 0; i < 64; i++) {
            adc_reading += adc1_get_raw(ADC1_CHANNEL_0);
        }
        adc_reading /= 64; 

        int voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc1_chars);

        // Create a packet containing the integer as a string
        uint8_t buf[128] = { 0 };
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        sprintf((char*)buf, "%d", voltage);
        ws_pkt.payload = buf;
        ws_pkt.len = strlen((char*)buf);
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;
       
        // Send the packet to all connected clients
        httpd_ws_send_frame_to_all_clients(&ws_pkt);

        // Wait for 1 second
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }

}

void app_main() {
    // Initialize the NVS flash memory
    ESP_LOGI(TAG, "SensorLink Module %s", GIT_COMMIT_HASH);
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

    // Initialize Serial Peripheral Interface Flash File System (SPIFFS)
    esp_vfs_spiffs_conf_t config = {
        .base_path = "/spiffs",
        .partition_label = NULL,
        .max_files = 5,
        .format_if_mount_failed = true,
    };
    esp_vfs_spiffs_register(&config);

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

    // Initialize the ADC with default configuration and calibrate it
    esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_DEFAULT, 0, &adc1_chars);

    // Configure the ADC channel with the default configuration width
    ESP_ERROR_CHECK(adc1_config_width(ADC_WIDTH_BIT_DEFAULT));
    
    // Configure the ADC0 channel with the default configuration attenuation
    ESP_ERROR_CHECK(adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_11));

    // Create a task to broadcast a random value every second
    xTaskCreate(broadcast_rand_value, "broadcast_rand_value", 4096, NULL, 5, NULL);

    // Wait for the web server to be stopped
    while(server != NULL) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }

    // Unmount the SPIFFS
    ESP_LOGI(TAG, "Unmounting the SPIFFS...");
    esp_vfs_spiffs_unregister(NULL);
}