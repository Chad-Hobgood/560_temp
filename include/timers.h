#ifndef TIMERS_H
#define TIMERS_H

#include <stdint.h>

void Init_TPM0(uint16_t period_us);
void Set_Sample_Period(uint16_t period_us);
void Start_Sample_Timer(void);
void Stop_Sample_Timer(void);

#endif