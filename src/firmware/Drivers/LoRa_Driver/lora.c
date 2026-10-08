#include "lora.h"
#include "lora_reg.h"

void lora_reset(lora_t *lora)
{
	HAL_Delay(20);
	HAL_GPIO_WritePin(lora->reset_port, lora->reset_pin, GPIO_PIN_RESET);
	HAL_Delay(10);
	HAL_GPIO_WritePin(lora->reset_port, lora->reset_pin, GPIO_PIN_SET);
	HAL_Delay(20);
}

void lora_spi_write_reg(lora_t *lora, uint8_t reg, uint8_t* data, uint16_t len)
{
	HAL_GPIO_WritePin(lora->cs_port, lora->cs_pin, GPIO_PIN_RESET);
	reg |= 0x80;
	HAL_SPI_Transmit(lora->hspi, &reg, 1, 1000);
	while (HAL_SPI_GetState(lora->hspi) != HAL_SPI_STATE_READY);
	HAL_SPI_Transmit(lora->hspi, data, len, 1000);
	while (HAL_SPI_GetState(lora->hspi) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(lora->cs_port, lora->cs_pin, GPIO_PIN_SET);
}

void lora_spi_read_reg(lora_t *lora, uint8_t reg, uint8_t *data, uint16_t len)
{
	HAL_GPIO_WritePin(lora->cs_port, lora->cs_pin, GPIO_PIN_RESET);
	HAL_SPI_Transmit(lora->hspi, &reg, 1, 1000);
	while (HAL_SPI_GetState(lora->hspi) != HAL_SPI_STATE_READY);
	HAL_SPI_Receive(lora->hspi, data, len, 1000);
	while (HAL_SPI_GetState(lora->hspi) != HAL_SPI_STATE_READY);
	HAL_GPIO_WritePin(lora->cs_port, lora->cs_pin, GPIO_PIN_SET);
}

void lora_write_reg(lora_t *lora, uint8_t reg, uint8_t val)
{
	lora_spi_write_reg(lora, reg, &val, 1);
}

void lora_write_reg_burst(lora_t *lora, uint8_t reg, uint8_t *val, uint16_t len)
{
	lora_spi_write_reg(lora, reg, val, len);
}

uint8_t lora_read_reg(lora_t *lora, uint8_t reg)
{
	uint8_t data = 0;

	lora_spi_read_reg(lora, reg, &data, 1);
	return data;
}

void lora_init(lora_t *lora)
{
	lora_reset(lora);

	lora_write_reg(lora, REG_OP_MODE, 0x80); // LoRa + Sleep

	lora_write_reg(lora, REG_FRF_MSB, 0x6C);
	lora_write_reg(lora, REG_FRF_MID, 0x40);
	lora_write_reg(lora, REG_FRF_LSB, 0x00);

	lora_write_reg(lora, REG_PA_CONFIG, 0x8F);

	lora_write_reg(lora, REG_MODEM_CONFIG1, 0x72);
	lora_write_reg(lora, REG_MODEM_CONFIG2, 0x74);

	lora_write_reg(lora, REG_PREAMBLE_MSB, 0x00);
	lora_write_reg(lora, REG_PREAMBLE_LSB, 0x08);
}

void lora_setup_rx(lora_t *lora)
{
	lora_write_reg(lora, REG_OP_MODE, 0x80); // LoRa + Sleep

	lora_write_reg(lora, REG_FIFO_RX_BASE_ADDR, 0x00);
	lora_write_reg(lora, REG_FIFO_ADDR_PTR, 0x00);

	lora_write_reg(lora, REG_IRQ_FLAGS, 0xFF);

	lora_write_reg(lora, REG_OP_MODE, 0x85); // Start Rx Continous
}

void lora_setup_tx(lora_t *lora)
{
	lora_write_reg(lora, REG_OP_MODE, 0x80); // LoRa + Sleep

	lora_write_reg(lora, REG_FIFO_TX_BASE_ADDR, 0x00);
	lora_write_reg(lora, REG_FIFO_ADDR_PTR, 0x00);
}

void lora_transmit(lora_t *lora, uint8_t *data, uint8_t len)
{
	lora_write_reg_burst(lora, REG_FIFO, data, len);

	lora_write_reg(lora, REG_PAYLOAD_LENGTH, len);

	lora_write_reg(lora, REG_OP_MODE, 0x83); // Start Tx
	
	uint32_t start_tick = HAL_GetTick();
	while ((lora_read_reg(lora, REG_IRQ_FLAGS) & 0x08) == 0 && 
			start_tick + LORA_TIMEOUT > HAL_GetTick());
}

uint8_t lora_receive(lora_t *lora, uint8_t *data)
{
	uint8_t fifo_addr = 0;
	uint8_t i = 0;
	uint8_t len = 0;

	len = lora_read_reg(lora, REG_RX_NB_BYTES);
	fifo_addr = lora_read_reg(lora, REG_FIFO_RX_CURRENT_ADDR);
	lora_write_reg(lora, REG_FIFO_ADDR_PTR, fifo_addr);
	i = 0;
	while (i < len)
	{
		data[i] = lora_read_reg(lora, REG_FIFO);
		i++;
	}
	lora_write_reg(lora, REG_IRQ_FLAGS, 0x40);
	return len;
}

uint8_t lora_check_rx(lora_t *lora)
{
	if ((lora_read_reg(lora, REG_IRQ_FLAGS) & 0x40) != 0)
		return 1;
	return 0;
}

uint8_t lora_version(lora_t *lora)
{
	uint8_t val = 0;

	val = lora_read_reg(lora, REG_VERSION);
	return val;
}
