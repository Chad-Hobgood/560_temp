#include <MKL25Z4.h>
#include "ADC.h"
#include "LEDs.h"
#include "UART.h"
#include "project.h"
#include "timers.h"

/* Watch these variables in the debugger. Interrupt handlers update them. */
volatile uint8_t app_state = APP_CLI;
volatile uint32_t debug_irq_flags = 0;
volatile uint32_t debug_uart_rx_count = 0;
volatile uint32_t debug_uart_error_count = 0;
volatile uint32_t debug_adc_count = 0;
volatile uint32_t debug_timer_count = 0;
volatile uint8_t debug_uart_byte = 0;
volatile uint8_t debug_uart_status = 0;
volatile uint16_t debug_adc_value = 0;

/* Recording settings from the project specification. */
uint16_t samples[PROJECT_MAX_SAMPLES];
uint16_t sample_count = PROJECT_DEFAULT_SAMPLES;
uint16_t sample_period_us = PROJECT_DEFAULT_PERIOD_US;
uint8_t sample_channel = PROJECT_DEFAULT_CHANNEL;

static void Show_State(uint8_t state) {
    /* The board LEDs are active low; Control_RGB_LEDs handles that detail. */
    switch (state) {
        case APP_CLI:
            Control_RGB_LEDs(0, 0, 1); /* Blue */
            break;
        case APP_AWAIT_TRIGGER:
            Control_RGB_LEDs(1, 1, 0); /* Yellow */
            break;
        case APP_RECORDING:
            Control_RGB_LEDs(0, 1, 0); /* Green */
            break;
        case APP_SEND_DATA:
            Control_RGB_LEDs(1, 0, 1); /* Magenta */
            break;
        default:
            Control_RGB_LEDs(0, 0, 0); /* Invalid state: LEDs off. */
            break;
    }
}

int main(void) {
    uint8_t shown_state = APP_CLI;

    /* Initialize the serial link, ADC, and sample timer once at startup. */
    Init_RGB_LEDs();
    Show_State(shown_state);
    Init_UART2();
    Init_ADC();
    Init_TPM0(sample_period_us);
#if PROJECT_DEBUG_START_INTERRUPTS
    /* Optional smoke test: one ADC result and recurring timer ticks. */
    Start_ADC_Conversion(sample_channel);
    Start_Sample_Timer();
#endif

    /* For now, the foreground only keeps the LED color in sync with state.
       IRQ flags and counters remain available for debugger inspection. */
    for (;;) {
        uint8_t current_state = app_state;
        if (current_state != shown_state) {
            shown_state = current_state;
            Show_State(current_state);
        }
    }
}
