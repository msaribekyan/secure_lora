#include "stm32f0xx_hal.h"

uint32_t get_time_ms(void)
{
	return HAL_GetTick();
}
