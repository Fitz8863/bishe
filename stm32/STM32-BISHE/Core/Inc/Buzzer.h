#ifndef __BUZZER_H_
#define __BUZZER_H_

#define BUZZER_PORT GPIOA
#define BUZZER_PIN  GPIO_PIN_15

void Buzzer_Init(void);
void Buzzer_Set(uint16_t SET);
void Buzzer_Toggle(void);
#endif