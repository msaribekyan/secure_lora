#ifndef RADIO_H
#define RADIO_H

#include <stdbool.h>
#include <stdint.h>

#define MAX_MESSAGE_CIPHERTEXT_SIZE 32

typedef enum packet_type_e
{
	PACKET_TYPE_MESSAGE = 0x00,
	// Key exchange packets
} packet_type_t;

typedef struct packet_message_s
{
	uint32_t counter;
	uint8_t id;
	uint8_t ciphertext_length;
	uint8_t ciphertext[MAX_MESSAGE_CIPHERTEXT_SIZE];
	uint8_t tag[16];
} packet_message_t;

typedef struct packet_s
{
	packet_type_t type;
	union
	{
		packet_message_t message;
		// Other packets here
	};
} packet_t;

typedef enum packet_status_e
{
	PACKET_OK = 0x00u,
	PACKET_ERROR,
} packet_status_t;

void radio_init(void);

packet_status_t radio_process_frame(packet_t *packet);

void radio_receive_data(void);
void radio_process_buffer(void);

void radio_queue_packet(packet_t *packet);
void radio_transmit_queue(void);

bool radio_rx_ready(void);
bool radio_rxne(void);
bool radio_txne(void);

#endif // RADIO_H
