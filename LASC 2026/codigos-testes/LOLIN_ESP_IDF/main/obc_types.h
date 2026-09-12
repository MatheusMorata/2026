#pragma once
#include <stdint.h>

typedef struct {
    uint32_t seconds;
    float temperatura;
    float umidade;
    float pressao;
    float altitude;
    float latitude;
    float longitude;
    int sats;
    float roll;
    float pitch;
    float yaw;
    float tempBat1;
    float tempBat2;
    float tensao;
    float corrente;
    uint32_t numPacotes;
    int rssi;
    uint16_t tamPacote;
} sensorsData_t;

typedef struct {
    sensorsData_t sensor;
    char controle[64];
} respost_t;
