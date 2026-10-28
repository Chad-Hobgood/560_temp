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
#include "ADC.h"

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



void Convert(void) {
	conv_complete = 0;
	ADC0->SC1[0] = ADC_SC1_AIEN(1) | ADC_SC1_ADCH(27); // start conversion on channel 27 (Bandgap reference)
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
