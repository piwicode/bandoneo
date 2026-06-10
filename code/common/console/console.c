#include "console.h"
#include <stdio.h>

microrl_t console_rl;

static UART_HandleTypeDef *console_uart;
static uint8_t console_rx_char;

/* Routes printf to console_uart. Callers use explicit \r\n. */
int _write(int fd, char *buf, int len)
{
  (void)fd;
  if (!console_uart) return len;
  HAL_UART_Transmit(console_uart, (uint8_t *)buf, len, HAL_MAX_DELAY);
  return len;
}

static void console_print(const char *str)
{
  printf("%s", str);
  fflush(stdout);
}

extern int console_execute(int argc, const char * const *argv);

void console_rx_callback(uint8_t ch)
{
  microrl_insert_char(&console_rl, ch);
  HAL_UART_Receive_IT(console_uart, &console_rx_char, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  if (huart == console_uart) {
    console_rx_callback(console_rx_char);
  }
}

void console_init(UART_HandleTypeDef *huart, IRQn_Type irqn)
{
  console_uart = huart;

  printf("\r\n=== Bandoneo Console ===\r\n");
  fflush(stdout);

  if (!NVIC_GetEnableIRQ(irqn)) {
    printf("WARNING: IRQ %d not enabled in NVIC, console RX will not work.\r\n"
           "Enable the UART global interrupt in the .ioc NVIC settings and regenerate code.\r\n",
           (int)irqn);
    fflush(stdout);
  }

  microrl_init(&console_rl, console_print);
  microrl_set_execute_callback(&console_rl, console_execute);

  HAL_UART_Receive_IT(console_uart, &console_rx_char, 1);
}
