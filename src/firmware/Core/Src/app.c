#include "usbd_cdc_if.h"
#include "comms.h"
#include "lora.h"

radio_packet_t tmp_lora_packet = {0};
uint8_t status = 0;

void app_init(void)
{
	comms_init();
}

void app_loop(void)
{
if (comms_check_radio_rxne() == 1) // check if there is a packet to receive
	{
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, SET);
		status = receive_packet(&tmp_lora_packet);
		if (status == 1)
		{
			tmp_lora_packet = decrypt_packet(tmp_lora_packet);
			CDC_Transmit_FS(tmp_lora_packet.data, tmp_lora_packet.length);
		}
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, RESET);
	}
	else if (comms_check_radio_txne() == 1) // check if there is a packet to transmit
	{
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, SET);
		status = radio_tx_queue_remove(&tmp_lora_packet);
		if (status == 1)
			transmit_packet(&tmp_lora_packet);
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, RESET);
	} else if (comms_check_usb_rxne() == 1) // check if there is a packet to prepare
	{
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, SET);
		status = process_usb_packet(&tmp_lora_packet);
		if (status == 1)
		{
			tmp_lora_packet = encrypt_packet(tmp_lora_packet); // Encrypt
			radio_tx_queue_add(tmp_lora_packet);
		}
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, RESET);
	}
	// check_receive_buffer();
	/*
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, SET);
	HAL_Delay(1000);
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, RESET);
	HAL_Delay(1000);
	*/

}
