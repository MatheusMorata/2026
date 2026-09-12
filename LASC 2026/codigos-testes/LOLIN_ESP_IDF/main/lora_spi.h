#pragma once
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

void lora_spi_init(void);
esp_err_t lora_spi_transmit(const uint8_t *data, size_t len);
int lora_spi_get_last_rssi(void);
