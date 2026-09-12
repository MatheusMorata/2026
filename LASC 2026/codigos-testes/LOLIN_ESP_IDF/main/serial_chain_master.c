#include "serial_chain_master.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_idf_version.h"

#define CHAIN_SOF            0x7E
#define CHAIN_MAX_PAYLOAD    96
#define CHAIN_HDR_SIZE       5
#define CHAIN_FTR_SIZE       1
#define CHAIN_FRAME_OVERHEAD (CHAIN_HDR_SIZE + CHAIN_FTR_SIZE)
#define CHAIN_MAX_FRAME      (CHAIN_MAX_PAYLOAD + CHAIN_FRAME_OVERHEAD)
#define CHAIN_UART_BUF_SIZE  2048

static uart_port_t s_uart = UART_NUM_2;

static uint8_t xor_crc(const uint8_t *data, size_t len) {
    uint8_t c = 0;
    for (size_t i = 0; i < len; i++) c ^= data[i];
    return c;
}

static esp_err_t read_exact(uint8_t *buf, size_t n, uint32_t timeout_ms) {
    size_t total = 0;
    TickType_t start = xTaskGetTickCount();

    while (total < n) {
        TickType_t elapsed = xTaskGetTickCount() - start;
        uint32_t elapsed_ms = (uint32_t)(elapsed * portTICK_PERIOD_MS);
        if (elapsed_ms >= timeout_ms) return ESP_ERR_TIMEOUT;

        uint32_t remain_ms = timeout_ms - elapsed_ms;
        int r = uart_read_bytes(s_uart, buf + total, n - total, pdMS_TO_TICKS(remain_ms));
        if (r > 0) {
            total += (size_t)r;
        }
    }

    return ESP_OK;
}

static esp_err_t receive_frame(uint8_t *src, uint8_t *dst, uint8_t *cmd, uint8_t *payload, size_t cap, size_t *len, uint32_t timeout_ms) {
    uint8_t b = 0;
    TickType_t start = xTaskGetTickCount();

    while (1) {
        TickType_t elapsed = xTaskGetTickCount() - start;
        uint32_t elapsed_ms = (uint32_t)(elapsed * portTICK_PERIOD_MS);
        if (elapsed_ms >= timeout_ms) return ESP_ERR_TIMEOUT;

        uint32_t remain_ms = timeout_ms - elapsed_ms;
        int r = uart_read_bytes(s_uart, &b, 1, pdMS_TO_TICKS(remain_ms));
        if (r <= 0) continue;
        if (b != CHAIN_SOF) continue;

        uint8_t header[4] = {0};
        esp_err_t err = read_exact(header, sizeof(header), remain_ms);
        if (err != ESP_OK) return err;

        *dst = header[0];
        *src = header[1];
        *cmd = header[2];
        uint8_t plen = header[3];

        if (plen > CHAIN_MAX_PAYLOAD || plen > cap) {
            return ESP_ERR_INVALID_SIZE;
        }

        err = read_exact(payload, plen, remain_ms);
        if (err != ESP_OK) return err;

        uint8_t crc = 0;
        err = read_exact(&crc, 1, remain_ms);
        if (err != ESP_OK) return err;

        uint8_t crc_buf[4 + CHAIN_MAX_PAYLOAD];
        crc_buf[0] = header[0];
        crc_buf[1] = header[1];
        crc_buf[2] = header[2];
        crc_buf[3] = header[3];
        memcpy(&crc_buf[4], payload, plen);

        if (xor_crc(crc_buf, 4 + plen) != crc) {
            continue;
        }

        *len = plen;
        return ESP_OK;
    }
}

esp_err_t serial_chain_master_init(uart_port_t uart_num, int tx_pin, int rx_pin, int baudrate) {
    s_uart = uart_num;

    uart_config_t cfg = {
        .baud_rate = baudrate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if ESP_IDF_VERSION_MAJOR >= 5
        .source_clk = UART_SCLK_DEFAULT,
#endif
    };

    esp_err_t err = uart_driver_delete(s_uart);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    err = uart_driver_install(s_uart, CHAIN_UART_BUF_SIZE, CHAIN_UART_BUF_SIZE, 0, NULL, 0);
    if (err != ESP_OK) return err;

    err = uart_param_config(s_uart, &cfg);
    if (err != ESP_OK) return err;

    err = uart_set_pin(s_uart, tx_pin, rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) return err;

    uart_flush_input(s_uart);
    return ESP_OK;
}

esp_err_t serial_chain_master_send(uint8_t dst, uint8_t cmd, const uint8_t *payload, size_t payload_len) {
    if (payload_len > CHAIN_MAX_PAYLOAD) return ESP_ERR_INVALID_SIZE;
    if (payload_len > 0 && payload == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t frame[CHAIN_MAX_FRAME] = {0};
    size_t idx = 0;

    frame[idx++] = CHAIN_SOF;
    frame[idx++] = dst;
    frame[idx++] = CHAIN_MASTER_ID;
    frame[idx++] = cmd;
    frame[idx++] = (uint8_t)payload_len;

    if (payload_len > 0) {
        memcpy(&frame[idx], payload, payload_len);
        idx += payload_len;
    }

    uint8_t crc = xor_crc(&frame[1], 4 + payload_len);
    frame[idx++] = crc;

    int w = uart_write_bytes(s_uart, (const char *)frame, idx);
    if (w != (int)idx) return ESP_FAIL;

    return ESP_OK;
}

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
) {
    if (!resp_payload || !resp_len) return ESP_ERR_INVALID_ARG;

    esp_err_t err = serial_chain_master_send(dst, cmd, payload, payload_len);
    if (err != ESP_OK) return err;

    TickType_t start = xTaskGetTickCount();
    while (1) {
        TickType_t elapsed = xTaskGetTickCount() - start;
        uint32_t elapsed_ms = (uint32_t)(elapsed * portTICK_PERIOD_MS);
        if (elapsed_ms >= timeout_ms) return ESP_ERR_TIMEOUT;

        uint32_t remain_ms = timeout_ms - elapsed_ms;
        uint8_t src = 0;
        uint8_t rx_dst = 0;
        uint8_t rx_cmd = 0;
        size_t len = 0;

        err = receive_frame(&src, &rx_dst, &rx_cmd, resp_payload, resp_capacity, &len, remain_ms);
        if (err != ESP_OK) return err;

        if (src == dst && rx_dst == CHAIN_MASTER_ID && rx_cmd == expected_cmd) {
            *resp_len = len;
            return ESP_OK;
        }
    }
}
