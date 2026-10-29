#ifndef SAMPLING_H
#define SAMPLING_H

#include <stdint.h>

void Sampling_Arm(void);
void Sampling_Force(void);
uint8_t Sampling_Take_Complete(void);
void Sampling_On_ADC_Complete(uint16_t value);

#endif
