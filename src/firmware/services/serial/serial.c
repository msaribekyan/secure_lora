// #include "main.h"
#include "serial.h"
#include "usbd_cdc_if.h"
#include "crc.h"

#include "timing.h"

#define SERIAL_SYNC_BYTE 0x24
#define SERIAL_PARSER_TIMEOUT_MS 100
#define RX_BUFFER_SIZE 128


#define RX_QUEUE_SIZE 3

static serial_packet_t rx_queue[RX_QUEUE_SIZE];
static uint8_t rx_queue_head = 0;
static uint8_t rx_queue_tail = 0;
static uint8_t rx_queue_count = 0;


typedef enum serial_parser_state_e
{
	SERIAL_PARSER_WAIT_SYNC = 0x00u,
	SERIAL_PARSER_READ_TYPE,
	SERIAL_PARSER_READ_MESSAGE,
	SERIAL_PARSER_READ_PARAM_NAME,
	SERIAL_PARSER_READ_PARAM,
	SERIAL_PARSER_READ_CRC1,
	SERIAL_PARSER_READ_CRC2,
} serial_parser_state_t;

typedef struct serial_parser_s
{
	uint32_t last_byte_time;
	uint16_t calculated_crc;
	uint16_t received_crc;
	uint8_t received_length;
	serial_parser_state_t state;

	uint32_t (*get_time_ms)(void);
} serial_parser_t;

typedef enum serial_parser_response_e
{
	SERIAL_PARSER_FRAME_READY = 0x00,
	SERIAL_PARSER_WAITING,
	SERIAL_PARSER_LENGTH_ERROR,
	SERIAL_PARSER_CRC_ERROR,
} serial_parser_response_t;

typedef enum serial_command_e
{
	SERIAL_COMMAND_KEY_EXCHANGE	= 0x00,
	// Other commands go here
} serial_command_t;

serial_parser_t parser = {0};
static serial_packet_t parsed_packet = {0};

static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_buffer_len = 0;

void serial_parser_check_timeout(void)
{
	if (parser.state == SERIAL_PARSER_WAIT_SYNC)
		return;
	if ((parser.get_time_ms() - parser.last_byte_time) < SERIAL_PARSER_TIMEOUT_MS)
		return;
	parser.state = SERIAL_PARSER_WAIT_SYNC;
}

serial_parser_response_t serial_parse_byte(uint8_t byte)
{
	parser.last_byte_time = parser.get_time_ms();

	switch (parser.state)
	{
	case SERIAL_PARSER_WAIT_SYNC:
		if (byte == SERIAL_SYNC_BYTE)
		{
			parser.state = SERIAL_PARSER_READ_TYPE;
			parser.received_crc = 0;
			parser.calculated_crc = crc16_ccitt_init();
			parser.received_length = 0;
		}
		break;
	case SERIAL_PARSER_READ_TYPE:
		parser.calculated_crc = crc16_ccitt_update(byte, parser.calculated_crc);
		if ((byte & 0x80) != 0)
		{
			// Packet is a message
			parser.state = SERIAL_PARSER_READ_MESSAGE;
			parsed_packet.type = SERIAL_PACKET_TYPE_MESSAGE;
			parsed_packet.message.length = byte & 0x7F;
			if (parsed_packet.message.length == 0) // Invalid length
				parser.state = SERIAL_PARSER_WAIT_SYNC;
		}
		else
		{
		/*
			switch (byte)
			{
			case SERIAL_COMMAND_KEY_EXCHANGE:
				parser.state = SERIAL_PARSER_READ_CRC1; 
				break;
			default:
				// Unknown command
				parser.state = SERIAL_PARSER_WAIT_SYNC;
				break;
			}
		*/
			parser.state = SERIAL_PARSER_WAIT_SYNC; // Remove this and implement normal command parser later
		}
		break;
	case SERIAL_PARSER_READ_MESSAGE:
		parser.calculated_crc = crc16_ccitt_update(byte, parser.calculated_crc);
		parsed_packet.message.data[parser.received_length] = byte;
		parser.received_length += 1;
		if (parser.received_length == parsed_packet.message.length)
			parser.state = SERIAL_PARSER_READ_CRC1;
		break;
	/*
	case SERIAL_PARSER_READ_PARAM_NAME:
		
		break;
	case SERIAL_PARSER_READ_PARAM:
		break;
	*/
	case SERIAL_PARSER_READ_CRC1:
		parser.received_crc = (uint16_t) byte;
		parser.state = SERIAL_PARSER_READ_CRC2;
		break;
	case SERIAL_PARSER_READ_CRC2:
		parser.received_crc |= (uint16_t) byte << 8;
		parser.state = SERIAL_PARSER_WAIT_SYNC;
		if (parser.received_crc != parser.calculated_crc)
		{
			return SERIAL_PARSER_CRC_ERROR;
		}
		return SERIAL_PARSER_FRAME_READY;
		break;
	default:
		parser.state = SERIAL_PARSER_WAIT_SYNC;
		break;
	}
	return SERIAL_PARSER_WAITING;
}

