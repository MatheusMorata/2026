#pragma once

#include <stdint.h>
#include "esp_err.h"

esp_err_t mission_slave_send_command(const char *command, uint32_t timeout_ms);
