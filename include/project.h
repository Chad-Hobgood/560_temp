#ifndef PROJECT_H
#define PROJECT_H

#include <stdint.h>

#define PROJECT_MAX_SAMPLES       2000U
#define PROJECT_DEFAULT_SAMPLES   20U
#define PROJECT_DEFAULT_PERIOD_US 100U
#define PROJECT_DEFAULT_CHANNEL   12U
#define PROJECT_TRIGGER_MV        500U
#define PROJECT_ADC_REFERENCE_MV  3300U
#define PROJECT_TRIGGER_ADC_CODE  ((PROJECT_TRIGGER_MV * 65535UL + \
                                    PROJECT_ADC_REFERENCE_MV / 2U) / \
                                   PROJECT_ADC_REFERENCE_MV)
#define DEBUG_IRQ_UART_RX    (1UL << 0)
#define DEBUG_IRQ_UART_ERROR (1UL << 1)
#define DEBUG_IRQ_ADC        (1UL << 2)
#define DEBUG_IRQ_TIMER      (1UL << 3)
#define DEBUG_IRQ_UART_DROP  (1UL << 4)

/* Values stored in the global app_state variable. */
#define APP_CLI           0U
#define APP_AWAIT_TRIGGER 1U
#define APP_RECORDING     2U
#define APP_SEND_DATA     3U

extern volatile uint8_t app_state;
extern volatile uint32_t debug_irq_flags;
extern volatile uint32_t debug_uart_rx_count;
extern volatile uint32_t debug_uart_error_count;
extern volatile uint32_t debug_uart_drop_count;
extern volatile uint32_t debug_adc_count;
extern volatile uint32_t debug_timer_count;
extern volatile uint8_t debug_uart_byte;
extern volatile uint8_t debug_uart_status;
extern volatile uint16_t debug_adc_value;
extern volatile uint32_t debug_sample_overrun_count;
extern volatile uint32_t debug_trigger_count;
extern volatile uint32_t debug_capture_complete_count;

extern uint16_t samples[PROJECT_MAX_SAMPLES];
extern uint16_t sample_count;
extern uint16_t sample_period_us;
extern uint8_t sample_channel;
extern volatile uint16_t recorded_count;

#endif
