#ifndef TIMERS_H
#define TIMERS_H
#include "MKL25Z4.h"

#define FREQ_LPO (1000)

#define WAIT_PERIOD 80

extern volatile int not_ready;
extern volatile int enable;

void Init_LPTMR(uint32_t freq);
void Start_LPTMR(void);
void Stop_LPTMR(void);

void Init_PIT(unsigned period);
void Start_PIT(void);
void Stop_PIT(void);

void Init_PWM(void);
void Set_PWM_Values(uint16_t perc1, uint16_t perc2);


#endif
// *******************************ARM University Program Copyright © ARM Ltd 2013*************************************   
