#pragma once
#include <stdint.h>
#include <stdbool.h>

#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_SCK  18
#define LORA_CS   5
#define LORA_RST  14
#define LORA_DIO0 2

void lora_sx1278_init(void);
void lora_start_receive(void);
bool lora_transmit(uint8_t *data, uint8_t len);
bool lora_check_rx(uint8_t *buffer, uint8_t *len, float *rssi, float *snr);
