#pragma once

#include <stddef.h>
#include <stdint.h>
#include "driver/uart.h"
#include "esp_err.h"

#define CHAIN_MASTER_ID      0x01
#define CHAIN_SUPPLY_ID      0x10
#define CHAIN_ATTITUDE_ID    0x20
#define CHAIN_MISSION_ID     0x30

esp_err_t serial_chain_master_init(uart_port_t uart_num, int tx_pin, int rx_pin, int baudrate);
esp_err_t serial_chain_master_send(uint8_t dst, uint8_t cmd, const uint8_t *payload, size_t payload_len);
esp_err_t serial_chain_master_request(
    uint8_t dst,
    uint8_t cmd,
    const uint8_t *payload,
    size_t payload_len,
    uint8_t expected_cmd,
    uint8_t *resp_payload,
    size_t resp_capacity,
    size_t *resp_len,
    uint32_t timeout_ms
);
