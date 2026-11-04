#include "led.h"
#include "gpio.h"
#include "main.h"

void led_init(void)
{
	return; // nothing yet
}

void led_on(void)
{
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, SET);
}

void led_off(void)
{
	HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, RESET);
}

