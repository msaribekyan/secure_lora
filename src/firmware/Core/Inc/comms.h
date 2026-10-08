#ifndef COMMS_H
#define COMMS_H

#include "lora.h"

#define MAX_PAYLOAD_LENGTH 64
#define MAX_PACKET_SIZE MAX_PAYLOAD_LENGTH + 5

#define PACKET_SYNC_BYTE 0x42

#define TX_QUEUE_SIZE 4

typedef struct lora_packet_s
{
	uint8_t length;
	uint8_t data[MAX_PACKET_SIZE];
} lora_packet_t;

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

void comms_init(
	SPI_HandleTypeDef *hspi,
	GPIO_TypeDef *cs_port,
	uint16_t cs_pin,
	GPIO_TypeDef *reset_port,
	uint16_t reset_pin
);
void usb_receive_callback(uint8_t *data, uint8_t len);
uint8_t receive_packet(lora_packet_t *packet);
void transmit_packet(lora_packet_t *packet);
uint8_t comms_check_usb_rxne(void);
uint8_t comms_check_radio_rxne(void);
uint8_t comms_check_radio_txne(void);


uint8_t radio_tx_queue_add(lora_packet_t packet);
uint8_t radio_tx_queue_remove(lora_packet_t *packet);

uint8_t process_usb_packet(lora_packet_t *packet);;

uint8_t check_radio_version(void);

lora_packet_t encrypt_packet(lora_packet_t packet);

#endif // COMMS_H
