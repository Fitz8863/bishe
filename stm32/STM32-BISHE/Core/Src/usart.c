/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.c
  * @brief   This file provides code for the configuration
  *          of the USART instances.
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
#include "usart.h"

/* USER CODE BEGIN 0 */
#include <stdio.h>
#include "steering_engine.h"

#define USART1_PACKET_SIZE         6U
#define USART1_PACKET_HEADER       0xAAU
#define USART1_PACKET_TYPE         0x01U
#define USART1_PACKET_TAIL         0x55U
#define SERVO1_PWM_MIN             1000U
#define SERVO1_PWM_MID             1500U
#define SERVO1_PWM_MAX             1900U
#define SERVO2_PWM_MIN             1000U
#define SERVO2_PWM_MID             1500U
#define SERVO2_PWM_MAX             2000U

extern volatile uint16_t servo_pwm[2];
extern volatile int8_t serial_col;
extern volatile int8_t serial_row;

static uint8_t usart1_rx_byte = 0;
static uint8_t usart2_rx_byte = 0;
static uint8_t usart1_packet[USART1_PACKET_SIZE] = {0};
static uint8_t usart1_packet_index = 0;

static uint16_t ClampPwm(uint16_t value, uint16_t min, uint16_t max)
{
  if (value < min)
  {
    return min;
  }

  if (value > max)
  {
    return max;
  }

  return value;
}

static uint16_t MapColToServo1Pwm(int8_t col)
{
  int32_t pwm = SERVO1_PWM_MID;

  if (col > 8)
  {
    col = 8;
  }
  else if (col < -8)
  {
    col = -8;
  }

  pwm += (col * 50);

  return ClampPwm((uint16_t)pwm, SERVO1_PWM_MIN, SERVO1_PWM_MAX);
}

static uint16_t MapRowToServo2Pwm(int8_t row)
{
  int32_t pwm = SERVO2_PWM_MID;

  if (row > 10)
  {
    row = 10;
  }
  else if (row < -10)
  {
    row = -10;
  }

  pwm -= (row * 50);

  return ClampPwm((uint16_t)pwm, SERVO2_PWM_MIN, SERVO2_PWM_MAX);
}

static void ProcessUsart1Packet(void)
{
  uint8_t checksum = 0;
  int8_t col = 0;
  int8_t row = 0;

  if ((usart1_packet[0] != USART1_PACKET_HEADER) ||
      (usart1_packet[1] != USART1_PACKET_TYPE) ||
      (usart1_packet[5] != USART1_PACKET_TAIL))
  {
    return;
  }

  checksum = (uint8_t)(usart1_packet[0] + usart1_packet[1] + usart1_packet[2] + usart1_packet[3]);
  if (checksum != usart1_packet[4])
  {
    return;
  }

  col = (int8_t)usart1_packet[2];
  row = (int8_t)usart1_packet[3];

  serial_col = col;
  serial_row = row;

  servo_pwm[0] = MapColToServo1Pwm(col);
  servo_pwm[1] = MapRowToServo2Pwm(row);

  Set_Pwm(1, servo_pwm[0]);
  Set_Pwm(2, servo_pwm[1]);
}

static void USART1_ParsePacketByte(uint8_t data)
{
  switch (usart1_packet_index)
  {
    case 0:
      if (data == USART1_PACKET_HEADER)
      {
        usart1_packet[usart1_packet_index++] = data;
      }
      break;

    case 1:
      if (data == USART1_PACKET_TYPE)
      {
        usart1_packet[usart1_packet_index++] = data;
      }
      else if (data == USART1_PACKET_HEADER)
      {
        usart1_packet[0] = data;
        usart1_packet_index = 1;
      }
      else
      {
        usart1_packet_index = 0;
      }
      break;

    default:
      usart1_packet[usart1_packet_index++] = data;
      if (usart1_packet_index >= USART1_PACKET_SIZE)
      {
        ProcessUsart1Packet();
        usart1_packet_index = 0;
      }
      break;
  }
}

int fputc(int ch, FILE *f)
{
  uint8_t temp[1] = {ch};
  HAL_UART_Transmit(&huart1, temp, 1, 2);
  return ch;
}

/* USER CODE END 0 */

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USART1 init function */

void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  USART1_StartReceiveIT();

  /* USER CODE END USART1_Init 2 */

}
/* USART2 init function */

void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  USART2_StartReceiveIT();

  /* USER CODE END USART2_Init 2 */

}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle)
{

  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspInit 0 */

  /* USER CODE END USART1_MspInit 0 */
    /* USART1 clock enable */
    __HAL_RCC_USART1_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_9;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART1 interrupt Init */
    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspInit 1 */

  /* USER CODE END USART1_MspInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspInit 0 */

  /* USER CODE END USART2_MspInit 0 */
    /* USART2 clock enable */
    __HAL_RCC_USART2_CLK_ENABLE();

    __HAL_RCC_GPIOA_CLK_ENABLE();
    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    /* USART2 interrupt Init */
    HAL_NVIC_SetPriority(USART2_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART2_IRQn);
  /* USER CODE BEGIN USART2_MspInit 1 */

  /* USER CODE END USART2_MspInit 1 */
  }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle)
{

  if(uartHandle->Instance==USART1)
  {
  /* USER CODE BEGIN USART1_MspDeInit 0 */

  /* USER CODE END USART1_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART1_CLK_DISABLE();

    /**USART1 GPIO Configuration
    PA9     ------> USART1_TX
    PA10     ------> USART1_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_9|GPIO_PIN_10);

    /* USART1 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART1_IRQn);
  /* USER CODE BEGIN USART1_MspDeInit 1 */

  /* USER CODE END USART1_MspDeInit 1 */
  }
  else if(uartHandle->Instance==USART2)
  {
  /* USER CODE BEGIN USART2_MspDeInit 0 */

  /* USER CODE END USART2_MspDeInit 0 */
    /* Peripheral clock disable */
    __HAL_RCC_USART2_CLK_DISABLE();

    /**USART2 GPIO Configuration
    PA2     ------> USART2_TX
    PA3     ------> USART2_RX
    */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_2|GPIO_PIN_3);

    /* USART2 interrupt Deinit */
    HAL_NVIC_DisableIRQ(USART2_IRQn);
  /* USER CODE BEGIN USART2_MspDeInit 1 */

  /* USER CODE END USART2_MspDeInit 1 */
  }
}

/* USER CODE BEGIN 1 */

void USART1_StartReceiveIT(void)
{
  HAL_UART_Receive_IT(&huart1, &usart1_rx_byte, 1);
}

void USART2_StartReceiveIT(void)
{
  HAL_UART_Receive_IT(&huart2, &usart2_rx_byte, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
  {
    USART1_ParsePacketByte(usart1_rx_byte);

    USART1_StartReceiveIT();
  }
  else if (huart->Instance == USART2)
  {
    HAL_UART_Transmit(&huart2, &usart2_rx_byte, 1, HAL_MAX_DELAY);

    if (usart2_rx_byte == '\r')
    {
      uint8_t lf = '\n';
      HAL_UART_Transmit(&huart2, &lf, 1, HAL_MAX_DELAY);
    }

    USART2_StartReceiveIT();
  }
}

/* USER CODE END 1 */
