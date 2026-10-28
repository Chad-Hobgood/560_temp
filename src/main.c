/*----------------------------------------------------------------------------
 *----------------------------------------------------------------------------*/
#include <MKL25Z4.H>
#include <stdio.h>
#include "gpio_defs.h"
#include "LEDs.h"
#include "UART.h"
#include "DAC.h"
#include "timers.h"
#include "delay.h"

#define SAMPLES_TO_AVERAGE (25)

volatile int conv_complete=0;
unsigned int res_min=0xffff, res_max = 0;
float P3V3_voltage = 0.0;
volatile int do_convert = 0, do_print = 0, do_calculate = 0;
volatile uint32_t ADC_temp = 0;

void Misc(void) {
	SIM->SCGC5 |= SIM_SCGC5_PORTA_MASK;
	PORTA->PCR[12] &= ~PORT_PCR_MUX_MASK;
	PORTA->PCR[12] |= PORT_PCR_MUX(1);
	GPIOA->PDDR |= 1 << 12;
	GPIOA->PCOR = 1 << 12;
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

void Convert(void) {
	conv_complete = 0;
	ADC0->SC1[0] = ADC_SC1_AIEN(1) | ADC_SC1_ADCH(27); // start conversion on channel 27 (Bandgap reference)
}

void ADC0_IRQHandler(void) {
	ADC_temp = ADC0->R[0]; 
	conv_complete = 1;
	NVIC_ClearPendingIRQ(ADC0_IRQn);
	do_calculate = 1;
}

float Calculate(void) {
	float vrail;
	uint32_t result;
	
	result = ADC_temp; // ADC0->R[0];
	// Update minimum and maximum results 
	if (result < res_min)
		res_min = result;
	if (result > res_max)
		res_max = result;
	vrail = (1.0/result)*4096;
	do_print++;
	return vrail;
}

void Print(float voltage) {
	static float sum=0.0;
	static int count=0;
	
	// Make LED red while printing out through serial port
	Control_RGB_LEDs(1, 2, 2);
	// Print out [minimum|maximum] range 
	// printf("[%5d <-%4d -> %5d] %4d. ", res_min, res_max - res_min, res_max);
	printf("P3V3 Rail is at %6.4d V", voltage); 
	
	sum += voltage;
	if (++count == SAMPLES_TO_AVERAGE){
		printf(" --- Average %f over %d samples\n\r", sum/count, SAMPLES_TO_AVERAGE);
		sum = 0.0;
		count = 0;
	} else
		printf("\n\r"); 

	Control_RGB_LEDs(0, 2, 2);
}	

/*----------------------------------------------------------------------------
  MAIN function
 *----------------------------------------------------------------------------*/
int main (void) {
	Init_RGB_LEDs();
	Control_RGB_LEDs(1, 1, 0); // Yellow
	Misc();
	//  Calibrate_ADC(); 
	Init_ADC(); 
	Init_UART2(57600); 
	Control_RGB_LEDs(1, 0, 0); // Red
	Init_DAC();
	Control_RGB_LEDs(0, 1, 1); // Cyan
	Init_LPTMR(1000);
	Control_RGB_LEDs(1, 0, 1); // Magenta
	printf("\n\rPlease debug me!\n\r");
	Control_RGB_LEDs(0, 1, 0); // Green
	Delay(3000);
	
	Start_LPTMR();
	
	while (1) {
		if (do_convert> 0) {
			do_convert--;
			Convert();
		}
		if (do_calculate > 0) {
			do_calculate--;
			P3V3_voltage = Calculate();
		}
		if (do_print > 0) {
			Print(P3V3_voltage); 
			do_print = 0;
		}
	}
}
