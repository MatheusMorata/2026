#include "task_rx.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "protocol.h"
#include "lora_sx1278.h"

static const char *TAG = "TASK_RX";

#define MAX_IMAGE_SIZE       46000
#define IMAGE_CHUNK_TIMEOUT  5000
#define TEXT_FRAME_MAX_LEN   255

uint8_t  imageBuffer[MAX_IMAGE_SIZE];
uint32_t imageWritePos        = 0;
uint8_t  imageExpectedChunks  = 0;
uint8_t  imageNextChunk       = 0;
bool     imageInProgress      = false;
uint64_t imageLastChunkMillis = 0;

typedef struct { 
    uint32_t totalRecebidos; 
    uint32_t sensorCount; 
    uint32_t imageCount; 
    uint32_t commandCount; 
    uint32_t debugCount;
    uint32_t desconhecidos; 
    uint32_t startByteInvalido; 
    uint32_t pacotesIncompletos; 
    uint32_t comandosEnviados; 
    uint32_t imagensCompletas;
    uint32_t imagensDescartadas;
    float ultimoRSSI; 
    float ultimoSNR; 
} Stats;

static Stats stats = {0};

static uint64_t get_millis() {
    return esp_timer_get_time() / 1000ULL;
}

static void resetImagem() {
    imageInProgress = false;
    imageWritePos = 0;
    imageExpectedChunks = 0;
    imageNextChunk = 0;
}

static void finalizarImagem() {
    printf("IMAGE_BEGIN\n");
    printf("SIZE:%lu\n", (unsigned long)imageWritePos);
    
    // UART write raw buffer
    fwrite(imageBuffer, 1, imageWritePos, stdout);
    fflush(stdout);

    printf("\nIMAGE_END\n");
    stats.imagensCompletas++;
    resetImagem();
}

static void verificarTimeoutImagem() {
    if(imageInProgress && (get_millis() - imageLastChunkMillis > IMAGE_CHUNK_TIMEOUT)) {
        printf("Imagem incompleta descartada (timeout)\n");
        stats.imagensDescartadas++;
        resetImagem();
    }
}

static bool is_text_telemetry_frame(const uint8_t *buf, uint16_t len) {
    static const char prefix[] = "Tempo:";
    if (len < (sizeof(prefix) - 1)) {
        return false;
    }
    return memcmp(buf, prefix, sizeof(prefix) - 1) == 0;
}

static void print_text_frame(const uint8_t *buf, uint16_t len) {
    char text[TEXT_FRAME_MAX_LEN + 1];
    uint16_t copyLen = (len > TEXT_FRAME_MAX_LEN) ? TEXT_FRAME_MAX_LEN : len;

    memcpy(text, buf, copyLen);
    text[copyLen] = '\0';

    while (copyLen > 0 && (text[copyLen - 1] == '\r' || text[copyLen - 1] == '\n')) {
        text[copyLen - 1] = '\0';
        copyLen--;
    }

    printf("%s\n", text);
}

void task_rx(void *pvParameters) {
    uint8_t rx_buf[256];
    uint8_t rx_len = 0;
    
    lora_start_receive();

    while (1) {
        verificarTimeoutImagem();

        if (lora_check_rx(rx_buf, &rx_len, &stats.ultimoRSSI, &stats.ultimoSNR)) {
            if (rx_len == 0) {
                stats.pacotesIncompletos++;
                continue;
            }

            // Compatibilidade com o transmissor atual: telemetria em texto rotulado.
            if (is_text_telemetry_frame(rx_buf, rx_len)) {
                stats.totalRecebidos++;
                stats.sensorCount++;
                print_text_frame(rx_buf, rx_len);
                continue;
            }
            
            if (!validateHeader(rx_buf, rx_len)) {
                stats.pacotesIncompletos++;
                continue;
            }

            if (rx_buf[0] != START_BYTE) {
                stats.startByteInvalido++;
                continue;
            }

            stats.totalRecebidos++;
            uint8_t type = packetType(rx_buf);
            const uint8_t* payload = packetPayload(rx_buf);
            uint16_t payloadLen = packetPayloadSize(rx_len);

            if (type == TYPE_RESPOST) {
                struct respost resp;
                if (!parseRespost(payload, payloadLen, &resp)) {
                    if (is_text_telemetry_frame(payload, payloadLen)) {
                        stats.sensorCount++;
                        print_text_frame(payload, payloadLen);
                        continue;
                    }

                    printf("Pacote RESPOST incompleto!\n");
                    stats.pacotesIncompletos++;
                    continue;
                }
                stats.sensorCount++;
                
                float temperatura = resp.sensor.temperatura / 100.0f;
                float umidade     = resp.sensor.umidade / 100.0f;
                float altitude    = resp.sensor.altitude / 10.0f;
                float pressao     = resp.sensor.pressao;
                float latitude    = resp.sensor.latitude / 10000000.0f;
                float longitude   = resp.sensor.longitude / 10000000.0f;
                float roll_deg    = resp.sensor.roll  / 100.0f;
                float pitch_deg   = resp.sensor.pitch / 100.0f;
                float yaw_deg     = resp.sensor.yaw   / 100.0f;

                // Formato AbaTrack Exato (printf stdout mapeia para USB/Serial do ESP32)
                printf("%lu:%.2f:%.2f:%.1f:%lu:%.7f:%.7f:%u:%.2f:%.2f:%.2f:%s:%s:%s:%s:%lu:%.2f:%u\n",
                    (unsigned long)resp.sensor.seconds, temperatura, umidade, altitude,
                    (unsigned long)pressao, latitude, longitude, resp.sensor.sats,
                    roll_deg, pitch_deg, yaw_deg,
                    resp.sensor.tempBat1, resp.sensor.tempBat2, resp.sensor.tensao, resp.sensor.corrente,
                    (unsigned long)stats.totalRecebidos, stats.ultimoRSSI, rx_len);
                
                if (strlen(resp.controle) > 0) {
                    printf("CTRL:%s\n", resp.controle);
                }
            }
            else if (type == TYPE_IMAGE) {
                stats.imageCount++;
                if (payloadLen < 3) continue;

                uint8_t chunkIndex = payload[0];
                uint8_t totalChunk = payload[1];
                uint8_t dataLen    = payload[2];
                const uint8_t* chunkData = payload + 3;

                printf("\n===== IMAGEM =====\nChunk: %d/%d\nDados: %d bytes\n", chunkIndex + 1, totalChunk, dataLen);

                if(chunkIndex == 0) {
                    resetImagem();
                    imageInProgress = true;
                    imageExpectedChunks = totalChunk;
                }

                if(imageInProgress && chunkIndex == imageNextChunk && totalChunk == imageExpectedChunks) {
                    if (imageWritePos + dataLen <= MAX_IMAGE_SIZE) {
                        memcpy(imageBuffer + imageWritePos, chunkData, dataLen);
                        imageWritePos += dataLen;
                        imageNextChunk++;
                        imageLastChunkMillis = get_millis();

                        if (imageNextChunk == totalChunk) {
                            finalizarImagem();
                        }
                    }
                }
            }
            else if (type == TYPE_DEBUG) {
                stats.debugCount++;
                char debugMsg[DBG_MSG_MAX_LEN];
                parseDebugMessage(payload, payloadLen, debugMsg, sizeof(debugMsg));
                printf("\n===== DEBUG (OBC) =====\n%s\n", debugMsg);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
