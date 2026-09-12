#include "task_tx.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "protocol.h"
#include "lora_sx1278.h"

static const char *TAG = "TASK_TX";

#define TX_LINE_MAX_LEN 96

static void enviar_comando_numerico(uint8_t comando) {
    uint8_t buffer[65];
    uint8_t idx = buildHeader(buffer, TYPE_COMMAND, ADDR_GROUND, ADDR_OBC);
    buffer[idx++] = comando;
    
    if (lora_transmit(buffer, idx)) {
        printf("Comando %d enviado!\n", comando);
    } else {
        printf("Erro ao enviar comando!\n");
    }
    
    // Retorna a receber
    lora_start_receive();
}

static void enviar_comando_texto(const char *texto) {
    uint8_t buffer[HEADER_SIZE + TX_LINE_MAX_LEN];
    size_t textLen = strlen(texto);
    if (textLen > TX_LINE_MAX_LEN) {
        textLen = TX_LINE_MAX_LEN;
    }

    uint8_t idx = buildHeader(buffer, TYPE_COMMAND, ADDR_GROUND, ADDR_OBC);
    memcpy(&buffer[idx], texto, textLen);
    idx += (uint8_t)textLen;

    if (lora_transmit(buffer, idx)) {
        printf("Comando texto enviado: %.*s\n", (int)textLen, texto);
    } else {
        printf("Erro ao enviar comando texto!\n");
    }

    lora_start_receive();
}

void task_tx(void *pvParameters) {
    uint8_t data[32];
    char line[TX_LINE_MAX_LEN + 1];
    int linePos = 0;

    printf("TX pronto. Digite 1..5 para comandos legados ou texto + Enter para comando livre.\n");
    
    while (1) {
        // Le UART (Monitor Serial)
        int length = uart_read_bytes(UART_NUM_0, data, sizeof(data), pdMS_TO_TICKS(50));
        
        if (length > 0) {
            for (int i = 0; i < length; i++) {
                char c = (char)data[i];

                if (c == '\r' || c == '\n') {
                    if (linePos > 0) {
                        line[linePos] = '\0';
                        if (linePos == 1 && line[0] >= '1' && line[0] <= '5') {
                            uint8_t comando = (uint8_t)(line[0] - '0');
                            enviar_comando_numerico(comando);
                        } else {
                            enviar_comando_texto(line);
                        }
                        linePos = 0;
                    }
                    continue;
                }

                if (linePos < TX_LINE_MAX_LEN) {
                    line[linePos++] = c;
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
