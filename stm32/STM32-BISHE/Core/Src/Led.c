#include "main.h"
#include "Led.h"
uint8_t start_flag = 0;
void Led_Light(uint16_t SET)
{
	HAL_GPIO_WritePin(LED_PORT, LED_PIN, SET);
}

void Led_Toggle(void)
{
	HAL_GPIO_TogglePin(LED_PORT, LED_PIN);
}