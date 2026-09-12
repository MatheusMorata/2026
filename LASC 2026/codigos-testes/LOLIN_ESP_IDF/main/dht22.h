#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

esp_err_t dht22_init(gpio_num_t pin);
esp_err_t dht22_read(float *temperature_c, float *humidity_pct);
