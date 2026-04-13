/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "oled.h"
#include "steering_engine.h"
#include "usart.h"
#include <stdio.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define KEY_SCAN_PERIOD_MS        10
#define KEY_TEST_FLASH_TIME_MS    120
#define SERVO_PWM_MIN             500
#define SERVO_PWM_MID             1500
#define SERVO_PWM_MAX             2500
#define SERVO_PWM_STEP            50

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

osThreadId_t ledBlinkTaskHandle;
const osThreadAttr_t ledBlinkTask_attributes = {
  .name = "ledBlinkTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

osThreadId_t oledTestTaskHandle;
const osThreadAttr_t oledTestTask_attributes = {
  .name = "oledTestTask",
  .stack_size = 256 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

volatile uint16_t servo_pwm[2] = {SERVO_PWM_MID, SERVO_PWM_MID};
volatile uint8_t selected_servo = 0;
volatile int8_t serial_col = 0;
volatile int8_t serial_row = 0;

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

void StartLedBlinkTask(void *argument);
void StartOledTestTask(void *argument);
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */
	
  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  ledBlinkTaskHandle = osThreadNew(StartLedBlinkTask, NULL, &ledBlinkTask_attributes);
  oledTestTaskHandle = osThreadNew(StartOledTestTask, NULL, &oledTestTask_attributes);
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
	key_init(KEY_SCAN_PERIOD_MS);
  Buzzer_Init();
	Steering_Engine_Init();
	Set_Pwm(1, servo_pwm[0]);
	Set_Pwm(2, servo_pwm[1]);
  /* Infinite loop */
  for (;;)
  {
    key_scanner();

    if(KEY_SHORT_PRESS == key_get_state(KEY_3))
    {
      selected_servo = 1 - selected_servo;
      Buzzer_Set(GPIO_PIN_SET);
      vTaskDelay(pdMS_TO_TICKS(50));
      Buzzer_Set(GPIO_PIN_RESET);
      key_clear_state(KEY_3);
    }

    if(KEY_SHORT_PRESS == key_get_state(KEY_1))
    {
      if((servo_pwm[selected_servo] + SERVO_PWM_STEP) <= SERVO_PWM_MAX)
      {
        servo_pwm[selected_servo] += SERVO_PWM_STEP;
      }
      else
      {
        servo_pwm[selected_servo] = SERVO_PWM_MAX;
      }

      Set_Pwm(selected_servo + 1, servo_pwm[selected_servo]);
      key_clear_state(KEY_1);
    }

    if(KEY_SHORT_PRESS == key_get_state(KEY_2))
    {
      if(servo_pwm[selected_servo] >= (SERVO_PWM_MIN + SERVO_PWM_STEP))
      {
        servo_pwm[selected_servo] -= SERVO_PWM_STEP;
      }
      else
      {
        servo_pwm[selected_servo] = SERVO_PWM_MIN;
      }

      Set_Pwm(selected_servo + 1, servo_pwm[selected_servo]);
      key_clear_state(KEY_2);
    }

    vTaskDelay(pdMS_TO_TICKS(KEY_SCAN_PERIOD_MS));

  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

void StartLedBlinkTask(void *argument)
{
  for(;;)
  {
    Led_Toggle();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void StartOledTestTask(void *argument)
{
  uint8_t line_buffer[24];
  uint16_t servo1_pwm = 0;
  uint16_t servo2_pwm = 0;
  int8_t current_col = 0;
  int8_t current_row = 0;

  OLED_Init();
  OLED_SetFlip(0);

  for(;;)
  {
    servo1_pwm = servo_pwm[0];
    servo2_pwm = servo_pwm[1];
    current_col = serial_col;
    current_row = serial_row;

    OLED_FullyClear();
    OLED_ShowStr(0, 0, (uint8_t *)"SERVO STATUS", 1);

    snprintf((char *)line_buffer, sizeof(line_buffer), "S1 PWM:%u", servo1_pwm);
    OLED_ShowStr(0, 16, line_buffer, 1);

    snprintf((char *)line_buffer, sizeof(line_buffer), "S2 PWM:%u", servo2_pwm);
    OLED_ShowStr(0, 32, line_buffer, 1);

    snprintf((char *)line_buffer, sizeof(line_buffer), "C:%+d R:%+d", current_col, current_row);
    OLED_ShowStr(0, 48, line_buffer, 1);

    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

/* USER CODE END Application */

