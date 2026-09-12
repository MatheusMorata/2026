#include "mpu9250.h"

#include <math.h>
#include "esp_log.h"
#include "i2c_bus.h"

#define MPU9250_WHO_AM_I_REG      0x75
#define MPU9250_PWR_MGMT_1_REG    0x6B
#define MPU9250_SMPLRT_DIV_REG    0x19
#define MPU9250_CONFIG_REG        0x1A
#define MPU9250_GYRO_CONFIG_REG   0x1B
#define MPU9250_ACCEL_CONFIG_REG  0x1C
#define MPU9250_ACCEL_XOUT_H_REG  0x3B

static const char *TAG = "MPU9250";
static uint8_t s_addr = 0x68;

static int16_t be16_to_i16(uint8_t hi, uint8_t lo) {
    return (int16_t)((hi << 8) | lo);
}

esp_err_t mpu9250_init(uint8_t i2c_addr) {
    s_addr = i2c_addr;

    uint8_t who_am_i = 0;
    esp_err_t err = i2c_read_bytes(s_addr, MPU9250_WHO_AM_I_REG, &who_am_i, 1);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao ler WHO_AM_I: %s", esp_err_to_name(err));
        return err;
    }

    if (who_am_i != 0x71 && who_am_i != 0x73) {
        ESP_LOGW(TAG, "WHO_AM_I inesperado: 0x%02X (esperado 0x71/0x73)", who_am_i);
    }

    uint8_t v = 0x00;
    err = i2c_write_bytes(s_addr, MPU9250_PWR_MGMT_1_REG, &v, 1);
    if (err != ESP_OK) return err;

    v = 0x04;
    err = i2c_write_bytes(s_addr, MPU9250_SMPLRT_DIV_REG, &v, 1);
    if (err != ESP_OK) return err;

    v = 0x03;
    err = i2c_write_bytes(s_addr, MPU9250_CONFIG_REG, &v, 1);
    if (err != ESP_OK) return err;

    v = 0x00;
    err = i2c_write_bytes(s_addr, MPU9250_GYRO_CONFIG_REG, &v, 1);
    if (err != ESP_OK) return err;

    v = 0x00;
    err = i2c_write_bytes(s_addr, MPU9250_ACCEL_CONFIG_REG, &v, 1);
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "Inicializado em 0x%02X", s_addr);
    return ESP_OK;
}

esp_err_t mpu9250_read(mpu9250_data_t *out) {
    if (!out) return ESP_ERR_INVALID_ARG;

    uint8_t raw[14] = {0};
    esp_err_t err = i2c_read_bytes(s_addr, MPU9250_ACCEL_XOUT_H_REG, raw, sizeof(raw));
    if (err != ESP_OK) return err;

    int16_t ax = be16_to_i16(raw[0], raw[1]);
    int16_t ay = be16_to_i16(raw[2], raw[3]);
    int16_t az = be16_to_i16(raw[4], raw[5]);
    int16_t temp = be16_to_i16(raw[6], raw[7]);
    int16_t gx = be16_to_i16(raw[8], raw[9]);
    int16_t gy = be16_to_i16(raw[10], raw[11]);
    int16_t gz = be16_to_i16(raw[12], raw[13]);

    out->ax_g = (float)ax / 16384.0f;
    out->ay_g = (float)ay / 16384.0f;
    out->az_g = (float)az / 16384.0f;
    out->gx_dps = (float)gx / 131.0f;
    out->gy_dps = (float)gy / 131.0f;
    out->gz_dps = (float)gz / 131.0f;
    out->temp_c = ((float)temp / 333.87f) + 21.0f;

    return ESP_OK;
}

void mpu9250_estimate_attitude(const mpu9250_data_t *data, float dt_s, float *roll_deg, float *pitch_deg, float *yaw_deg) {
    if (!data || !roll_deg || !pitch_deg || !yaw_deg) return;

    float accel_roll = atan2f(data->ay_g, data->az_g) * (180.0f / (float)M_PI);
    float accel_pitch = atan2f(-data->ax_g, sqrtf(data->ay_g * data->ay_g + data->az_g * data->az_g)) * (180.0f / (float)M_PI);

    const float alpha = 0.98f;
    *roll_deg = alpha * (*roll_deg + data->gx_dps * dt_s) + (1.0f - alpha) * accel_roll;
    *pitch_deg = alpha * (*pitch_deg + data->gy_dps * dt_s) + (1.0f - alpha) * accel_pitch;
    *yaw_deg += data->gz_dps * dt_s;

    if (*yaw_deg > 180.0f) *yaw_deg -= 360.0f;
    if (*yaw_deg < -180.0f) *yaw_deg += 360.0f;
}
