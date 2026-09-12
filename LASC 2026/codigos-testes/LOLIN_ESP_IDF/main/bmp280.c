#include "bmp280.h"

#include <math.h>
#include <stdbool.h>
#include "esp_log.h"
#include "i2c_bus.h"

#define BMP280_CHIP_ID_REG         0xD0
#define BMP280_RESET_REG           0xE0
#define BMP280_CTRL_MEAS_REG       0xF4
#define BMP280_CONFIG_REG          0xF5
#define BMP280_PRESS_MSB_REG       0xF7
#define BMP280_CALIB_REG_START     0x88

static const char *TAG = "BMP280";
static uint8_t s_addr = 0x76;
static bool s_calib_ok = false;

static uint16_t dig_T1;
static int16_t dig_T2;
static int16_t dig_T3;
static uint16_t dig_P1;
static int16_t dig_P2;
static int16_t dig_P3;
static int16_t dig_P4;
static int16_t dig_P5;
static int16_t dig_P6;
static int16_t dig_P7;
static int16_t dig_P8;
static int16_t dig_P9;
static int32_t t_fine;

static uint16_t le_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static int16_t le_i16(const uint8_t *p) {
    return (int16_t)le_u16(p);
}

static esp_err_t bmp280_read_calib(void) {
    uint8_t c[24] = {0};
    esp_err_t err = i2c_read_bytes(s_addr, BMP280_CALIB_REG_START, c, sizeof(c));
    if (err != ESP_OK) return err;

    dig_T1 = le_u16(&c[0]);
    dig_T2 = le_i16(&c[2]);
    dig_T3 = le_i16(&c[4]);
    dig_P1 = le_u16(&c[6]);
    dig_P2 = le_i16(&c[8]);
    dig_P3 = le_i16(&c[10]);
    dig_P4 = le_i16(&c[12]);
    dig_P5 = le_i16(&c[14]);
    dig_P6 = le_i16(&c[16]);
    dig_P7 = le_i16(&c[18]);
    dig_P8 = le_i16(&c[20]);
    dig_P9 = le_i16(&c[22]);

    s_calib_ok = true;
    return ESP_OK;
}

esp_err_t bmp280_init(uint8_t i2c_addr) {
    s_addr = i2c_addr;

    uint8_t id = 0;
    esp_err_t err = i2c_read_bytes(s_addr, BMP280_CHIP_ID_REG, &id, 1);
    if (err != ESP_OK) return err;
    if (id != 0x58) {
        ESP_LOGW(TAG, "CHIP ID inesperado: 0x%02X (esperado 0x58)", id);
    }

    uint8_t reset = 0xB6;
    err = i2c_write_bytes(s_addr, BMP280_RESET_REG, &reset, 1);
    if (err != ESP_OK) return err;

    uint8_t ctrl_meas = 0x27;
    err = i2c_write_bytes(s_addr, BMP280_CTRL_MEAS_REG, &ctrl_meas, 1);
    if (err != ESP_OK) return err;

    uint8_t config = 0x10;
    err = i2c_write_bytes(s_addr, BMP280_CONFIG_REG, &config, 1);
    if (err != ESP_OK) return err;

    err = bmp280_read_calib();
    if (err != ESP_OK) return err;

    ESP_LOGI(TAG, "Inicializado em 0x%02X", s_addr);
    return ESP_OK;
}

esp_err_t bmp280_read(float *temperature_c, float *pressure_hpa, float *altitude_m) {
    if (!temperature_c || !pressure_hpa || !altitude_m) return ESP_ERR_INVALID_ARG;
    if (!s_calib_ok) return ESP_ERR_INVALID_STATE;

    uint8_t raw[6] = {0};
    esp_err_t err = i2c_read_bytes(s_addr, BMP280_PRESS_MSB_REG, raw, sizeof(raw));
    if (err != ESP_OK) return err;

    int32_t adc_P = (int32_t)((raw[0] << 12) | (raw[1] << 4) | (raw[2] >> 4));
    int32_t adc_T = (int32_t)((raw[3] << 12) | (raw[4] << 4) | (raw[5] >> 4));

    int32_t var1 = ((((adc_T >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    int32_t var2 = (((((adc_T >> 4) - ((int32_t)dig_T1)) * ((adc_T >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >> 14;
    t_fine = var1 + var2;
    int32_t T = (t_fine * 5 + 128) >> 8;

    int64_t pvar1 = ((int64_t)t_fine) - 128000;
    int64_t pvar2 = pvar1 * pvar1 * (int64_t)dig_P6;
    pvar2 = pvar2 + ((pvar1 * (int64_t)dig_P5) << 17);
    pvar2 = pvar2 + (((int64_t)dig_P4) << 35);
    pvar1 = ((pvar1 * pvar1 * (int64_t)dig_P3) >> 8) + ((pvar1 * (int64_t)dig_P2) << 12);
    pvar1 = (((((int64_t)1) << 47) + pvar1) * ((int64_t)dig_P1)) >> 33;

    if (pvar1 == 0) return ESP_FAIL;

    int64_t p = 1048576 - adc_P;
    p = (((p << 31) - pvar2) * 3125) / pvar1;
    pvar1 = (((int64_t)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    pvar2 = (((int64_t)dig_P8) * p) >> 19;
    p = ((p + pvar1 + pvar2) >> 8) + (((int64_t)dig_P7) << 4);

    float temp = T / 100.0f;
    float pressure_pa = p / 256.0f;
    float pressure_h = pressure_pa / 100.0f;
    float altitude = 44330.0f * (1.0f - powf(pressure_h / 1013.25f, 0.1903f));

    *temperature_c = temp;
    *pressure_hpa = pressure_h;
    *altitude_m = altitude;

    return ESP_OK;
}
