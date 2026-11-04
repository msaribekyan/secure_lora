// #include "main.h"
#include "serial.h"
#include "usbd_cdc_if.h"

#define RX_BUFFER_SIZE 128
#define RX_QUEUE_SIZE 5
/*
static serial_packet_t rx_queue[RX_QUEUE_SIZE];
static uint8_t rx_queue_head = 0;
static uint8_t rx_queue_tail = 0;
static uint8_t rx_queue_count = 0;
*/

static uint8_t rx_buffer[RX_BUFFER_SIZE];
static uint8_t rx_buffer_len = 0;

void serial_init(void)
{
	// nothing to do, for now
	return;	
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

void serial_create_packet(serial_packet_t *packet)
{
	packet->length = rx_buffer_len;
	for(uint8_t i = 0;i < rx_buffer_len;i++)
	{
		packet->data[i] = rx_buffer[i];
	}
	rx_buffer_len = 0;
}
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
	//if (rx_queue_count != 0)
	//	return true;
	return false;
}
