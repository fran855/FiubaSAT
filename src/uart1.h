#ifndef UART1_H
#define UART1_H

#include "task.h"
#include "uart2.h"
#include <queue.h>
#include "semphr.h"

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/cm3/nvic.h>

extern SemaphoreHandle_t uart1_mutex; // Mutex for UART

void taskUART1_transmit(void *args __attribute__((unused)));
void taskUART1_receive(void *args __attribute__((unused)));

uint16_t UART1_puts(const char *s);
void UART1_putchar(char ch);
void UART1_setup(void);

#endif /* ifndef UART1_H */