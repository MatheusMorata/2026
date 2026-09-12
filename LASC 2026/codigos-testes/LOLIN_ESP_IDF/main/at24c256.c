#include "at24c256.h"

#include <stdbool.h>
#include <string.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_bus.h"

#define AT24C256_WRITE_CYCLE_MS  6U
#define AT24C256_POLL_TIMEOUT_MS 20U

static const char *TAG = "AT24C256";
static uint8_t s_dev_addr = AT24C256_DEFAULT_ADDR;

static bool at24c256_range_ok(uint16_t mem_addr, size_t len) {
    if (len == 0) {
        return true;
    }

    uint32_t end_addr = (uint32_t)mem_addr + (uint32_t)len;
    return end_addr <= AT24C256_SIZE_BYTES;
}

static esp_err_t at24c256_wait_ready(void) {
    TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(AT24C256_POLL_TIMEOUT_MS);

    while (1) {
        esp_err_t err = i2c_master_write_to_device(
            I2C_MASTER_NUM,
            s_dev_addr,
            NULL,
            0,
            I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS
        );

        if (err == ESP_OK) {
            return ESP_OK;
        }

        if (xTaskGetTickCount() >= deadline) {
            return err;
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

esp_err_t at24c256_init(uint8_t i2c_addr) {
    s_dev_addr = i2c_addr;

    esp_err_t err = at24c256_wait_ready();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "EEPROM nao encontrada no endereco 0x%02X: %s", s_dev_addr, esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "EEPROM AT24C256 pronta em 0x%02X", s_dev_addr);
    return ESP_OK;
}

esp_err_t at24c256_read(uint16_t mem_addr, uint8_t *data, size_t len) {
    if (!data || !at24c256_range_ok(mem_addr, len)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ESP_OK;
    }

    uint8_t addr_bytes[2] = {
        (uint8_t)((mem_addr >> 8) & 0xFF),
        (uint8_t)(mem_addr & 0xFF)
    };

    return i2c_master_write_read_device(
        I2C_MASTER_NUM,
        s_dev_addr,
        addr_bytes,
        sizeof(addr_bytes),
        data,
        len,
        I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS
    );
}

esp_err_t at24c256_write(uint16_t mem_addr, const uint8_t *data, size_t len) {
    if (!data || !at24c256_range_ok(mem_addr, len)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (len == 0) {
        return ESP_OK;
    }

    size_t offset = 0;
    while (offset < len) {
        uint16_t curr_addr = (uint16_t)(mem_addr + offset);
        size_t page_offset = curr_addr % AT24C256_PAGE_SIZE;
        size_t bytes_to_page_end = AT24C256_PAGE_SIZE - page_offset;
        size_t chunk = len - offset;
        if (chunk > bytes_to_page_end) {
            chunk = bytes_to_page_end;
        }

        uint8_t tx_buf[AT24C256_PAGE_SIZE + 2];
        tx_buf[0] = (uint8_t)((curr_addr >> 8) & 0xFF);
        tx_buf[1] = (uint8_t)(curr_addr & 0xFF);
        memcpy(&tx_buf[2], &data[offset], chunk);

        esp_err_t err = i2c_master_write_to_device(
            I2C_MASTER_NUM,
            s_dev_addr,
            tx_buf,
            chunk + 2,
            I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS
        );
        if (err != ESP_OK) {
            return err;
        }

        vTaskDelay(pdMS_TO_TICKS(AT24C256_WRITE_CYCLE_MS));
        err = at24c256_wait_ready();
        if (err != ESP_OK) {
            return err;
        }

        offset += chunk;
    }

    return ESP_OK;
}
