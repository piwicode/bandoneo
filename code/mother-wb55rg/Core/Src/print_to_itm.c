/*
 * Writes standard output to ITM.
 *
 * Instrumentation Trace Macrocell (also known as ITM) is a specialized hardware block
 * that transfers high-speed diagnostic data from the MCU to the developer's workstation.
 *
 * 1. Enable Serial Wire Debugger (also known as SWO) pin in the board configuration:
 * 	  `System Core` > `Sys` > `Debug` = `Trace Asynchronous SW`.
 *
 * 2. In the debugger configuration, go to `Debugger` tab, tick `Enable` in
 *    `Serial Wire Debugger (SWO)` and use the `To CPU1 FCLK` speed as `Core Clock` speed.
 *
 * 3. Run the program with debugger attached.
 * 4. While execution is paused, go to the `SWV ITM Data Console` and press the `tool` icon
 *    to `Configure Traces`, the enable `ITM Stimulus Port` 0
 * 5. Press the red icon `Start Trace`, and resume execution.
 */

#include <stdio.h>
#include "main.h"
// Overrides the "weak" symbol providing default implementation for "puts" `putchar` and `printf`
// defined in Core/Src/syscalls.c

int _write(int file, char *ptr, int len) {
	// Skip logging when not debugger is attached to consume the buffer.
//	if ((CoreDebug->DHCSR & CoreDebug_DHCSR_C_DEBUGEN_Msk)
//			&& (ITM->TCR & ITM_TCR_ITMENA_Msk) && (ITM->TER & (1UL << 0))) {
		for (int i = 0; i < len; i++) {
			ITM_SendChar(*ptr++);
		}
//	}

	return len;
}
