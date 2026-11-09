#include "radio.h"
#include "serial.h"
#include "led.h"
#include "timing.h"

// radio_packet_t tmp_lora_packet = {0};
// uint8_t status = 0;

static void app_receive_serial_packet(void)
{
	serial_packet_t *serial_packet;
	packet_t radio_packet;

	serial_process_buffer();
	
	if (serial_rxne())
	{
		serial_packet = serial_rx_queue_peak();

		switch (serial_packet->type)
		{
		case SERIAL_PACKET_TYPE_MESSAGE:
			radio_packet.type = PACKET_TYPE_MESSAGE;
			radio_packet.message.ciphertext_length = serial_packet->message.length;
			for (uint8_t i = 0;i < serial_packet->message.length;i++)
			{
				radio_packet.message.ciphertext[i] = serial_packet->message.data[i];
			}
			radio_queue_packet(&radio_packet);
			break;
		default:
			break;
		}
		serial_rx_queue_pop();
	}
	// TODO: receive usb packet, parse and do whatever needs to be done
	// check if message type, ecrypt and stuff and add to transmit queue
	// packet_t packet;

	//if (!serial_rx_ready())
	//	return;
	// serial_packet_t packet = {0};
	// packet_t radio_packet = {0};

	// serial_create_packet(&packet);
	//if (packet.length == 0)
	//	return;

	/* 
		*/
}
/*
static void app_process_serial_packet(void)
{

}
*/

static void app_receive_radio_packet(void)
{
	radio_receive_data();
	radio_process_buffer();
}

static void app_process_radio_packet(void)
{
	packet_t packet;

	if (radio_process_frame(&packet) != PACKET_OK)
		return;

	switch (packet.type)
	{
	case PACKET_TYPE_MESSAGE:
		// TODO: decrypt
		serial_transmit(
			packet.message.ciphertext,
			packet.message.ciphertext_length
		); // TODO: fix the send over usb
		break;
	// TODO: key exchange algorithm
	default:
		break;
	}
}

void app_init(void)
{
	radio_init();
	serial_init();
	led_on();
	uint32_t start = get_time_ms();
	while (get_time_ms() - start < 1000);
	led_off();
}

void app_loop(void)
{
	if (radio_rx_ready()) // Packet available for receive
	{
		led_on();
		app_receive_radio_packet();
		led_off();
	}
	else if (radio_txne()) // Packet available for transmit
	{
		radio_transmit_queue();
	}
	if (radio_rxne()) // Radio packet available for processing
	{
		app_process_radio_packet();
	}
	if (serial_rx_ready()) // Serial packet available to receive
	{
		led_on();
		app_receive_serial_packet();
		led_off();
	}
	/*
	if (serial_rxne()) // Serial packet available for processing
	{
		app_process_serial_packet();
	}
	*/
}

