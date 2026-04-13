#ifndef __LED_H_
#define __LED_H_

#define LED_PORT GPIOB
#define LED_PIN  GPIO_PIN_3
void Led_Light(uint16_t SET);
void Led_Toggle(void);
extern uint8_t start_flag;
#endif