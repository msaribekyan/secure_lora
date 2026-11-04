#ifndef CRC_H
#define CRC_H

#include <stdint.h>

uint16_t crc16_ccitt_init(void);
uint16_t crc16_ccitt_update(uint8_t byte, uint16_t crc);

#endif // CRC_H
