#include "radio.h"
#include "serial.h"
#include "led.h"

// radio_packet_t tmp_lora_packet = {0};
// uint8_t status = 0;

static void app_receive_serial_packet(void)
{
	// TODO: receive usb packet, parse and do whatever needs to be done
	// check if message type, ecrypt and stuff and add to transmit queue
	// packet_t packet;

	//if (!serial_rx_ready())
	//	return;
	serial_packet_t packet = {0};
	packet_t radio_packet = {0};

	serial_create_packet(&packet);
	if (packet.length == 0)
		return;

	radio_packet.type = PACKET_TYPE_MESSAGE;
	radio_packet.message.ciphertext_length = packet.length;
	for (uint8_t i = 0;i < packet.length;i++)
	{
		radio_packet.message.ciphertext[i] = packet.data[i];
	}
	radio_queue_packet(&radio_packet);
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

