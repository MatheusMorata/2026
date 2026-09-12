#include "task_eeprom.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "at24c256.h"
#include "obc_types.h"

#define EEPROM_ADDR               AT24C256_DEFAULT_ADDR
#define EEPROM_DATA_START_ADDR    0x0100U
#define EEPROM_RECORD_MAGIC       0x4550524DU

static const char *TAG = "TASK_EEPROM";
extern QueueHandle_t filaEeprom;

typedef struct {
    uint32_t magic;
    uint32_t seq;
    sensorsData_t payload;
    uint32_t checksum;
} eeprom_record_t;

static uint32_t calc_checksum(const eeprom_record_t *record) {
    const uint8_t *bytes = (const uint8_t *)record;
    size_t data_len = sizeof(eeprom_record_t) - sizeof(record->checksum);
    uint32_t acc = 0;

    for (size_t i = 0; i < data_len; i++) {
        acc = (acc << 5) - acc + bytes[i];
    }

    return acc;
}

static bool record_valid(const eeprom_record_t *record) {
    if (record->magic != EEPROM_RECORD_MAGIC) {
        return false;
    }

    return calc_checksum(record) == record->checksum;
}

static uint16_t slot_address(uint32_t slot, uint32_t max_slots) {
    if (max_slots == 0) {
        return EEPROM_DATA_START_ADDR;
    }

    return (uint16_t)(EEPROM_DATA_START_ADDR + (slot % max_slots) * sizeof(eeprom_record_t));
}

static uint32_t recover_next_sequence(uint32_t max_slots) {
    uint32_t best_seq = 0;
    bool found_any = false;

    for (uint32_t slot = 0; slot < max_slots; slot++) {
        eeprom_record_t record = {0};
        uint16_t addr = slot_address(slot, max_slots);

        if (at24c256_read(addr, (uint8_t *)&record, sizeof(record)) != ESP_OK) {
            continue;
        }

        if (!record_valid(&record)) {
            continue;
        }

        if (!found_any || record.seq > best_seq) {
            best_seq = record.seq;
            found_any = true;
        }
    }

    return found_any ? (best_seq + 1U) : 0U;
}

void task_eeprom(void *pvParameters) {
    ESP_LOGI(TAG, "Task EEPROM Iniciada");

    esp_err_t init_err = at24c256_init(EEPROM_ADDR);
    if (init_err != ESP_OK) {
        ESP_LOGE(TAG, "EEPROM indisponivel: %s", esp_err_to_name(init_err));
        vTaskDelete(NULL);
        return;
    }

    const uint32_t available_bytes = AT24C256_SIZE_BYTES - EEPROM_DATA_START_ADDR;
    const uint32_t max_slots = available_bytes / sizeof(eeprom_record_t);
    if (max_slots == 0) {
        ESP_LOGE(TAG, "Sem espaco para registros na EEPROM");
        vTaskDelete(NULL);
        return;
    }

    uint32_t next_seq = recover_next_sequence(max_slots);
    ESP_LOGI(TAG, "Buffer circular pronto: %lu registros, prox seq=%lu",
             (unsigned long)max_slots,
             (unsigned long)next_seq);

    sensorsData_t dados;
    while (1) {
        if (xQueueReceive(filaEeprom, &dados, pdMS_TO_TICKS(500)) != pdPASS) {
            continue;
        }

        eeprom_record_t record = {0};
        record.magic = EEPROM_RECORD_MAGIC;
        record.seq = next_seq;
        record.payload = dados;
        record.checksum = calc_checksum(&record);

        uint16_t wr_addr = slot_address(next_seq, max_slots);
        esp_err_t wr_err = at24c256_write(wr_addr, (const uint8_t *)&record, sizeof(record));
        if (wr_err != ESP_OK) {
            ESP_LOGW(TAG, "Falha gravando seq %lu em 0x%04X: %s",
                     (unsigned long)next_seq,
                     wr_addr,
                     esp_err_to_name(wr_err));
            continue;
        }

        if ((next_seq % 20U) == 0U) {
            ESP_LOGI(TAG, "EEPROM gravada: seq=%lu addr=0x%04X tempo=%lu",
                     (unsigned long)next_seq,
                     wr_addr,
                     (unsigned long)dados.seconds);
        }

        next_seq++;
    }
}
