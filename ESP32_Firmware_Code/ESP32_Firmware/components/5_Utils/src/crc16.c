#include "crc16.h"

uint16_t CRC16_Calculate(const uint8_t *data, size_t length) {
    uint16_t crc = 0x0000;
    
    while (length--) {
        crc ^= (uint16_t)(*data++) << 8;
        for (int i = 0; i < 8; i++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}
