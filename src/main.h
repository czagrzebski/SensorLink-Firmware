#ifndef MAIN_H
#define MAIN_H

#include "esp_adc_cal.h"

#define OUTPUT_GPIO_PIN GPIO_NUM_11
#define LED_STRIP_GPIO GPIO_NUM_48
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

esp_adc_cal_characteristics_t adc1_chars;


#endif