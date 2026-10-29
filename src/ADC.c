#include <MKL25Z4.h>
#include "ADC.h"
#include "project.h"
#include "sampling.h"

void Init_ADC(void) {
    /* Enable the ADC, its external input pin, and the channel 27 bandgap. */
    SIM->SCGC6 |= SIM_SCGC6_ADC0_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTB_MASK;
    PMC->REGSC |= PMC_REGSC_BGBE_MASK;

    /* MUX=0 makes PTB2 an analog input (ADC0_SE12). */
    PORTB->PCR[2] &= ~PORT_PCR_MUX_MASK;

    ADC0->SC1[0] = ADC_SC1_ADCH(31); /* Channel 31 stops conversions. */
    /* 24 MHz bus / 2 = 12 MHz ADC clock; MODE=3 selects 16 bits.
       High-speed mode permits this clock rate. Keep the A input mux. */
    ADC0->CFG1 = ADC_CFG1_ADIV(1) | ADC_CFG1_MODE(3);
    ADC0->CFG2 = ADC_CFG2_ADHSC_MASK;
    ADC0->SC2 = 0;  /* Software trigger; VREFH/VREFL = 3.3 V/ground. */
    ADC0->SC3 = 0;  /* One conversion at a time; no averaging. */

    /* Allow conversion-complete interrupts through the NVIC. */
    /* TPM0 has priority 1 so its overflow count is recorded first. */
    NVIC_SetPriority(ADC0_IRQn, 2);
    NVIC_ClearPendingIRQ(ADC0_IRQn);
    NVIC_EnableIRQ(ADC0_IRQn);
}

void ADC_Start_Single(uint8_t channel) {
    /* Ignore any channel outside the three required by the project. */
    if ((channel == 12U) || (channel == 26U) || (channel == 27U)) {
        /* SC2/SC3 were set by Init_ADC or ADC_Stop. Keep this ISR path short. */
        ADC0->SC1[0] = ADC_SC1_AIEN_MASK | ADC_SC1_ADCH(channel);
    }
}

void ADC_Start_Timer_Trigger(uint8_t channel) {
    /* TPM0 overflow starts each later conversion in hardware. The selected
       pretrigger is A, which uses SC1[0] and result register R[0]. */
    SIM->SOPT7 = (SIM->SOPT7 &
                  ~(SIM_SOPT7_ADC0TRGSEL_MASK |
                    SIM_SOPT7_ADC0PRETRGSEL_MASK)) |
                 SIM_SOPT7_ADC0ALTTRGEN_MASK |
                 SIM_SOPT7_ADC0TRGSEL(8);
    ADC0->SC2 = ADC_SC2_ADTRG_MASK;
    ADC0->SC1[0] = ADC_SC1_AIEN_MASK | ADC_SC1_ADCH(channel);
}

void ADC_Start_Trigger_Monitor(void) {
    /* In continuous mode, compare only interrupts when PTB2 is below
       0.5 V. This arms the next upward crossing. */
    ADC0->SC1[0] = ADC_SC1_ADCH(31);
    ADC0->CV1 = (uint16_t)PROJECT_TRIGGER_ADC_CODE;
    ADC0->SC2 = ADC_SC2_ACFE_MASK;
    ADC0->SC3 = ADC_SC3_ADCO_MASK;
    ADC0->SC1[0] = ADC_SC1_AIEN_MASK | ADC_SC1_ADCH(12);
}

void ADC_Trigger_Compare_Above(void) {
    /* Changing SC2 aborts the current conversion. Restart continuous
       conversions so the next interrupt reports a rising 0.5 V crossing. */
    ADC0->SC2 = ADC_SC2_ACFE_MASK | ADC_SC2_ACFGT_MASK;
    ADC0->SC1[0] = ADC_SC1_AIEN_MASK | ADC_SC1_ADCH(12);
}

void ADC_Stop(void) {
    ADC0->SC3 = 0;
    ADC0->SC1[0] = ADC_SC1_ADCH(31);
    ADC0->SC2 = 0;
}

void ADC0_IRQHandler(void) {
    /* Read the result to clear COCO, then record it for the debugger. */
    uint16_t value = (uint16_t)ADC0->R[0];
    debug_adc_value = value;
    ++debug_adc_count;
    debug_irq_flags |= DEBUG_IRQ_ADC;
    Sampling_On_ADC_Complete(value);
}
