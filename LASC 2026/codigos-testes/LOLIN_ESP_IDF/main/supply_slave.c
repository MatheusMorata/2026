#include "supply_slave.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "serial_chain_master.h"

#define SUPPLY_CMD_GET_STATUS  0x01
#define SUPPLY_CMD_STATUS_RESP 0x81

static float parse_field(const char *s, const char *key, float fallback) {
    const char *p = strstr(s, key);
    if (!p) return fallback;
    p += strlen(key);
    return strtof(p, NULL);
}

esp_err_t supply_slave_get_status(supply_status_t *out, uint32_t timeout_ms) {
    if (!out) return ESP_ERR_INVALID_ARG;

    uint8_t payload[96] = {0};
    size_t len = 0;

    esp_err_t err = serial_chain_master_request(
        CHAIN_SUPPLY_ID,
        SUPPLY_CMD_GET_STATUS,
        NULL,
        0,
        SUPPLY_CMD_STATUS_RESP,
        payload,
        sizeof(payload) - 1,
        &len,
        timeout_ms
    );
    if (err != ESP_OK) return err;

    payload[len] = '\0';
    const char *msg = (const char *)payload;

    out->temp_bat1 = parse_field(msg, "TB1=", out->temp_bat1);
    out->temp_bat2 = parse_field(msg, "TB2=", out->temp_bat2);
    out->tensao = parse_field(msg, "V=", out->tensao);
    out->corrente = parse_field(msg, "I=", out->corrente);

    return ESP_OK;
}
