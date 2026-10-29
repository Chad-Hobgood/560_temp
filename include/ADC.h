#ifndef ADC_H
#define ADC_H

#include <stdint.h>

void Init_ADC(void);
void ADC_Start_Single(uint8_t channel);
void ADC_Start_Timer_Trigger(uint8_t channel);
void ADC_Start_Trigger_Monitor(void);
void ADC_Trigger_Compare_Above(void);
void ADC_Stop(void);

#endif
