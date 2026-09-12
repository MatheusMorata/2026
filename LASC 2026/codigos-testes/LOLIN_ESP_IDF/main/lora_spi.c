#include "lora_spi.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_SCK  18
#define LORA_CS   5

static const char *TAG = "LORA_SPI";
static int s_last_rssi = -120;

void lora_spi_init(void) {
    spi_bus_config_t buscfg = {
        .miso_io_num = LORA_MISO,
        .mosi_io_num = LORA_MOSI,
        .sclk_io_num = LORA_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 256
    };
    spi_bus_initialize(VSPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_LOGI(TAG, "SPI do LoRa Inicializado");
}

esp_err_t lora_spi_transmit(const uint8_t *data, size_t len) {
    if (!data || len == 0) return ESP_ERR_INVALID_ARG;

    ESP_LOGI(TAG, "TX LoRa (%u bytes): %.*s", (unsigned)len, (int)len, (const char *)data);
    return ESP_OK;
}

int lora_spi_get_last_rssi(void) {
    return s_last_rssi;
}
