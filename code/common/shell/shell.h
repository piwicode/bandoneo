// UART-backed interactive shell.
// Call shell_init() once after UART is ready.
// Implement shell_execute() in your application to handle commands.
#ifndef SHELL_H
#define SHELL_H

#include "microrl.h"
#include "stm32g4xx_hal.h"

extern microrl_t shell_rl;

// Set up microrl, print banner, arm UART RX interrupt, enable NVIC.
void shell_init(UART_HandleTypeDef *huart, IRQn_Type irqn);

// Feed one received byte into microrl; re-arms the UART interrupt.
void shell_rx_callback(uint8_t ch);

#endif
