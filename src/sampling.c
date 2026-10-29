#include "ADC.h"
#include "project.h"
#include "sampling.h"
#include "timers.h"

/* These settings and results are visible in the debugger. */
uint16_t samples[PROJECT_MAX_SAMPLES];
uint16_t sample_count = PROJECT_DEFAULT_SAMPLES;
uint16_t sample_period_us = PROJECT_DEFAULT_PERIOD_US;
uint8_t sample_channel = PROJECT_DEFAULT_CHANNEL;
volatile uint16_t recorded_count = 0;
volatile uint32_t debug_sample_overrun_count = 0;
volatile uint32_t debug_trigger_count = 0;
volatile uint32_t debug_capture_complete_count = 0;

#define TRIGGER_WAIT_BELOW 0U
#define TRIGGER_WAIT_ABOVE 1U

static volatile uint8_t trigger_phase = TRIGGER_WAIT_BELOW;
static volatile uint8_t capture_complete = 0;

void Sampling_Arm(void) {
    /* First see PTB2 below 0.5 V, then wait for its upward crossing. */
    recorded_count = 0;
    capture_complete = 0;
    debug_timer_count = 0;
    debug_sample_overrun_count = 0;
    trigger_phase = TRIGGER_WAIT_BELOW;
    app_state = APP_AWAIT_TRIGGER;
    ADC_Start_Trigger_Monitor();
}

void Sampling_Force(void) {
    /* The first conversion starts now. Timer periods follow that sample. */
    recorded_count = 0;
    capture_complete = 0;
    debug_timer_count = 0;
    debug_sample_overrun_count = 0;
    app_state = APP_RECORDING;
    ADC_Start_Single(sample_channel);
}

uint8_t Sampling_Take_Complete(void) {
    uint8_t done = capture_complete;
    capture_complete = 0;
    return done;
}

static void Store_Sample(uint16_t value) {
    samples[recorded_count++] = value;
    if (recorded_count == sample_count) {
        /* Each later sample should have one TPM0 overflow. Extra overflows
           mean an ADC trigger or result was missed during this capture. */
        if (debug_timer_count > (uint32_t)(sample_count - 1U)) {
            debug_sample_overrun_count =
                debug_timer_count - (uint32_t)(sample_count - 1U);
        }
        Stop_Sample_Timer();
        ADC_Stop();
        ++debug_capture_complete_count;
        capture_complete = 1;
        app_state = APP_CLI;
    } else if (recorded_count == 1U) {
        /* Configure a hardware-triggered ADC conversion on each overflow.
           This removes interrupt dispatch time from the sample period. */
        ADC_Start_Timer_Trigger(sample_channel);
        Start_Sample_Timer();
    }
}

void Sampling_On_ADC_Complete(uint16_t value) {
    if (app_state == APP_AWAIT_TRIGGER) {
        if (trigger_phase == TRIGGER_WAIT_BELOW) {
            /* ADC compare interrupted because PTB2 is below threshold. */
            trigger_phase = TRIGGER_WAIT_ABOVE;
            ADC_Trigger_Compare_Above();
        } else {
            ++debug_trigger_count;
            ADC_Stop();
            app_state = APP_RECORDING;
            if (sample_channel == 12U) {
                /* The crossing conversion is already sample one. */
                Store_Sample(value);
            } else {
                /* For internal channels, convert the selected channel now. */
                ADC_Start_Single(sample_channel);
            }
        }
    } else if (app_state == APP_RECORDING) {
        Store_Sample(value);
    }
}

