#include "attitude_slave.h"

#include <string.h>
#include "serial_chain_master.h"

#define ATT_CMD_EXEC       0x10
#define ATT_CMD_EXEC_ACK   0x90

esp_err_t attitude_slave_send_command(const char *command, uint32_t timeout_ms) {
    if (!command) return ESP_ERR_INVALID_ARG;

    size_t cmd_len = strnlen(command, 48);
    uint8_t response[8] = {0};
    size_t resp_len = 0;

    return serial_chain_master_request(
        CHAIN_ATTITUDE_ID,
        ATT_CMD_EXEC,
        (const uint8_t *)command,
        cmd_len,
        ATT_CMD_EXEC_ACK,
        response,
        sizeof(response),
        &resp_len,
        timeout_ms
    );
}
