#include "web.h"

#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_event.h"
#include "string.h"

char *generate_homepage() {
    char *html = "<!DOCTYPE html><body><h1>SensorLink</h1></body></html>";
    return html;
}