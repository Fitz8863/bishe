#ifndef __KEY_H_
#define __KEY_H_

#define KEY_RELEASE_LEVEL           (1)                                
#define KEY_MAX_SHOCK_PERIOD        (10       )                                
#define KEY_LONG_PRESS_PERIOD       (1000     )                                


typedef enum
{
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_NUMBER,
}key_index_enum;                                                               

typedef enum
{
    KEY_RELEASE,                                                                
    KEY_SHORT_PRESS,                                                            
    KEY_LONG_PRESS,                                                            
}key_state_enum;

void            key_scanner             (void);                               
key_state_enum  key_get_state           (key_index_enum key_n);                
void            key_clear_state         (key_index_enum key_n);                 
void            key_clear_all_state     (void);                                
void            key_init                (int period);                      


#endif