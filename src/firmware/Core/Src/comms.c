#include "main.h"
#include "aes.h"
#include "comms.h"
#include "crc16-ccitt-algorithm.h"

static lora_t lora = {0};

static lora_packet_t radio_tx_queue[TX_QUEUE_SIZE];
static uint8_t radio_tx_queue_head = 0;
static uint8_t radio_tx_queue_tail = 0;
static uint8_t radio_tx_queue_count = 0;

static uint8_t usb_rx_packet[64] = {0};
static uint8_t usb_rx_packet_len = 0;
static uint8_t usb_rx_available = 0;

const uint8_t aes_key[16] __attribute__((section(".secret_storage"))) = {
    0xE0, 0x3E, 0xA3, 0xF6,
    0xDB, 0xC1, 0x61, 0xC0,
    0x74, 0x01, 0x04, 0x5C,
    0x70, 0x62, 0x9A, 0xE6
};

uint8_t radio_tx_queue_add(lora_packet_t packet)
{
	if (radio_tx_queue_count == TX_QUEUE_SIZE)
		return 0;
	radio_tx_queue[radio_tx_queue_tail] = packet;
	radio_tx_queue_tail = (radio_tx_queue_tail + 1) % TX_QUEUE_SIZE;
	radio_tx_queue_count++;
	return 1;
}

uint8_t radio_tx_queue_remove(lora_packet_t *packet)
{
	if (radio_tx_queue_count == 0)
		return 0;
	*packet = radio_tx_queue[radio_tx_queue_head];
	radio_tx_queue_head = (radio_tx_queue_head + 1) % TX_QUEUE_SIZE;
	radio_tx_queue_count--;
	return 1;
}

uint8_t receive_packet(lora_packet_t *packet)
{
	packet->length = lora_receive(&lora, packet->data);	
	return 1;
}

void transmit_packet(lora_packet_t *packet)
{
	lora_setup_tx(&lora);
	lora_transmit(&lora, packet->data, packet->length);
	lora_setup_rx(&lora);
}

uint8_t check_radio_version(void)
{
	return lora_version(&lora);
}


lora_packet_t encrypt_packet(lora_packet_t packet)
{
	lora_packet_t new_packet = {0};
	uint8_t new_len = (packet.length + 15) & 0xF0;
	uint8_t nb_blocks = new_len / 16;

	// Zero pad
	for (uint8_t i = packet.length;i < 64;i++)
	{
		packet.data[i] = 0;
	}

	for (uint8_t i = 0;i < nb_blocks;i++)
	{
		aes128_encrypt_block(aes_key, packet.data + (i * 16), new_packet.data + (i * 16));
	}

	new_packet.length = new_len;
	return new_packet;
}


/*
packet_t packet_create(uint8_t *data, uint8_t len)
{
	packet_t packet;
	uint16_t crc = crc16_ccitt_init();

	packet.sync = PACKET_SYNC_BYTE;
	packet.length = len;
	crc = crc16_ccitt_update(len, crc);
	for (uint8_t i = 0;i < len;i++)
	{
		packet.message[i] = data[i];
		crc = crc16_ccitt_update(data[i], crc);
	}
	crc = crc16_ccitt_finalize(crc);
	packet.crc = crc;
	return packet;
}
*/

void comms_init(
	SPI_HandleTypeDef *hspi,
	GPIO_TypeDef *cs_port,
	uint16_t cs_pin,
	GPIO_TypeDef *reset_port,
	uint16_t reset_pin
)
{
	lora.hspi = hspi;
	lora.cs_port = cs_port;
	lora.cs_pin = cs_pin;
	lora.reset_port = reset_port;
	lora.reset_pin = reset_pin;
	lora_init(&lora);
	lora_setup_rx(&lora);
}

uint8_t comms_check_usb_rxne(void)
{
	if (usb_rx_available)
		return 1;
	return 0;
}

uint8_t comms_check_radio_rxne(void)
{
	if (lora_check_rx(&lora) == 1)
		return 1;
	return 0;
}

uint8_t comms_check_radio_txne(void)
{
	if (radio_tx_queue_count > 0)
		return 1;
	return 0;
}

void usb_receive_callback(uint8_t *data, uint8_t len)
{
	if (usb_rx_available)
		return; // Drops the packet, change later.
	usb_rx_packet_len  = len;
	for (uint8_t i = 0;i < len;i++)
	{
		usb_rx_packet[i] = data[i];
	}
	usb_rx_available = 1;
}

uint8_t process_usb_packet(lora_packet_t *packet)
{
	if (usb_rx_available == 0)
		return 0;
	packet->length = usb_rx_packet_len;
	for (uint8_t i = 0;i < usb_rx_packet_len;i++)
	{
		packet->data[i] = usb_rx_packet[i];
	}
	usb_rx_available = 0;
	usb_rx_packet_len = 0;
	return 1;
}

/*
void check_receive_buffer()
{
	if (rxne == 1)
	{
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, SET);
		//CDC_Transmit_FS(rx_buf, rx_len);
		HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, RESET);
		rxne = 0;
	}
}

packet_error_t packet_decode(const uint8_t *data, uint8_t len, packet_t *packet)
{
	uint16_t crc = 0;
	uint16_t received_crc = 0;
	uint8_t i = 0;

	if (len < 4)
		return PACKET_TOO_SMALL;
	i = 0;
	while (data[i] != PACKET_SYNC_BYTE)
	{
		i++;
		if (i >= len)
		{
			return PACKET_NO_SYNC;
		}
	}
	packet->sync = data[i++];
	if (i >= len)
		return PACKET_OUT_OF_BOUND;
	packet->length = data[i++];
	if (i + packet->length + 2 > len)
		return PACKET_OUT_OF_BOUND;
	crc = crc16_ccitt_init();
	crc = crc16_ccitt_update(packet->length, crc);
	for (uint8_t j = 0;j < packet->length;j++)
	{
		packet->message[j] = data[i];
		crc = crc16_ccitt_update(data[i], crc);
		i++;
	}
	crc = crc16_ccitt_finalize(crc);
	received_crc = data[i] << 8 | data[i + 1];
	if (crc != received_crc)
		return PACKET_INVALID_CRC;
	return PACKET_OK;
}

void lora_receive_callback(uint8_t *data, uint8_t len)
{
	packet_decode(data, len, &rx_packet);
}
*/
