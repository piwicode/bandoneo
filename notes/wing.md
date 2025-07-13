# Wing

There are two candidates:

|   | [STM32F030C8T6](https://jlcpcb.com/partdetail/Stmicroelectronics-STM32F030C8T6/C23922) | [STM32F103C8T6](https://jlcpcb.com/partdetail/Stmicroelectronics-STM32F103C8T6/C8734) | [STM32WB1MMC](https://jlcpcb.com/partdetail/STMicroelectronics-STM32WB1MMCH6TR/C5456183) STM32WB15CCY | [STM32WB5MMGH6TR](https://jlcpcb.com/partdetail/STMicroelectronics-STM32WB5MMGH6TR/C5184586)  
STM32WB55VGY |
| --- | --- | --- | --- | --- |
|   | **Pick for higher channel number: 12 x 4 = 48 keys** |   |   | **Pick for cheaper and more SPI and touch sensing** |
| Price | .83 € | 1.18 € | 10.89 € stock: 10 | 9.59 € stock:46 |
| Clock ext | 48 MHz | 72 MHz | 64 MHz  | 64 MHz |
| Clock Int | 8 MHz | 8 MHz |   | 16 MHz ? HSI16 |
| Consumption | 4.4 mA |   | 91 μA/MHz | 107  μA/MHz ~ 3mA |
| Memory | SRAM 8Kb | SRAM 20 Kb | SRAM 48Kb | SRAM 256Kb |
| Poer supply | 2.4-3.6V | **2.0**\-3.6V | 1.71-3.6V | 1.8-3.6V |
| ADC | 1x12-bit @ 1 Msps | 2x12-bit @ 1Msps | 12-bit ADC @ 2.5 Msps | 12-bit ADC |
| Channels | 12 channels | 10 channels | 10 channels | 16 channels |
| GPIO | 39 | 37 | 25 | 76 |
| SPI Bus | 2 SPI @ 18 Mbit/s | 2 SPI @ 18 Mbit/s | 1 SPI @ 32 Mbit/s | 2 SPI @ 32 Mbit/s |
| USB Bus |   |   |   | 1 x USB 2.0 |
| Connectivity | SWD | SWD, JTAG | SWD, JTAG | SWD, JTAG |
| DMA | 5-channelswith SPI | 7 channels with SPI | 7 channels with SPI | 2 controlers with SPI |
| Bootloader |   |   | Suports USART, SPI, I2C |   |
| Capacitive senseing | \- | \- | 2 | 18 sensors |
|   |   |   | [Dev board](https://www.mouser.fr/ProductDetail/STMicroelectronics/NUCLEO-F030R8?qs=fK8dlpkaUMvL9GSuoYnNYw%3D%3D&mgh=1&vip=1&utm_id=18189432403&utm_source=google&utm_medium=cpc&utm_marketing_tactic=emeacorp&gad_source=1&gad_campaignid=18194208143&gclid=Cj0KCQjwj8jDBhD1ARIsACRV2TuMFzR2xPGz4mOad_9n7fF12duHYsjhRrJ_6VzR5CbDneVjlzRqI1IaAkFQEALw_wcB) |   |
| Audio |   |   |   | 1 SAI |

Comment utiliser SWD? Peut on programmer par USB.

Should I uses embedded CRC? DMA?

## Capture

Assuming 40 keys @ 1 khz, 40 khz would be enough.

10 channels is the strict minimum to have 40 keys and a simple layout.

## Protocol: full frame

Assuming all keys values are sent at each frames: 40 keys x 12 bits = 480 bits.

A 18 Mhz bus, can achieve 37.5K frames per seconds.

A .5 Mhz but should be enough.