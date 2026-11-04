#include "crc.h"
#include "radio_link.h"

#define SYNC_BYTE 0x42
#define PARSER_TIMEOUT_MS 1000

typedef enum parser_state_e
{
	PARSER_WAIT_SYNC = 0x00u,
	PARSER_READ_LENGTH,
	PARSER_READ_DATA,
	PARSER_READ_CRC1,
	PARSER_READ_CRC2,
} parser_state_t;

typedef struct parser_s
{
	uint32_t last_byte_time;
	uint16_t calculated_crc;
	uint16_t received_crc;
	uint8_t received_length;
	parser_state_t state;

	uint32_t (*get_time_ms)(void);
} parser_t;

static parser_t parser = {0};
static frame_t parsed_frame = {0};

void parser_init(uint32_t (*get_time_ms)(void))
{
	parser.get_time_ms = get_time_ms;
	parser.state = PARSER_WAIT_SYNC;
	parser.last_byte_time = get_time_ms();
}

void parser_check_timeout(void)
{
	if (parser.state == PARSER_WAIT_SYNC)
		return;
	if ((parser.get_time_ms() - parser.last_byte_time) < PARSER_TIMEOUT_MS)
		return;
	parser.state = PARSER_WAIT_SYNC;
}

const frame_t *parser_get_frame(void)
{
    return &parsed_frame;
}

uint8_t serialize_frame(frame_t *frame, uint8_t *data)
{
	uint16_t crc;

	crc = crc16_ccitt_init();
	data[0] = SYNC_BYTE;
	data[1] = frame->length;
	crc = crc16_ccitt_update(frame->length, crc);
	for (uint8_t i = 0;i < frame->length;i++)
	{
		data[2 + i] = frame->data[i];
		crc = crc16_ccitt_update(frame->data[i], crc);
	}
	// crc = crc16_ccitt_finalize(crc);
	data[2 + frame->length] = (uint8_t) crc & 0xFF;
	data[3 + frame->length] = (uint8_t) (crc >> 8) & 0xFF;
	return frame->length + 4;
}

parser_response_t parse_byte(uint8_t byte)
{
	parser.last_byte_time = parser.get_time_ms();

	switch (parser.state)
	{
	case PARSER_WAIT_SYNC:
		if (byte == SYNC_BYTE)
		{
			parser.state = PARSER_READ_LENGTH;
			parser.received_crc = 0;
			parser.calculated_crc = crc16_ccitt_init();
			parser.received_length = 0;
		}
		break;
	case PARSER_READ_LENGTH:
		if (byte == 0 || byte > MAX_FRAME_LENGTH)
		{
			parser.state = PARSER_WAIT_SYNC;
			return PARSER_LENGTH_ERROR;
		}
		parsed_frame.length = byte;
		parser.calculated_crc = crc16_ccitt_update(byte, parser.calculated_crc);
		parser.state = PARSER_READ_DATA;
		break;
	case PARSER_READ_DATA:
		parsed_frame.data[parser.received_length] = byte;
		parser.calculated_crc = crc16_ccitt_update(byte, parser.calculated_crc);
		parser.received_length += 1;
		if (parser.received_length >= parsed_frame.length)
		{
			parser.state = PARSER_READ_CRC1;
		}
		break;
	case PARSER_READ_CRC1:
		parser.received_crc = (uint16_t) byte;
		parser.state = PARSER_READ_CRC2;
		break;
	case PARSER_READ_CRC2:
		parser.received_crc |= (uint16_t) byte << 8;
		parser.state = PARSER_WAIT_SYNC;
		if (parser.received_crc != parser.calculated_crc)
		{
			return PARSER_CRC_ERROR;
		}
		return PARSER_FRAME_READY;
		break;
	default:
		break;
	}
	return PARSER_WAITING;
}
