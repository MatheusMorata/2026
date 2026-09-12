#include "i2c_bus.h"
#include "esp_log.h"
#include <stdlib.h>

static const char *TAG = "I2C_BUS";

void i2c_bus_init(void) {
    int i2c_master_port = I2C_MASTER_NUM;
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
        .clk_flags = 0,
    };
    i2c_param_config(i2c_master_port, &conf);
    i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
    ESP_LOGI(TAG, "I2C Inicializado nos pinos SDA:%d SCL:%d", I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
}

esp_err_t i2c_read_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, dev_addr, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

esp_err_t i2c_write_bytes(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, size_t len) {
    uint8_t *write_buf = (uint8_t*) malloc(len + 1);
    write_buf[0] = reg_addr;
    for (size_t i = 0; i < len; i++) write_buf[i+1] = data[i];
    esp_err_t err = i2c_master_write_to_device(I2C_MASTER_NUM, dev_addr, write_buf, len + 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    free(write_buf);
    return err;
}
