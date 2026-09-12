#pragma once

#include <stdbool.h>
#include "driver/uart.h"
#include "esp_err.h"

typedef struct {
    bool valid;
    float latitude;
    float longitude;
    int sats;
} gps_fix_t;

esp_err_t gps_neo6m_init(uart_port_t uart_num, int tx_pin, int rx_pin, int baudrate);
esp_err_t gps_neo6m_poll(gps_fix_t *fix);
