#ifndef RADIO_LINK_H
#define RADIO_LINK_H

#include <stdint.h>

#define MAX_FRAME_LENGTH 128

typedef struct frame_s
{
	uint8_t length;
	uint8_t data[MAX_FRAME_LENGTH];
} frame_t;

typedef enum parser_response_e
{
	PARSER_FRAME_READY = 0x00,
	PARSER_WAITING,
	PARSER_LENGTH_ERROR,
	PARSER_CRC_ERROR,
} parser_response_t;

void parser_init(uint32_t (*get_time_ms)(void));
void parser_check_timeout(void);
uint8_t serialize_frame(frame_t *frame, uint8_t *data);
parser_response_t parse_byte(uint8_t byte);
const frame_t *parser_get_frame(void);

#endif // RADIO_LINK_H
