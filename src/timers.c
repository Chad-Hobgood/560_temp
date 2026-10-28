#include "timers.h"
#include "MKL25Z4.h"
#include "LEDs.h"
#include "GPIO_defs.h"
#include "DAC.h"

extern float P3V3_voltage;
extern int do_convert, do_calculate, do_print;

void Init_LPTMR(uint32_t freq) {
	SIM->SCGC5 |=  SIM_SCGC5_LPTMR_MASK;

	// Configure LPTMR
	// select 1 kHz LPO clock with prescale factor 0, dividing clock by 2
	// resulting in 500 Hz clock
	LPTMR0->PSR = LPTMR_PSR_PCS(1) | LPTMR_PSR_PRESCALE(0) | LPTMR_PSR_PBYP(1); 
	LPTMR0->CSR = LPTMR_CSR_TIE_MASK;
	if (freq < 1000)
		LPTMR0->CMR = (FREQ_LPO/(freq))-1; // Period - 1
	else
		LPTMR0->CMR = 0; // Safety - clipping
	
	// Configure NVIC 
	NVIC_SetPriority(LPTimer_IRQn, 2); 
	NVIC_ClearPendingIRQ(LPTimer_IRQn); 
	NVIC_EnableIRQ(LPTimer_IRQn);	
}

void Start_LPTMR(void) {
	LPTMR0->CSR |= LPTMR_CSR_TEN_MASK;
}

void Stop_LPTMR(void) {
	LPTMR0->CSR &= ~LPTMR_CSR_TEN_MASK;
}

void LPTimer_IRQHandler(void) {
	static int i=0, c=1, DAC_value=DAC_RESOLUTION/2;
	static int timeout = WAIT_PERIOD;

	NVIC_ClearPendingIRQ(LPTimer_IRQn);
	LPTMR0->CSR |= LPTMR_CSR_TCF_MASK;

	timeout--;
	if (timeout == 0) {
		timeout = WAIT_PERIOD;
		if (do_print == 0)
			do_convert++;
	}
	Control_RGB_LEDs(2, 2, 1-do_convert);

	if ((i++&0x03f) == 0)
		Control_RGB_LEDs(2, (c++)&1, 2);
	else
		Control_RGB_LEDs(2, 0, 0);

	if (do_convert)
		DAC_value = 500;
	else if (do_calculate)
		DAC_value = 2000;
	else
		DAC_value = DAC_RESOLUTION*timeout/(2*WAIT_PERIOD) + 1000*do_print;
	Set_DAC(DAC_value);
}
