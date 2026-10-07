#ifndef LORA_H
#define LORA_H

#include "main.h"

#define LORA_TIMEOUT 1000

#define REG_FIFO                    0x00

#define REG_OP_MODE                 0x01

#define REG_FRF_MSB                 0x06
#define REG_FRF_MID                 0x07
#define REG_FRF_LSB                 0x08

#define REG_PA_CONFIG               0x09
#define REG_PA_RAMP                 0x0A
#define REG_OCP                     0x0B
#define REG_LNA                     0x0C

#define REG_FIFO_ADDR_PTR           0x0D
#define REG_FIFO_TX_BASE_ADDR       0x0E
#define REG_FIFO_RX_BASE_ADDR       0x0F

#define REG_FIFO_RX_CURRENT_ADDR	0x10
#define REG_IRQ_FLAGS_MASK          0x11
#define REG_IRQ_FLAGS               0x12
#define REG_RX_NB_BYTES				0x13
#define REG_MODEM_CONFIG1           0x1D
#define REG_MODEM_CONFIG2           0x1E
#define REG_PREAMBLE_MSB            0x20
#define REG_PREAMBLE_LSB            0x21
#define REG_PAYLOAD_LENGTH          0x22
#define REG_MODEM_CONFIG3           0x26

#define REG_SYNC_WORD               0x39

#define REG_VERSION                0x42

typedef struct lora_s
{
	SPI_HandleTypeDef *hspi;
	GPIO_TypeDef *cs_port;
	uint16_t cs_pin;
	GPIO_TypeDef *reset_port;
	uint16_t reset_pin;
} lora_t;

void lora_init(lora_t *lora);
void lora_setup_rx(lora_t *lora);
void lora_setup_tx(lora_t *lora);
void lora_transmit(lora_t *lora, uint8_t *data, uint8_t len);
uint8_t lora_receive(lora_t *lora, uint8_t *data);
uint8_t lora_check_rx(lora_t *lora);
uint8_t lora_version(lora_t *lora);

#endif // LORA_H