void serial_parser_init(uint32_t (*get_time_ms)(void))
{
	parser.get_time_ms = get_time_ms;
	parser.state = SERIAL_PARSER_WAIT_SYNC;
	parser.last_byte_time = get_time_ms();
}

void serial_init(void)
{
	serial_parser_init(get_time_ms);
}

void serial_receive_callback(uint8_t *data, uint8_t len)
{
	if (rx_buffer_len != 0)
		return; // Drops the packet, change later, maybe implement queue
	for (uint8_t i = 0;i < len;i++)
	{
		rx_buffer[i] = data[i];
	}
	rx_buffer_len = len;
}

void serial_process_buffer(void)
{
	serial_parser_check_timeout();

	for (uint8_t i = 0;i < rx_buffer_len;i++)
	{
		if (serial_parse_byte(rx_buffer[i]) == SERIAL_PARSER_FRAME_READY)
		{
			if (rx_queue_count == RX_QUEUE_SIZE)
				continue;
			rx_queue[rx_queue_tail] = parsed_packet;
			rx_queue_tail = (rx_queue_tail + 1) % RX_QUEUE_SIZE;
			rx_queue_count++;
		}
	}
	rx_buffer_len = 0; // Fix this later with queues
}

serial_packet_t *serial_rx_queue_peak(void)
{
	if (rx_queue_count == 0)
		return NULL;
	return &rx_queue[rx_queue_head];
}

void serial_rx_queue_pop(void)
{
	if (rx_queue_count == 0)
		return;
	rx_queue_head = (rx_queue_head + 1) % RX_QUEUE_SIZE;
	rx_queue_count--;
}
/*
void serial_create_packet(serial_packet_t *packet)
{
	packet->length = rx_buffer_len;
	for(uint8_t i = 0;i < rx_buffer_len;i++)
	{
		packet->data[i] = rx_buffer[i];
	}
	rx_buffer_len = 0;
}
*/
/*
void serial_process_buffer(void)
{
	
	serial_packet_t *packet;

	packet = &rx_queue[rx_queue_head];
	packet->length = rx_buffer_len;
	for (uint8_t i = 0;i < rx_buffer_len;i++)
	{
		packet->data[i] = rx_buffer[i];
	}
	rx_queue_count++;
	rx_queue_head = (rx_queue_head + 1) % RX_QUEUE_SIZE;
	rx_buffer_len = 0;
	
}

void serial_process_packet(void)
{
	
}
*/
void serial_transmit(uint8_t *data, uint8_t len)
{
	CDC_Transmit_FS(data, len);
}

bool serial_rx_ready(void)
{
	if (rx_buffer_len != 0)
		return true;
	return false;
}

bool serial_rxne(void)
{
	if (rx_queue_count != 0)
		return true;
	return false;
}

