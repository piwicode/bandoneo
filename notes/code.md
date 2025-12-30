# Programming STM32WB55 and STM32WB65RI

They come with nucleao-wb55 and nucleo-wb65ri development board.

The entrypoint for STM32 developmenet kit is the source checkout:

```
git clone --recursive https://github.com/STMicroelectronics/STM32CubeWB.git
git clone --recursive https://github.com/STMicroelectronics/STM32CubeWBA.git
```

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

## Log debug messages with SWO:

As seen above the nucleo board STLink has SWO support, but STLink clone dongle don't.
This can be improved with a soldering iron:

https://www.eevblog.com/forum/microcontrollers/quick-hack-to-get-swo-on-st-link-clones/

Configured in STM32CubeIDE in:
* For `stm32wg55` :  `System Core` > `Sys` > `Trace Asynchronous SW`.
* For `stm32wg65` :  `Trace and Debug` > `DEBUG` > `Debug` = `Trace Asynchronous SW`

This assign 3 pins:
* `SYS_JTMS_SWDIO` - required for debugging. Data input and output.
* `SYS_JTCK_SWCLK` - required for debugging. Clock.
* `SYS_JTDO_SWO` - optional for debugging. Provides sampling, real time watch and traces

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

There are several way to wait for convertion: polling, interrupt and DMA.

### Polling
* Start conversion with `HAL_ADC_Start`
* Wait for completion with `HAL_ADC_PollForConversion`
* Fetch results with `HAL_ADC_GetValue`

### Interupt

* Configure the pin for analog input in "Analog" > ADC1.
* Enable "ADC1 global interrupt" in VNIC Settings otherwise `ADC1_IRQHandler` is
  not generated in `stm32wbxx_it.cc`.
* Define a symbol `void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)` to
  override the weak symbol used by the drivers.
* Trigger conversion with `HAL_ADC_Start_IT`, get the result in the interrupt function with `HAL_ADC_GetValue`

## USB

USB is a serial device limited to .5A, 5V, and 5 meters.
* STM32 MCU implement USB 2.0.
* USB uses a tiered star topology
  * Each device has a 7 bit address, including the hub.
  * Adress 0 is reserved to device enumeration, leaving 127 device addresses available.
* The signal goes on a differential pair, self synchronized which requires a precise clock (builtin STM32WB55).
* Communication starts with Low Speed (LS) or Fast Speed (FS) depending on
  devices hardwared pullups setup, and can attempt to switch to High Speed (HS)
  on device's demand. (STM32WB55 only support FS)
* Datagrams are sent to an address and to an endpoints with the following types:
  * setup - configuration usb 1, publish de capability of device.
  * bulk - typical for mas storage. No time constraint, integrity constraint. Typical for mass storage.
  * interrupt - typically a mouse. Tiny requests from the host. Poll interval is 1ms.
  * isochonous - typically for the audio, time constrained, but no integrity. It is high priority.
* There are keepalive signals named start-of-frame packet. 3 missed mean suspend.
* `Token` packets are signaling the destination, it is followed by a data packet, and there is an `acknowledge` packet to close the transaction.
* All workding "out" and "in" are host centric. All exchanges are initiated by host.
* A device publishes a set of configurstions  only one can be active at a time. The configuration has a set of interfaces.

* USB standard classes corresponds to features implemented by OS drivers.
  * CDC virtual com port


One can request a vendor id on www.usb.org required for have the USB logo on a product; STM32 can provide a PID.

* The code sample can be found in `Projects/P-NUCLEO-WB55.Nucleo/Applications/USB_Device/HID_Standalone`
* HID stands for Human Interface Device.
* USB supports multiple transfer types, we are interested in isochronous trnsfer or interrupt transfer fro minimal latency.

Reference:
* Documenation https://wiki.st.com/stm32mcu/wiki/Introduction_to_USB_with_STM32
* Video series https://www.youtube.com/playlist?list=PLnMKNibPkDnFFRBVD206EfnnHhQZI4Hxa


## USB Hardware

* The USB D+ and D- is a differential pair. It should have a single ended impedence of 45 Ohms and 90 Ohms differential impedence. STM32 USB pins embedd resistors in line with USB specification.
* It is possible to detect connection with a voltage divider and a GPIO PIN.
  * TODO: Add a connection GPIO PIN
