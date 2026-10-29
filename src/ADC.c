#include <MKL25Z4.h>
#include "ADC.h"
#include "project.h"

void Init_ADC(void) {
    /* Enable the ADC, its external input pin, and the channel 27 bandgap. */
    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    PMC->REGSC |= PMC_REGSC_BGBE_MASK;

    /* MUX=0 makes PTB2 an analog input (ADC0_SE12). */
    PORTB->PCR[2] &= ~PORT_PCR_MUX_MASK;

    ADC0->SC1[0] = ADC_SC1_ADCH(31); /* Channel 31 stops conversions. */
    /* 24 MHz bus / 4 = 6 MHz ADC clock; MODE=3 selects 16 bits. */
    ADC0->CFG1 = ADC_CFG1_ADIV(2) | ADC_CFG1_MODE(3);
    ADC0->CFG2 = 0; /* A mux for channels 12, 26, and 27. */
    ADC0->SC2 = 0;  /* Software trigger; VREFH/VREFL = 3.3 V/ground. */
    ADC0->SC3 = 0;  /* One conversion at a time; no averaging. */

    /* Allow conversion-complete interrupts through the NVIC. */
    NVIC_SetPriority(ADC0_IRQn, 1);
    NVIC_ClearPendingIRQ(ADC0_IRQn);
    NVIC_EnableIRQ(ADC0_IRQn);
}

void Start_ADC_Conversion(uint8_t channel) {
    /* Ignore any channel outside the three required by the project. */
    if ((channel == 12U) || (channel == 26U) || (channel == 27U)) {
        /* Writing SC1 starts a conversion and enables its interrupt. */
        ADC0->SC1[0] = ADC_SC1_AIEN_MASK | ADC_SC1_ADCH(channel);
    }
}

void ADC0_IRQHandler(void) {
    /* Read the result to clear COCO, then record it for the debugger. */
    debug_adc_value = (uint16_t)ADC0->R[0];
    ++debug_adc_count;
    debug_irq_flags |= DEBUG_IRQ_ADC;
}
