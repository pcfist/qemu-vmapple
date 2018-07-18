#include <stdint.h>
#include "crc16.h"

static uint16_t crc16(uint8_t *message, int nBytes,
        uint16_t remainder, uint16_t polynomial)
{
        int byte;
        uint8_t bit;

    for (byte = 0; byte < nBytes; ++byte) {
        remainder ^= (message[byte] << 8);
        for (bit = 8; bit > 0; --bit) {
            if (remainder & 0x8000) {
                remainder = (remainder << 1) ^ polynomial;
            } else {
                remainder = (remainder << 1);
            }
        }
    }
    return remainder;
}

uint16_t crc16ccitt_xmodem(uint8_t *message, int nBytes) {
    return crc16(message, nBytes, 0x0000, 0x1021);
}
