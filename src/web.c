#include "web.h"

#include "esp_wifi.h"
#include "esp_mac.h"
#include "wifi.h"
#include "esp_log.h"
#include "esp_event.h"
#include "string.h"
#include "esp_log.h"
#include <esp_http_server.h>
#include "esp_adc_cal.h"
#include "driver/adc.h"

// Import MIN function
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#endif 

#include "lwip/err.h"
#include "lwip/sys.h"

// Define GIT_COMMIT_HASH if it's not already defined
// stops GCC from whining about an undefined macro 
#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "undefined"
#endif

httpd_handle_t server_handle = NULL;

//==== URI handlers ====//
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

httpd_uri_t wifi_config_uri = {
    .uri      = "/wifi-save",
    .method   = HTTP_POST,
    .handler  = wifi_config_handler,
    .user_ctx = NULL
};

httpd_uri_t wifi_config_html_uri = {
    .uri      = "/network.html",
    .method   = HTTP_GET,
    .handler  = network_page_handler,
    .user_ctx = NULL
};

httpd_uri_t chart_js_uri = {
    .uri      = "/chartjs",
    .method   = HTTP_GET,
    .handler  = chart_js_handler,
    .user_ctx = NULL
};

httpd_uri_t toggle_led_uri = {
    .uri      = "/led",
    .method   = HTTP_GET,
    .handler  = toggle_led_handler,
    .user_ctx = NULL
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
    ESP_LOGI(WEB_TAG, "Request received!");

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

esp_err_t network_page_handler(httpd_req_t *req) {
    // Open file from SPIFFS
    ESP_LOGI(WEB_TAG, "Request received!");

    FILE* file = fopen("/spiffs/network.html", "r");
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

esp_err_t wifi_config_handler(httpd_req_t *req) {
    char buf[1024];
    int ret = httpd_req_recv(req, buf, 1024);
    if (ret <= 0) {  // 0 if connection closed, < 0 if error
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    // Null-terminate the buffer
    buf[ret] = '\0';

    // Parse the buffer into key/value pairs
    char ssid[32];
    char password[64];
    ret = httpd_query_key_value(buf, "ssid", ssid, sizeof(ssid));
    if (ret != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    ret = httpd_query_key_value(buf, "password", password, sizeof(password));
    if (ret != ESP_OK) {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    // Save the Wi-Fi credentials to NVS
    save_wifi_credentials_to_nvs(ssid, password);

    // Send a response
    httpd_resp_send(req, "OK", 2);

    // Restart the device
    esp_restart();
    return ESP_OK;
}

esp_err_t chart_js_handler(httpd_req_t *req) {
    // Open file from SPIFFS
    ESP_LOGI(WEB_TAG, "Request received!");

    FILE* file = fopen("/spiffs/chart.js", "r");
    if (file == NULL) {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    // Set header for cache control
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=3600");

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
    ESP_LOGI(WEB_TAG, "Request received!");

    httpd_resp_send(req, GIT_COMMIT_HASH, strlen(GIT_COMMIT_HASH));
    return ESP_OK;
}

// WebSocket handler
esp_err_t ws_handler(httpd_req_t *req) {
    ESP_LOGI(WEB_TAG, "Websocket request received!");
    ESP_LOGI(WEB_TAG, "Data: %s", req->uri);
   
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

// Start the web server
httpd_handle_t start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    if(httpd_start(&server_handle, &config) == ESP_OK) {
        // Register URI handlers
        httpd_register_uri_handler(server_handle, &uri_get);
        httpd_register_uri_handler(server_handle, &ws_uri);
        httpd_register_uri_handler(server_handle, &uri_version);
        httpd_register_uri_handler(server_handle, &chart_js_uri);
        httpd_register_uri_handler(server_handle, &toggle_led_uri);
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

esp_err_t toggle_led_handler(httpd_req_t *req) {
   // Get query for pin number and state. Toggle the pin based on state (HTTP GET REQUEST)
    char buf[128];
    int buf_len = 128;
    int ret = httpd_req_get_url_query_str(req, buf, buf_len);
    if (ret == ESP_OK) {
        ESP_LOGI(WEB_TAG, "Found URL query => %s", buf);
        char param[32];
        // Get pin number
        ret = httpd_query_key_value(buf, "pin", param, sizeof(param));
        if (ret == ESP_OK) {
            int pin = atoi(param);
            ESP_LOGI(WEB_TAG, "Pin Number => %d", pin);
            // Get state
            ret = httpd_query_key_value(buf, "state", param, sizeof(param));
            if (ret == ESP_OK) {
                int state = atoi(param);
                ESP_LOGI(WEB_TAG, "State => %d", state);
                gpio_set_level(pin, state);
                httpd_resp_send(req, "OK", 2);
                return ESP_OK;
            }
        }
    }
    httpd_resp_send_404(req);
    return ESP_FAIL;
}

// Broadcast a random value every second
void broadcast_adc_values(void* pvParameters) {
    while(true) {

        // Get 64 samples from the ADC
        int adc_reading = 0;
        for(int i = 0; i < 64; i++) {
            adc_reading += adc1_get_raw(ADC1_CHANNEL_0);
        }
        adc_reading /= 64; 

        int voltage = esp_adc_cal_raw_to_voltage(adc_reading, &adc1_chars);

        // Create a packet containing the integer as a string in JSON format with the current
        // digital state of pin 22
        char buf[128];
        sprintf(buf, "{\"adc\": %d, \"pin\": %d}", voltage, gpio_get_level(22));
        httpd_ws_frame_t ws_pkt;
        memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
        ws_pkt.payload = (uint8_t*)buf;
        ws_pkt.len = strlen(buf);
        ws_pkt.type = HTTPD_WS_TYPE_TEXT;

        // Send the packet to all connected clients
        httpd_ws_send_frame_to_all_clients(&ws_pkt);

        // Wait for 1 second
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}