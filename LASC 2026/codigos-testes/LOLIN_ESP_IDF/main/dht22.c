#include "dht22.h"

#include <stdint.h>
#include "esp_rom_sys.h"

static gpio_num_t s_pin = GPIO_NUM_NC;

static inline int wait_level(int level, uint32_t timeout_us) {
    uint32_t waited = 0;
    while (gpio_get_level(s_pin) == level) {
        if (waited++ >= timeout_us) return -1;
        esp_rom_delay_us(1);
    }
    return (int)waited;
}

esp_err_t dht22_init(gpio_num_t pin) {
    if (pin < 0) return ESP_ERR_INVALID_ARG;
    s_pin = pin;

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << s_pin),
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };

    return gpio_config(&io_conf);
}

esp_err_t dht22_read(float *temperature_c, float *humidity_pct) {
    if (!temperature_c || !humidity_pct) return ESP_ERR_INVALID_ARG;
    if (s_pin == GPIO_NUM_NC) return ESP_ERR_INVALID_STATE;

    uint8_t data[5] = {0};

    gpio_set_direction(s_pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(s_pin, 0);
    esp_rom_delay_us(1200);
    gpio_set_level(s_pin, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(s_pin, GPIO_MODE_INPUT);

    if (wait_level(1, 100) < 0) return ESP_ERR_TIMEOUT;
    if (wait_level(0, 100) < 0) return ESP_ERR_TIMEOUT;
    if (wait_level(1, 100) < 0) return ESP_ERR_TIMEOUT;

    for (int i = 0; i < 40; i++) {
        if (wait_level(0, 80) < 0) return ESP_ERR_TIMEOUT;
        int high_time = wait_level(1, 120);
        if (high_time < 0) return ESP_ERR_TIMEOUT;

        data[i / 8] <<= 1;
        if (high_time > 40) data[i / 8] |= 1;
    }

    uint8_t checksum = (uint8_t)(data[0] + data[1] + data[2] + data[3]);
    if (checksum != data[4]) return ESP_ERR_INVALID_CRC;

    uint16_t raw_h = (uint16_t)((data[0] << 8) | data[1]);
    uint16_t raw_t = (uint16_t)((data[2] << 8) | data[3]);

    float humidity = raw_h / 10.0f;
    float temperature = (raw_t & 0x7FFF) / 10.0f;
    if (raw_t & 0x8000) temperature = -temperature;

    *temperature_c = temperature;
    *humidity_pct = humidity;

    return ESP_OK;
}
