#include "main.h"
#include "Buzzer.h"
void Buzzer_Init(void)
{
	HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, GPIO_PIN_RESET);
}

void Buzzer_Set(uint16_t SET)
{
	HAL_GPIO_WritePin(BUZZER_PORT, BUZZER_PIN, SET);
}

void Buzzer_Toggle(void)
{
	HAL_GPIO_TogglePin(BUZZER_PORT, BUZZER_PIN);
}