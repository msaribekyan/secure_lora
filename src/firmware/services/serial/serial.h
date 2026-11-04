#ifndef COMMS_H
#define COMMS_H

#include <stdbool.h>
#include <stdint.h>

// #define MAX_PAYLOAD_LENGTH 64
// #define MAX_PACKET_SIZE MAX_PAYLOAD_LENGTH + 5

#define SERIAL_SYNC_BYTE 0x42
#define MAX_PACKET_SIZE 128
#define MAX_MESSAGE_SIZE 32
/*
typedef struct radio_packet_s
{
	uint8_t length;
	uint8_t data[MAX_PACKET_SIZE];
} radio_packet_t;
*/
/*
typedef enum packet_error_e
{
	PACKET_OK = 0x00u,
	PACKET_TOO_SMALL,
	PACKET_NO_SYNC,
	PACKET_OUT_OF_BOUND,
	PACKET_INVALID_CRC,
} packet_error_t;
*/

/*
typedef enum serial_packet_type_e
{
	SERIAL_PACKET_TYPE_MESSAGE = 0x00u,
	// Command packets go here
} serial_packet_type_t;

typedef struct serial_message_packet_s
{
	uint8_t length;
	uint8_t message[MAX_MESSAGE_SIZE];
} serial_message_packet_t;

typedef struct serial_packet_s
{
	serial_packet_type_t type;
	union 
	{
		serial_message_packet_t message;
		// Command packets go here
	} content;
} serial_packet_t;
*/

typedef struct serial_packet_s
{
	uint8_t length;
	uint8_t data[MAX_PACKET_SIZE];
} serial_packet_t;

void serial_init(void);
void serial_receive_callback(uint8_t *data, uint8_t len);
bool serial_rx_ready(void);
bool serial_rxne(void);
void serial_create_packet(serial_packet_t *packet);

void serial_parse_rx(void);

void serial_transmit(uint8_t *data, uint8_t len);

/*
uint8_t receive_packet(radio_packet_t *packet);
void transmit_packet(radio_packet_t *packet);
uint8_t comms_check_usb_rxne(void);
uint8_t comms_check_radio_rxne(void);
uint8_t comms_check_radio_txne(void);


uint8_t radio_tx_queue_add(radio_packet_t packet);
uint8_t radio_tx_queue_remove(radio_packet_t *packet);

uint8_t process_usb_packet(radio_packet_t *packet);;

uint8_t check_radio_version(void);

radio_packet_t encrypt_packet(radio_packet_t packet);
radio_packet_t decrypt_packet(radio_packet_t packet);
*/

#endif // COMMS_H
