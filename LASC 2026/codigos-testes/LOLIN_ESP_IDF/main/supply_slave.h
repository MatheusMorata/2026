#pragma once

#include <stdint.h>
#include "esp_err.h"

typedef struct {
    float temp_bat1;
    float temp_bat2;
    float tensao;
    float corrente;
} supply_status_t;

esp_err_t supply_slave_get_status(supply_status_t *out, uint32_t timeout_ms);
