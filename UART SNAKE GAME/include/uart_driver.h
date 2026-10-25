#ifndef UART_DRIVER_H
#define UART_DRIVER_H

#include "main.h"

void UART_Init(void);
void UART_SendString(const char *str);
void UART_SendLine(const char *str);    /* appends \r\n */
/* Command parser called from IRQ or main loop — implemented Step 2 */
void UART_ProcessCommands(void);

#endif /* UART_DRIVER_H */