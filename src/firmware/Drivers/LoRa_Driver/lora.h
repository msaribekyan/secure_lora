#ifndef LORA_H
#define LORA_H

#define LORA_TIMEOUT 1000

#include "stm32f0xx_hal.h"

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
