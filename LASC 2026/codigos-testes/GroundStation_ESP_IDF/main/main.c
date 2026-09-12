#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "lora_sx1278.h"
#include "task_rx.h"
#include "task_tx.h"

static const char *TAG = "GS_MAIN";

void app_main(void)
{
    ESP_LOGI(TAG, "Inicializando Estacao de Solo (Receptor) - Ceres-1");

    // Configura UART0 (Console) para leitura de comandos (nao-bloqueante)
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    uart_param_config(UART_NUM_0, &uart_config);
    uart_driver_install(UART_NUM_0, 256, 0, 0, NULL, 0);

    // Inicializa o LoRa SX1278 na Lolin D32
    lora_sx1278_init();

    // Cria as Tasks fixadas nos cores
    xTaskCreatePinnedToCore(task_rx, "TaskRX", 8192, NULL, 4, NULL, 1);
    xTaskCreatePinnedToCore(task_tx, "TaskTX", 4096, NULL, 3, NULL, 1);
}
