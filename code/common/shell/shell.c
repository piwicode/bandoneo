#include "shell.h"
#include <stdio.h>

// microrl instance — shared across all shell operations
microrl_t shell_rl;

// UART handle — set by shell_init(), used in RX callback to re-arm interrupt
static UART_HandleTypeDef *shell_uart;

// RX buffer — holds single byte from HAL_UART_Receive_IT(); valid only in RxCpltCallback
static uint8_t shell_rx_char;

static void shell_print(const char *str)
{
  printf("%s", str);
  fflush(stdout);
}

extern int shell_execute(int argc, const char * const *argv);

void shell_rx_callback(uint8_t ch)
{
  microrl_insert_char(&shell_rl, ch);
  HAL_UART_Receive_IT(shell_uart, &shell_rx_char, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == shell_uart) {
    shell_rx_callback(shell_rx_char);
  }
}

// Initialize shell: set up microrl, arm UART RX interrupt, enable NVIC.
// Must be called after UART is initialized and before entering main loop.
// Params: huart — UART handle (e.g., &huart1)
//         irqn — UART interrupt number (e.g., USART1_IRQn)
void shell_init(UART_HandleTypeDef *huart, IRQn_Type irqn)
{
  shell_uart = huart;

  printf("\r\n=== Bandoneo Console ===\r\n");
  fflush(stdout);

  microrl_init(&shell_rl, shell_print);
  microrl_set_execute_callback(&shell_rl, shell_execute);

  HAL_UART_Receive_IT(shell_uart, &shell_rx_char, 1);
  NVIC_EnableIRQ(irqn);
  fflush(stdout);
}
