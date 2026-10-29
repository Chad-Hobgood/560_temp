#include <MKL25Z4.h>
#include "UART.h"
#include "project.h"

/* 24 MHz bus / (16 samples per bit * 115200 baud) rounds to 13. */
#define UART2_BAUD_DIVISOR 13U
#define UART_RX_BUFFER_SIZE 128U
#define UART_RX_BUFFER_MASK (UART_RX_BUFFER_SIZE - 1U)

/* The ISR writes at rx_head; the foreground reads at rx_tail. */
static volatile uint8_t rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint8_t rx_head = 0;
static volatile uint8_t rx_tail = 0;


void UART2_Write_Char(char value) {
    /* Wait until the transmit register can accept the next character. */
    while ((UART2->S1 & UART_S1_TDRE_MASK) == 0U) {
    }
    UART2->D = (uint8_t)value;
}

void UART2_Write_String(const char *text) {
    while (*text != '\0') {
        UART2_Write_Char(*text++);
    }
}

void UART2_Write_Unsigned(uint32_t value) {
    char digits[10];
    uint8_t count = 0;

    do {
        digits[count++] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);

    while (count > 0U) {
        UART2_Write_Char(digits[--count]);
    }
}

void Init_UART2(void) {
    /* Turn on clocks and stop UART2 while changing its configuration. */
    SIM->SCGC4 |= SIM_SCGC4_UART2_MASK;
    SIM->SCGC5 |= SIM_SCGC5_PORTD_MASK;
    UART2->C2 = 0;

    /* Pin mux 3 connects PTD2 to RX and PTD3 to TX. */
    PORTD->PCR[2] = PORT_PCR_MUX(3);
    PORTD->PCR[3] = PORT_PCR_MUX(3);

    /* SBR=13 gives about 115385 baud; BDH=0 selects one stop bit.
       C1=0 selects eight data bits, no parity, and normal operation. */
    UART2->BDH = 0;
    UART2->BDL = UART2_BAUD_DIVISOR;
    UART2->C1 = 0;
    /* Report UART errors through the same IRQ as received characters. */
    UART2->C3 = UART_C3_ORIE_MASK | UART_C3_NEIE_MASK |
                UART_C3_FEIE_MASK | UART_C3_PEIE_MASK;
    UART2->S2 = 0;

    /* Enable the NVIC line before enabling UART receive interrupts. */
    NVIC_SetPriority(UART2_IRQn, 3);
    NVIC_ClearPendingIRQ(UART2_IRQn);
    NVIC_EnableIRQ(UART2_IRQn);
    UART2->C2 = UART_C2_TE_MASK | UART_C2_RE_MASK | UART_C2_RIE_MASK;
}

int UART2_Read_Char(void) {
    uint8_t value;

    if (rx_tail == rx_head) {
        return -1;
    }

    value = rx_buffer[rx_tail];
    rx_tail = (uint8_t)((rx_tail + 1U) & UART_RX_BUFFER_MASK);
    return value;
}

void UART2_IRQHandler(void) {
    uint8_t status = UART2->S1;
    uint8_t value;
    const uint8_t error_mask = UART_S1_OR_MASK | UART_S1_NF_MASK |
                               UART_S1_FE_MASK | UART_S1_PF_MASK;

    /* Nothing to consume if neither a byte nor an error arrived. */
    if ((status & (UART_S1_RDRF_MASK | error_mask)) == 0U) {
        return;
    }

    /* Reading S1, then D, clears the receive and error status bits. */
    value = UART2->D;
    debug_uart_status = status;
    if ((status & error_mask) != 0U) {
        /* Preserve the error for inspection; discard the bad byte. */
        ++debug_uart_error_count;
        debug_irq_flags |= DEBUG_IRQ_UART_ERROR;
    } else {
        uint8_t next_head = (uint8_t)((rx_head + 1U) & UART_RX_BUFFER_MASK);
        debug_uart_byte = value;
        ++debug_uart_rx_count;
        debug_irq_flags |= DEBUG_IRQ_UART_RX;
        if (next_head == rx_tail) {
            /* Keep existing bytes if a fast sender fills the buffer. */
            ++debug_uart_drop_count;
            debug_irq_flags |= DEBUG_IRQ_UART_DROP;
        } else {
            rx_buffer[rx_head] = value;
            rx_head = next_head;
        }
    }
}
