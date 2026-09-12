#include "task_eeprom.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "i2c_bus.h"

#define EEPROM_ADDR 0x50

static const char *TAG = "TASK_EEPROM";

void task_eeprom(void *pvParameters) {
    ESP_LOGI(TAG, "Task EEPROM Iniciada");
    while (1) {
        // Rotinas de paginação não-bloqueantes da EEPROM entram aqui
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
