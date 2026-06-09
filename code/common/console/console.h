// UART-backed interactive console with printf routing.
// Call console_init() once after UART is ready.
// Implement console_execute() in your application to handle commands.
#ifndef CONSOLE_H
#define CONSOLE_H

#include "microrl.h"
#include "stm32g4xx_hal.h"

extern microrl_t console_rl;

// Set up microrl, route printf to huart, print banner, arm UART RX interrupt.
void console_init(UART_HandleTypeDef *huart, IRQn_Type irqn);

// Feed one received byte into microrl; re-arms the UART interrupt.
void console_rx_callback(uint8_t ch);

#endif
