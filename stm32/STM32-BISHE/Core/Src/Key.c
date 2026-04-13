#include "main.h"
#include "Key.h"

static uint32_t               scanner_period = 0;                                
static uint32_t               key_press_time[KEY_NUMBER];                        
static key_state_enum         key_state[KEY_NUMBER];                              

static const uint16_t key_index[KEY_NUMBER] = {KEY1_Pin, KEY2_Pin, KEY3_Pin};                  
static GPIO_TypeDef * const key_port[KEY_NUMBER] = {KEY1_GPIO_Port, KEY2_GPIO_Port, KEY3_GPIO_Port};

void key_scanner (void)
{
    uint16_t i = 0;
    for(i = 0; KEY_NUMBER > i; i ++)
    {
        if(KEY_RELEASE_LEVEL != HAL_GPIO_ReadPin(key_port[i], key_index[i]))                  
        {
            key_press_time[i] ++;
            if(KEY_LONG_PRESS_PERIOD / scanner_period <= key_press_time[i])
            {
                key_state[i] = KEY_LONG_PRESS;
            }
        }
        else                                                                    
        {
            if((KEY_LONG_PRESS != key_state[i]) && (KEY_MAX_SHOCK_PERIOD / scanner_period <= key_press_time[i]))
            {
                key_state[i] = KEY_SHORT_PRESS;
            }
            else
            {
                key_state[i] = KEY_RELEASE;
            }
            key_press_time[i] = 0;
        }
    }
}

key_state_enum key_get_state (key_index_enum key_n)
{
    return key_state[key_n];
}

void key_clear_state (key_index_enum key_n)
{
    key_state[key_n] = KEY_RELEASE;
}


void key_clear_all_state (void)
{
    key_state[0] = KEY_RELEASE;
    key_state[1] = KEY_RELEASE;
    key_state[2] = KEY_RELEASE;
}

void key_init (int period)
{
    uint16_t loop_temp = 0; 
    for(loop_temp = 0; KEY_NUMBER > loop_temp; loop_temp ++)
    {
        key_state[loop_temp] = KEY_RELEASE;
    }
    scanner_period = period;
}
