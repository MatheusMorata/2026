#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    float ax_g;
    float ay_g;
    float az_g;
    float gx_dps;
    float gy_dps;
    float gz_dps;
    float temp_c;
} mpu9250_data_t;

esp_err_t mpu9250_init(uint8_t i2c_addr);
esp_err_t mpu9250_read(mpu9250_data_t *out);
void mpu9250_estimate_attitude(const mpu9250_data_t *data, float dt_s, float *roll_deg, float *pitch_deg, float *yaw_deg);
