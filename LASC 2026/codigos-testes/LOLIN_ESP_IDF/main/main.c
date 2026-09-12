#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "i2c_bus.h"
#include "lora_spi.h"
#include "task_sensores.h"
#include "task_telemetria.h"
#include "task_eeprom.h"
#include "serial_chain_master.h"

static const char *TAG = "OBC_MAIN";

#define CHAIN_UART_PORT UART_NUM_2
#define CHAIN_TX_PIN    27
#define CHAIN_RX_PIN    25
#define CHAIN_BAUDRATE  115200

QueueHandle_t filaTelemetria;
QueueHandle_t filaCompleta;

void app_main(void)
{
    ESP_LOGI(TAG, "Inicializando Computador de Bordo - Ceres-1");

    // Inicializa barramentos
    i2c_bus_init();
    lora_spi_init();
    esp_err_t chain_err = serial_chain_master_init(CHAIN_UART_PORT, CHAIN_TX_PIN, CHAIN_RX_PIN, CHAIN_BAUDRATE);
    if (chain_err != ESP_OK) {
        ESP_LOGW(TAG, "Barramento serial em cadeia indisponivel: %s", esp_err_to_name(chain_err));
    }

    // Cria as filas
    filaTelemetria = xQueueCreate(10, sizeof(sensorsData_t));
    filaCompleta = xQueueCreate(3, sizeof(respost_t));

    if (filaTelemetria == NULL || filaCompleta == NULL) {
        ESP_LOGE(TAG, "Falha ao criar filas do FreeRTOS");
        return;
    }

    // Cria as Tasks fixadas nos cores (ESP-IDF style)
    xTaskCreatePinnedToCore(task_sensores, "TaskSensores", 6144, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(task_eeprom, "TaskEEPROM", 4096, NULL, 3, NULL, 1);
    xTaskCreatePinnedToCore(task_telemetria, "TaskTelemetria", 4096, NULL, 4, NULL, 1);
    
    // O loop principal (app_main) pode ser finalizado, as tasks assumem.
}
