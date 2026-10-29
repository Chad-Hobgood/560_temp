#include <MKL25Z4.h>
#include "project.h"
#include "timers.h"

/* CLOCK_SETUP=1 gives TPM0 24 clock ticks per microsecond. */
#define TPM_TICKS_PER_US 24U

void Init_TPM0(uint16_t period_us) {
    /* Enable TPM0 and select the 24 MHz PLL/FLL timer clock. */
    SIM->SCGC6 |= SIM_SCGC6_TPM0_MASK;
    SIM->SOPT2 = (SIM->SOPT2 & ~SIM_SOPT2_TPMSRC_MASK) |
                 SIM_SOPT2_TPMSRC(1);
    TPM0->SC = 0; /* Stop the counter and use a 1:1 prescaler. */
    Set_Sample_Period(period_us);
    TPM0->SC = TPM_SC_TOIE_MASK; /* Enable overflow IRQ; keep timer stopped. */

    /* Route TPM0 overflows to the interrupt handler. */
    NVIC_SetPriority(TPM0_IRQn, 2);
    NVIC_ClearPendingIRQ(TPM0_IRQn);
    NVIC_EnableIRQ(TPM0_IRQn);
}

void Set_Sample_Period(uint16_t period_us) {
    /* ECE 560 permits whole-number periods from 5 to 1000 us. */
    if ((period_us < 5U) || (period_us > 1000U)) {
        return;
    }

    /* TPM0 counts from zero through MOD, so subtract one from the ticks. */
    TPM0->SC &= ~TPM_SC_CMOD_MASK; /* Stop while changing the period. */
    TPM0->CNT = 0;
    TPM0->MOD = (uint16_t)(TPM_TICKS_PER_US * period_us - 1U);
    sample_period_us = period_us;
}

void Start_Sample_Timer(void) {
    /* Start a fresh period; clear an old overflow before enabling counting. */
    TPM0->CNT = 0;
    TPM0->SC |= TPM_SC_TOF_MASK; /* Write one to clear TOF. */
    TPM0->SC = TPM_SC_TOIE_MASK | TPM_SC_CMOD(1);
}

void Stop_Sample_Timer(void) {
    /* Leave the clock and interrupt configuration in place for a restart. */
    TPM0->SC &= ~TPM_SC_CMOD_MASK;
}

void TPM0_IRQHandler(void) {
    /* The TPM0 vector can serve other sources; only count overflows here. */
    if (TPM0->SC & TPM_SC_TOF_MASK) {
        TPM0->SC |= TPM_SC_TOF_MASK; /* Write one to clear TOF. */
        ++debug_timer_count;
        debug_irq_flags |= DEBUG_IRQ_TIMER;
    }
}
