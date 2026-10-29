#ifndef ADC_H
#define ADC_H

#include <stdint.h>

void Init_ADC(void);
void Start_ADC_Conversion(uint8_t channel);

#endif