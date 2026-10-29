#ifndef UART_H
#define UART_H

#include <stdint.h>

/* Configure UART2 for the project's fixed 115200 baud, 8N1 link. */
void Init_UART2(void);
void UART2_Write_Char(char value);
void UART2_Write_String(const char *text);
void UART2_Write_Unsigned(uint32_t value);

/* Return the next received byte, or -1 when the receive buffer is empty. */
int UART2_Read_Char(void);

#endif
