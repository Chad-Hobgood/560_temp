
#include <MKL25Z4.H>
#include <stdio.h>
#include "gpio_defs.h"
#include "LEDs.h"
#include "UART.h"
#include "DAC.h"
#include "timers.h"
#include "delay.h"

extern volatile int conv_complete=0;
extern unsigned int res_min=0xffff, res_max = 0;
extern float P3V3_voltage = 0.0;
extern volatile int do_convert = 0, do_print = 0, do_calculate = 0;
extern volatile uint32_t ADC_temp = 0;

void ADC0_IRQHandler(void) {
	ADC_temp = ADC0->R[0]; 
	conv_complete = 1;
	NVIC_ClearPendingIRQ(ADC0_IRQn);
	do_calculate = 1;
}

void Init_ADC(void) {
	ADC0->CFG1 = ADC_CFG1_ADLPC(0) | ADC_CFG1_ADIV(3) | ADC_CFG1_ADICLK(0) |  
		ADC_CFG1_ADLSMP(1) | ADC_CFG1_MODE(1);
	ADC0->CFG2 = ADC_CFG2_ADLSTS(0) | ADC_CFG2_ADHSC(0) | ADC_CFG2_ADACKEN(0) | 
		ADC_CFG2_MUXSEL(0);
	ADC0->SC2 = ADC_SC2_REFSEL(0); // VREFHL selection, software trigger
	ADC0->SC3 = ADC_SC3_AVGE(0) | ADC_SC3_AVGS(0); // If enabled (AVGE=1), average multiple samples (0:4, 1:8, 2:16, 3:32) 
	
	// Configure NVIC 
	NVIC_SetPriority(ADC0_IRQn, 3); // 0, 1, 2 or 3
	NVIC_ClearPendingIRQ(ADC0_IRQn); 
	NVIC_EnableIRQ(ADC0_IRQn);	
}

void Calibrate_ADC(void) {
	SIM->SCGC6 |= SIM_SCGC6_ADC0(1); 
	// From SRM section 28.4.6

	// Set fADCK <= 4 MHz. Not low power, divide clock by 8, select bus clock, long sample time, 16-bit conversion
	// With CLK_SETUP == 1, Bus clock = 24 MHz, so fADCK = 24 MHz/8 = 3 MHz
	ADC0->CFG1 = ADC_CFG1_ADLPC(0) | ADC_CFG1_ADIV(3) | ADC_CFG1_ADICLK(0) |  
		ADC_CFG1_ADLSMP(1) | ADC_CFG1_MODE(3);
	// longest sample time, normal conversion speed, async clock output disabled, ADxxa channels selected
	ADC0->CFG2 = ADC_CFG2_ADLSTS(0) | ADC_CFG2_ADHSC(0) | ADC_CFG2_ADACKEN(0) | 
		ADC_CFG2_MUXSEL(0);
	// Set maximum hardware averaging: AVGE = 1, AVGS = 11
	ADC0_SC3 = ADC_SC3_AVGS(1) | ADC_SC3_AVGS(3);
	// Use Vdda reference, software ADC trigger
	ADC0->SC2 = ADC_SC2_REFSEL(1) | ADC_SC2_ADTRG(0);
	
	// Start calibration
	ADC0_SC3 |= ADC_SC3_CAL(1);
	// Await calibration completion
	while (!(ADC0->SC1[0] & ADC_SC1_COCO_MASK) )
		;
	
	if (ADC0_SC3 & ADC_SC3_CALF_MASK) {
		// calibration failed
		Control_RGB_LEDs(1, 0, 0);
		while (1);
	}
		
	// Read calibration results, assign gains
	uint16_t pcal;
	pcal = ADC0_CLP0 + ADC0_CLP1 + ADC0_CLP2 + ADC0_CLP3 + ADC0_CLP4 + ADC0_CLPS;
	pcal /= 2;
	pcal |= 0x8000;
	ADC0_PG = pcal;
	
	uint16_t mcal;
	mcal = ADC0_CLM0 + ADC0_CLM1 + ADC0_CLM2 + ADC0_CLM3 + ADC0_CLM4 + ADC0_CLMS;
	mcal /= 2;
	mcal |= 0x8000;
	ADC0_MG = mcal;
}