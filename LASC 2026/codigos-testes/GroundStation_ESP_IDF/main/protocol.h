#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <string.h>

#define START_BYTE   0x7E

#define TYPE_SENSOR  0x01
#define TYPE_GPS     0x02
#define TYPE_GYRO    0x03
#define TYPE_RESPOST 0x04
#define TYPE_IMAGE   0x10
#define TYPE_DEBUG   0x20
#define TYPE_COMMAND 0x30

#define ADDR_GROUND  0x01
#define ADDR_OBC     0x02

#define HEADER_SIZE  4
#define SLAVE_STR_LEN  16
#define CTRL_STR_LEN   64

#pragma pack(push, 1)
struct sensorsData {
    uint32_t seconds;
    int16_t temperatura;
    int16_t umidade;
    int16_t altitude;
    uint32_t pressao;
    int32_t latitude;
    int32_t longitude;
    uint8_t sats;
    int16_t roll;
    int16_t pitch;
    int16_t yaw;
    char tempBat1[SLAVE_STR_LEN];
    char tempBat2[SLAVE_STR_LEN];
    char tensao[SLAVE_STR_LEN];
    char corrente[SLAVE_STR_LEN];
};

struct respost {
    struct sensorsData sensor;
    char controle[CTRL_STR_LEN];
};
#pragma pack(pop)

#define DBG_MSG_MAX_LEN 200

inline uint8_t buildHeader(uint8_t* buf, uint8_t type, uint8_t src, uint8_t dst) {
    buf[0] = START_BYTE;
    buf[1] = type;
    buf[2] = src;
    buf[3] = dst;
    return HEADER_SIZE;
}

inline bool validateHeader(const uint8_t* buf, uint16_t size) {
    if (size < HEADER_SIZE) return false;
    if (buf[0] != START_BYTE) return false;
    return true;
}

inline uint8_t packetType(const uint8_t* buf) { return buf[1]; }
inline uint8_t packetSrc(const uint8_t* buf) { return buf[2]; }
inline uint8_t packetDst(const uint8_t* buf) { return buf[3]; }
inline const uint8_t* packetPayload(const uint8_t* buf) { return &buf[HEADER_SIZE]; }
inline uint16_t packetPayloadSize(uint16_t totalSize) { return (totalSize > HEADER_SIZE) ? (totalSize - HEADER_SIZE) : 0; }

inline bool parseRespost(const uint8_t* payload, uint16_t payloadSize, struct respost* out) {
    if (payloadSize < sizeof(struct respost)) return false;
    memcpy(out, payload, sizeof(struct respost));
    return true;
}

inline uint16_t parseDebugMessage(const uint8_t* payload, uint16_t payloadSize, char* out, uint16_t outSize) {
    if (payloadSize == 0 || outSize == 0) return 0;
    uint16_t copyLen = (payloadSize < outSize - 1) ? payloadSize : (outSize - 1);
    memcpy(out, payload, copyLen);
    out[copyLen] = '\0';
    return copyLen;
}
#endif