* Some MCU require a 1.5 Kohm pullup on D+ controled by a GPIO.
* Electro Static Discharge (ESD) protection is required using USBLC6-2SC6 or USBLC6-4SC6 (ref AN4879) or ESDA6V1BC6 (ref training videos); plus ESDA7P60-1U1M  on the VBUS
  * Switch the design to USBLC6 which is an STMicroelectronics part given that most of the cose will be du to extended part assembly cost.
  * Also self powered devices should not connect pin 5, VBUS should be connected to a regular pin such as pin 6, becaue of the D+ pullup. (To be further investigated from the video)

* AN4879 tells that STM32WB55
  * has an embedded pullup resistor
  * has Batery Charging Detection which allow to detect and draw more current from USB.

* When VBat is 0V and VDDUSB is 5V, because GPIO pin only allow VDD+4.0V (Table 13 of usb Voltage Characteristics), this should be prevented. PA9 requires 200 uA.
  * TODO Verify if I need USB BUS sensing: YES and differentialte suspend and disconnection events.
  * TODO: How is it done in the board?
* When a device is suspended (no activity on dataline), the amout of current that can be drawn is limited to a couple of mA.
* TODO: Should I wire the ID pin of my USB connector to a 100kOhm pulldown.

### Enable USB in STM21CUBEIDE

For STM32WBA65, open the `.ioc` file and then:
1. Go to `System Core` > `GPIO` (also known as `General Input Output`), select
   tab `RCC` (`Reset and Clock Control`)
2. Bind pin `OSC_IN` to signal `RCC_OSC_IN` and `OSC_OUT` to `RCC_OSC_OUT`.
3. Go to `Connectivity` > `USB_OTG_HS` (OTG means Over The Go which indicates
   the processor can either be a device or a host, and HS means Hish Speed which
   indicates the fastest communication speed capability). If OSC pins are
   correctly binded it should no longer be greyed out (disabled).
4. Set `Internal HS Phy` to `Device_Only` and in the `Parameter Settings` > `OTG
   PHY reference clock selection` set `32 MHz` for the nucleo board.
5. Ensure `PLL1 Source Mux` and `OTG_S PHY Clock Mux` use `HSE`, otherwise you
   might block forever in `USB_CoreReset()`.

In the code add an interrupt handler to `stm32wbaxx_it.cc`:

```
void USB_OTG_HS_IRQHandler(void)
{
  // Call TinyUSB's DCD interrupt handler
  // rhport 0 = USB_OTG_HS (first/only controller when only HS is defined)
  dcd_int_handler(0);
}
```


* [ ] TODO: Investigate hardware requirements on the final mdoule. Are external oscilators required?

Discussion: https://community.st.com/t5/stm32-mcus-boards-and-hardware/nucleo-wb55rg-user-usb-connection-sensing/m-p/866010#M29057

### Debugging Default_Handler

When the code halts in teh default handler infinite loop, it is likely that an
interrupe without handler was triggered. In Gdb:

`print ($xpsr & 0x1f)`

Then go to `g_pfnVectors` to find the matching handlin in the list (0 based).

## USB Midi

https://github.com/Hypnotriod/midi-box-stm32/tree/master

## STM32CubeIDE

* Activate NOE activates a USB_NOE output bit that can be used to show
  transmission acivity on a led.


* Transmission line documentation: https://www.youtube.com/watch?v=**u5Cgycpvq6Y**
* JCLPcb blog post https://jlcpcb.com/blog/selecting-the-right-impedance-for-usb-ethernet-hdmi-and-sd-card-interfaces
*
## Open points

* [ ] Should I add a 100KOhm to boot0 from stlink like in the devboard?
* [ ] How does capacitive sensing work?****
* [ ] Do I need hardware CRC ?
* [ ] Does bluetooth pairing requires a batterie ?
* [ ] How does the watchdog work (WWDG) ?
* [ ] How does SPI work ?
* [ ] How does USB MIDI work ?
* [ ] How does BT MIDI works ?
* [ ] How does BT AUDIO works ? Do we need WBA55 ?

## STM32CubeIde

* Issue: `"Java" is not responding` error showing up on the splash screen.
* Fix: remove ./.metadata/.plugin/org.eclipse.e4.workbench org.eclipse.e4.workbench

## Further reading:

* https://github.com/STMicroelectronics/STM32CubeWB
*
  https://docs.zephyrproject.org/latest/boards/st/nucleo_wb55rg/doc/nucleo_wb55rg.html