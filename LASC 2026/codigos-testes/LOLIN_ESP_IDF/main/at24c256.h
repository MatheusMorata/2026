#pragma once

#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#define AT24C256_DEFAULT_ADDR    0x50
#define AT24C256_SIZE_BYTES      32768U
#define AT24C256_PAGE_SIZE       64U

esp_err_t at24c256_init(uint8_t i2c_addr);
esp_err_t at24c256_read(uint16_t mem_addr, uint8_t *data, size_t len);
esp_err_t at24c256_write(uint16_t mem_addr, const uint8_t *data, size_t len);
