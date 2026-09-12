#include "task_telemetria.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_err.h"
#include "obc_types.h"
#include "lora_spi.h"
#include "supply_slave.h"

extern QueueHandle_t filaTelemetria;
static const char *TAG = "TASK_TELEM";

void task_telemetria(void *pvParameters) {
    ESP_LOGI(TAG, "Task de Telemetria Iniciada");
    sensorsData_t dados;
    supply_status_t supply = {0};
    uint32_t packet_count = 0;

    while (1) {
        if (xQueueReceive(filaTelemetria, &dados, pdMS_TO_TICKS(100)) == pdPASS) {
            if (supply_slave_get_status(&supply, 120) == ESP_OK) {
                dados.tempBat1 = supply.temp_bat1;
                dados.tempBat2 = supply.temp_bat2;
                dados.tensao = supply.tensao;
                dados.corrente = supply.corrente;
            }

            packet_count++;
            dados.numPacotes = packet_count;
            dados.rssi = lora_spi_get_last_rssi();

            char payload[300];
            int payload_len = snprintf(
                payload,
                sizeof(payload),
                "Tempo:%lu:Temperatura:%.2f:Umidade:%.2f:Altitude:%.2f:Pressao:%.2f:Latitude:%.6f:Longitude:%.6f:Sats:%d:Roll:%.2f:Pitch:%.2f:Yaw:%.2f:TempBat1:%.2f:TempBat2:%.2f:Tensao:%.2f:Corrente:%.2f:NumPacotes:%lu:RSSI:%d:TamPacote:%u",
                (unsigned long)dados.seconds,
                dados.temperatura,
                dados.umidade,
                dados.altitude,
                dados.pressao,
                dados.latitude,
                dados.longitude,
                dados.sats,
                dados.roll,
                dados.pitch,
                dados.yaw,
                dados.tempBat1,
                dados.tempBat2,
                dados.tensao,
                dados.corrente,
                (unsigned long)dados.numPacotes,
                dados.rssi,
                0U
            );

            if (payload_len <= 0) {
                ESP_LOGW(TAG, "Falha ao montar payload de telemetria");
                continue;
            }

            if (payload_len >= (int)sizeof(payload)) {
                payload_len = (int)sizeof(payload) - 1;
                payload[payload_len] = '\0';
            }

            dados.tamPacote = (uint16_t)payload_len;

            payload_len = snprintf(
                payload,
                sizeof(payload),
                "Tempo:%lu:Temperatura:%.2f:Umidade:%.2f:Altitude:%.2f:Pressao:%.2f:Latitude:%.6f:Longitude:%.6f:Sats:%d:Roll:%.2f:Pitch:%.2f:Yaw:%.2f:TempBat1:%.2f:TempBat2:%.2f:Tensao:%.2f:Corrente:%.2f:NumPacotes:%lu:RSSI:%d:TamPacote:%u",
                (unsigned long)dados.seconds,
                dados.temperatura,
                dados.umidade,
                dados.altitude,
                dados.pressao,
                dados.latitude,
                dados.longitude,
                dados.sats,
                dados.roll,
                dados.pitch,
                dados.yaw,
                dados.tempBat1,
                dados.tempBat2,
                dados.tensao,
                dados.corrente,
                (unsigned long)dados.numPacotes,
                dados.rssi,
                (unsigned)dados.tamPacote
            );

            if (payload_len > 0 && payload_len < (int)sizeof(payload)) {
                esp_err_t tx_err = lora_spi_transmit((const uint8_t *)payload, (size_t)payload_len);
                if (tx_err != ESP_OK) {
                    ESP_LOGW(TAG, "Falha envio LoRa: %s", esp_err_to_name(tx_err));
                } else {
                    ESP_LOGI(TAG, "%s", payload);
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
