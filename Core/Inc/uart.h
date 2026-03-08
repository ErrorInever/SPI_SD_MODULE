#ifndef UART_H_
#define UART_H_

#include "stm32f446xx.h"
#include <stdbool.h>

extern volatile bool needs_log;
void USART2_init(void);
void USART2_send_byte(const char byte);
void USART2_log(const char *str);
void UART_Printf(const char *format, ...);

#endif