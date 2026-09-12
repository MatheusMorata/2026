#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t bmp280_init(uint8_t i2c_addr);
esp_err_t bmp280_read(float *temperature_c, float *pressure_hpa, float *altitude_m);
