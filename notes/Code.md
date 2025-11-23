The entrypoint fo STM32 developmenet kit is the source checkout:

git clone --recursive https://github.com/STMicroelectronics/STM32CubeWB.git

## Simple text output to COM port:

My nucleo board model is MB1355D, which is documented by STM32WB55RG D01 schematics.
The board STLink interface uses STM32F103CBT6, which is connected to STM32WB55_QFN68
via a the set of 6 jumpers labeled JP5.

* TX_STlink, RX_STlink: a USART that is forwarded to the USB as a virtual COM port.
* SWCLK, SWDIO,NRST: the [Serial Wire Debugs signals](https://developer.arm.com/documentation/101761/1-0/Debug-and-trace-interface/Serial-Wire-Debug-signals)
* SWO: low bandwidth traces, alongside to SWD signals.

The schematics tells the virtual COM port is connected to PB6 PB7.

Configured in STM32CubeIDE the following:

* Connectivity / USART1:
  * Mode: Asynchronous
  * Hardware Flow Control RS232 and RS485: Disable
  * GPIO mode: Alternate Fuction Push Pull
  * GPIO Push-Pull: Pull-up
  * Maximum output speed: Very High
  * Fast Mode: Disable
  * Baud Rate: 115200 / 8 bit / No parity / stop bit:1

When regerating the code with `ALT-K` the `MX_USART1_UART_Init` is generated.

In `min.c` write some data:

```
const char *test_msg = "Test message\r\n";
HAL_UART_Transmit(&huart1, (uint8_t*)test_msg, strlen(test_msg), HAL_MAX_DELAY);
```

Locate the port on linux with:

```
$ sudo dmesg | grep tty
[62970.469804] cdc_acm 1-1:1.2: ttyACM1: USB ACM device
```


Install minicom `sudo apt instal minicom` then run:

```
minicom -b 115200 -D /dev/ttyACM0
```

## Write to SWO:

As seen above the nucleo board has SWO support, but STLink clone dongle don't.
This can be improved.

https://www.eevblog.com/forum/microcontrollers/quick-hack-to-get-swo-on-st-link-clones/

Configured in STM32CubeIDE the following:

* System Core / SYS:
  * Debug: Trace Asynchronous Sw

Characters written to `ITM_SendChar` show show up in a view names
`SWV ITM Data Console`, but the configuration button does not work, and that
falls short.

* In STM32Cube IDE debug configuration enable SWO and use the `To CPU1 FCLK`
  speed as Core Clock speed, that is to say 4MHz.
* Then add in "Windows" > Show View > SDW > SDW ITM Data Console
* Enable ITM Stimulus Port 0 in the parameter of this view, and finally press
  the red button.

Reference: 
- https://www.youtube.com/watch?v=j-GaEZKrkbQ

## Blink leds

Once the pins have been configured for GPIO out and labeled it is possible to
control the output with:

```
HAL_GPIO_WritePin(LedRed_GPIO_Port, LedRed_Pin, GPIO_PIN_SET);
```

## Read from ADC:

There are several way to wait from completion. I will try first to get an interupt.

* Configure the pin for analog input in "Analog" > ADC1.
* Pay attention to enable "ADC1 global interrupt" in VNIC Settings otherwise
  `ADC1_IRQHandler` is not generated in `stm32wbxx_it.cc`.
* Then a symbol `void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)` must be
  defined to override the weak symbol used by the drivers.

Sticking to ADC_SingleConversion_TriggerSW_IT is usefull.

## Open points

* [ ] How does capacitive sensing work?
* [ ] Do I need hardware CRC ?
* [ ] Does bluetooth pairing requires a batterie ?
* [ ] How does the watchdog work (WWDG) ?
* [ ] How does SPI work ?
* [ ] How does USB MIDI work ?
* [ ] How does BT MIDI works ?
* [ ] How does BT AUDIO works ? Do we need WBA55 ?

## Further reading:

* https://github.com/STMicroelectronics/STM32CubeWB
*
  https://docs.zephyrproject.org/latest/boards/st/nucleo_wb55rg/doc/nucleo_wb55rg.html