#include "radio.h"
#include "radio_link.h"
#include "lora.h"

#include "spi.h"
#include "gpio.h"

#include "timing.h"

#define RX_BUFFER_SIZE 128
#define TX_BUFFER_SIZE 128
#define RX_QUEUE_SIZE 2
#define TX_QUEUE_SIZE 2

static lora_t lora = {
	.hspi = &hspi1,
	.cs_port = LORA_NSS_GPIO_Port,
	.cs_pin = LORA_NSS_Pin,
	.reset_port = LORA_RST_GPIO_Port,
	.reset_pin = LORA_RST_Pin,
};

static frame_t rx_queue[RX_QUEUE_SIZE];
static uint8_t rx_queue_head = 0;
static uint8_t rx_queue_tail = 0;
static uint8_t rx_queue_count = 0;

static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_buffer_len = 0;

static uint8_t tx_buffer[TX_BUFFER_SIZE];
static uint8_t tx_buffer_len = 0;

static packet_t tx_queue[TX_QUEUE_SIZE];
static uint8_t tx_queue_head = 0;
static uint8_t tx_queue_tail = 0;
static uint8_t tx_queue_count = 0;

void radio_init(void)
{
	lora_init(&lora);
	parser_init(get_time_ms);
	lora_setup_rx(&lora);
}

static packet_status_t encode_packet(packet_t *packet, frame_t *frame)
{
	switch (packet->type)
	{
	case PACKET_TYPE_MESSAGE:
		frame->length = 23 + packet->message.ciphertext_length;
		frame->data[0] = (uint8_t) PACKET_TYPE_MESSAGE;
		frame->data[1] = packet->message.id;
		frame->data[2] = (uint8_t) (packet->message.counter >> 24) & 0xFF;
		frame->data[3] = (uint8_t) (packet->message.counter >> 16) & 0xFF;
		frame->data[4] = (uint8_t) (packet->message.counter >> 8) & 0xFF;
		frame->data[5] = (uint8_t) (packet->message.counter) & 0xFF;
		frame->data[6] = packet->message.ciphertext_length;
		for (uint8_t i = 0;i < packet->message.ciphertext_length;i++)
		{
			frame->data[7 + i] = packet->message.ciphertext[i];
		}
		for (uint8_t i = 0;i < 16;i++)
		{
			frame->data[7 + packet->message.ciphertext_length + i] = 
				packet->message.tag[i];
		}		
		break;
	// Other packets go here
	default:
		return PACKET_ERROR;
	}
	return PACKET_OK;
}

static packet_status_t parse_frame(frame_t *frame, packet_t *packet)
{	
	if (frame->length == 0)
		return PACKET_ERROR;
	switch (frame->data[0])
	{
	case PACKET_TYPE_MESSAGE:
		packet->type = PACKET_TYPE_MESSAGE;
		if (frame->length < 23)
			return PACKET_ERROR;
		packet->message.id = frame->data[1];
		packet->message.counter = (uint32_t) frame->data[2] << 24;
		packet->message.counter |= (uint32_t) frame->data[3] << 16;
		packet->message.counter |= (uint32_t) frame->data[4] << 8;
		packet->message.counter |= (uint32_t) frame->data[5];
		packet->message.ciphertext_length = frame->data[6];
		// if (frame->length != 23 + packet->message.ciphertext_length)
		// 	return PACKET_ERROR; // Wrong packet length
		for (uint8_t i = 0;i < packet->message.ciphertext_length;i++)
		{
			packet->message.ciphertext[i] = frame->data[7 + i];
		}
		for (uint8_t i = 0;i < 16;i++)
		{
			packet->message.tag[i] =
				frame->data[7 + packet->message.ciphertext_length + i];
		}
		break;
	// Other packets go here
	default:
		return PACKET_ERROR;
	}
	return PACKET_OK;
}

packet_status_t radio_process_frame(packet_t *packet)
{
	packet_status_t status;

	if (rx_queue_count == 0)
		return PACKET_ERROR;
	status = parse_frame(&rx_queue[rx_queue_head], packet);
	rx_queue_count--;
	rx_queue_head = (rx_queue_head + 1) % RX_QUEUE_SIZE;
	return status;
}

static void radio_parse_data(uint8_t *data, uint8_t length)
{
	parser_response_t resp;
	const frame_t *frame;

	parser_check_timeout();
	for (uint8_t i = 0;i < length;i++)
	{
		resp = parse_byte(data[i]);
		if (resp == PARSER_FRAME_READY)
		{
			frame = parser_get_frame();
			if (rx_queue_count != RX_QUEUE_SIZE)
			{
				rx_queue[rx_queue_tail] = *frame; // TODO: better copy
				rx_queue_tail = (rx_queue_tail + 1) % RX_QUEUE_SIZE;
				rx_queue_count++;
			}
		}
	}
}

void radio_queue_packet(packet_t *packet)
{
	if (tx_queue_count == TX_QUEUE_SIZE)
		return;
	tx_queue[tx_queue_tail] = *packet; // Improve this some time
	tx_queue_tail = (tx_queue_tail + 1) % TX_QUEUE_SIZE;
	tx_queue_count++;
}

void radio_transmit_queue(void)
{
	packet_t *packet;
	packet_status_t status;
	frame_t frame;

	if (tx_queue_count == 0)
		return;
	packet = &tx_queue[tx_queue_head];
	status = encode_packet(packet, &frame);
	tx_queue_count--;
	tx_queue_head = (tx_queue_head + 1) % TX_QUEUE_SIZE;
	if (status != PACKET_OK)
		return; // Invalid something
	// transmit
	tx_buffer_len = serialize_frame(&frame, tx_buffer);
	if (tx_buffer_len == 0)
		return;
	lora_setup_tx(&lora);
	lora_transmit(&lora, tx_buffer, tx_buffer_len);
	lora_setup_rx(&lora);
	tx_buffer_len = 0;
}

void radio_process_buffer(void)
{
	radio_parse_data(rx_buffer, rx_buffer_len);
	rx_buffer_len = 0;
}

void radio_receive_data(void)
{
	rx_buffer_len = lora_receive(&lora, rx_buffer, RX_BUFFER_SIZE);
}

bool radio_rx_ready(void)
{
	return lora_check_rx(&lora);
}

bool radio_rxne(void)
{
	if (rx_queue_count != 0)
		return true;
	return false;
}

bool radio_txne(void)
{
	if (tx_queue_count != 0)
		return true;
	return false;
}
