#include "project.h"

/* Interrupt counters and flags are kept in one file for debugger watches. */
volatile uint8_t app_state = APP_CLI;
volatile uint32_t debug_irq_flags = 0;
volatile uint32_t debug_uart_rx_count = 0;
volatile uint32_t debug_uart_error_count = 0;
volatile uint32_t debug_uart_drop_count = 0;
volatile uint32_t debug_adc_count = 0;
volatile uint32_t debug_timer_count = 0;
volatile uint8_t debug_uart_byte = 0;
volatile uint8_t debug_uart_status = 0;
volatile uint16_t debug_adc_value = 0;
