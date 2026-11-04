#include <stdint.h>

uint16_t crc16_ccitt_init(void) {
	return 0xFFFF;
}

uint16_t crc16_ccitt_update(uint8_t byte, uint16_t crc) {
	crc ^= (uint16_t) byte << 8;
	for (uint8_t i = 0; i < 8; i++) {
		if (crc & 0x8000)
			crc = (crc << 1 ) ^ 0x1021;
		else
			crc = crc << 1;
	}
	return crc;
}
