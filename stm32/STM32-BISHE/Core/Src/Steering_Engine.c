#include "main.h"
#include "tim.h"

void Steering_Engine_Init(void)
{
	HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim3,TIM_CHANNEL_2);	
}

void Set_Pwm(int Steering_Engine_Index, int PWM)
{
	switch(Steering_Engine_Index)
	{
		case 1:__HAL_TIM_SET_COMPARE(&htim3,TIM_CHANNEL_1,PWM);break;
		case 2:__HAL_TIM_SET_COMPARE(&htim3,TIM_CHANNEL_2,PWM);break;
		default:break;
	}
}

