# Wing

There are two candidates:

| Reference | [STM32F030C8T6](https://jlcpcb.com/partdetail/Stmicroelectronics-STM32F030C8T6/C23922) | [STM32F103C8T6](https://jlcpcb.com/partdetail/Stmicroelectronics-STM32F103C8T6/C8734) | [STM32WB1MMC](https://jlcpcb.com/partdetail/STMicroelectronics-STM32WB1MMCH6TR/C5456183) | [STM32WB5MMGH6TR](https://jlcpcb.com/partdetail/STMicroelectronics-STM32WB5MMGH6TR/C5184586) |
| --- | --- | --- | --- | --- |
| Processor |   |   | STM32WB15CCY | STM32WB55VGY |
| Price (Stock) |   |   | 17.43$ (0) | 15.18$ (35) |
| Package |   | LQFP-48\_7x7 |   |   |
| Decision | **Pick for smallest** | **Pick for 2 ADC, low power** |   |   |
| Price | .83 € stock: 72032 | ✅ .90 € stock: 159851 | ⚠️ 10.89 € stock: 10 | ⚠️ 9.59 € stock:46 |
| Clock ext | 48 MHz | 72 MHz | 64 MHz | 64 MHz |
| Clock Int | 8 MHz | 8 MHz |   | 16 MHz |
| Consumption | 4.4 mA |   | 91 μA/MHz | 107  μA/MHz ~ 3mA |
| Memory | SRAM 8Kb | SRAM 20 Kb | SRAM 48Kb | SRAM 256Kb |
| Power supply | 2.4-3.6V | **✅ 2.0**\-3.6V | 1.71-3.6V | 1.8-3.6V |
| ADC | 1x12-bit @ 1 Msps | ✅ 2x12-bit @ 1Msps | 12-bit ADC @ 2.5 Msps | 12-bit ADC |
| Channels | 10 channels + TMP & VREF hardwired | 10 channels | 10 channels | 16 channels |
| GPIO | 39 | 37 | 25 | 76 |
| SPI Bus | 2 SPI @ 18 Mbit/s | ✅ 2 SPI @ 18 Mbit/s | 1 SPI @ 32 Mbit/s | 2 SPI @ 32 Mbit/s |
| USB Bus |   | 1 |   | 1 x USB 2.0 |
| Connectivity | SWD | SWD, JTAG | SWD, JTAG | SWD, JTAG |
| DMA | 5-channelswith SPI | 7 channels with SPI | 7 channels with SPI | 2 controlers with SPI |
| Bootloader |   |   | Suports USART, SPI, I2C |   |
| Capacitive sensing | \- | \- | 2 | 18 sensors |
|   |   |   | [Dev board](https://www.mouser.fr/ProductDetail/STMicroelectronics/NUCLEO-F030R8?qs=fK8dlpkaUMvL9GSuoYnNYw%3D%3D&mgh=1&vip=1&utm_id=18189432403&utm_source=google&utm_medium=cpc&utm_marketing_tactic=emeacorp&gad_source=1&gad_campaignid=18194208143&gclid=Cj0KCQjwj8jDBhD1ARIsACRV2TuMFzR2xPGz4mOad_9n7fF12duHYsjhRrJ_6VzR5CbDneVjlzRqI1IaAkFQEALw_wcB) |   |
| Audio |   |   |   | 1 SAI |

### Bootloader

At startup, boot pins are used to select one of three boot options:  
\- Boot from user Flash  
\- Boot from System memory  
\- Boot from embedded SRAM  
The boot loader is located in System memory.

It only supports USART (V2.2) protocol on USART1 ( on PA8-12), hence it is unlikely that I will support update via the main board.

## Jtag

The JTAG TMS and TCK pins are shared with SWDIO and SWCLK

Is it fine not to connect vbat?

\<?>

Comment utiliser SWD? Peut on programmer par USB.

Should I uses embedded CRC? DMA?

## STM32 Processor configuraiton.

I would enable  WWDG to detect when the processor is no longer polling or polling too fast the ADCs and reboot.

## Capture

Assuming 40 keys @ 1 khz, 40 khz would be enough.

10 channels is the strict minimum to have 40 keys and a simple layout.

## Protocol: full frame

Assuming all keys values are sent at each frames: 40 keys x 12 bits = 480 bits.

A 18 Mhz bus, can achieve 37.5K frames per seconds.

A .5 Mhz but should be enough.

## Switches

Relevant characteristics:

*   Circuit: I need a single pole four throw (SP4T) so that I can turn 10 processor channels into 40 inputs. 
*   On resistance (Ron) is typically around 100Ω and is very small compared to the maximum impedence input (Rain). The input leakage current (Iikg) is 3 μA.
*   Propagation delay: 320 ns (n is 10^-9) is small compared to the sampling time. It might worth initaiting the switch just after the sampling is achieved.
*   The voltage suppliy should be in the same ballpark as the processor that is to say 2.4 V

<table><tbody><tr><td>&nbsp;</td><td><a href="https://jlcpcb.com/partdetail/Nexperia-74HC4052PW118/C5646">74HC4052PW,118</a></td><td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>Price</td><td>.13 € stock 5K</td><td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>Bandwidth</td><td>180 MHz</td><td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>Supply voltage</td><td>2-10 V</td><td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>Ron</td><td>80Ω</td><td>&nbsp;</td><td>&nbsp;</td></tr></tbody></table>

<table><tbody><tr><td><a href="https://jlcpcb.com/partdetail/TexasInstruments-CD4052BM96/C6521">CD4052BM96</a></td><td>$0.1683</td><td>2 240Ω Single Pole Four Throw (SP4T) SOIC-16 Analog Switches, Multiplexers ROHS</td></tr></tbody></table>

## SAI (Serial Audio Interface)

We can create signal with [PCM5102](https://jlcpcb.com/partdetail/TexasInstruments-PCM5100APW/C544856)  (.89EUR) and a small class D amplifier such as [PAM8403](https://jlcpcb.com/partdetail/Slkor_SLKORMICRO_Elec-PAM8403/C5122557) (.23EUR).

Programming and debuging:

I will use [stlink](https://fr.aliexpress.com/item/1005009146713824.html).

## Wire to board connectors

The mother board is connected to the wings with flexible cables with connectors that lock in place. The cable makes possible to addapt to intrumenet body layout change, or card substitution.

Length: 10 cm shoult be enough for dismounting convenience.

Wire grading: 26AWG should be enough for 3V low intensity.

**Dupont connectors:** Nice for to tinkers, and addapts well to breadboards because of the 2.56 mm spacing between pins, but they do not lock in place.

JST PH2.0 with [Câble de borne à connecteur femelle PH2.0, fil JST, tête double, 10cm](https://fr.aliexpress.com/item/1005004113247279.html)

JST XG2.54

JLCPCB Connectors:  \[[wire to board](https://jlcpcb.com/parts/2nd/Connectors/Housings_(Wire_To_Board__Wire_To_Wire_)_2131)\]

## Switches

Hall effect switch

<table><tbody><tr><td>&nbsp;</td><td><a href="https://www.gateron.com/products/gateron-full-pom-low-profile-magnetic-jade-pro-switch-set">Gateron Low Profile Magnetic Jade Pro</a></td><td><a href="https://www.gateron.com/products/gateron-low-profile-magnetic-jade-switch?VariantsId=10872">Gateron Magnetic Jade&nbsp;</a></td><td><a href="https://wooting.io/product/lekker-switch-linear60"><u>lekker-switch-linear60</u></a></td></tr><tr><td>Price</td><td>$56.00 / 70 = .8</td><td>$56.00 / 70 = .8</td><td>&nbsp;</td></tr><tr><td>&nbsp;</td><td>$72.00 / 90 = .8</td><td>$72.00 / 90 = .8</td><td>&nbsp;</td></tr><tr><td>Dimensions</td><td>14.6mm x 14.6mm</td><td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>Total travel</td><td>3.5±0.2mm</td><td>3.5±0.2mm</td><td>4mm</td></tr><tr><td>Force</td><td>40-60gf</td><td>30-50gf</td><td>40-60gf</td></tr><tr><td>Magnetic field</td><td>75±10Gs</td><td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>&nbsp;</td><td>680±70Gs</td><td>700±80Gs</td><td>&nbsp;</td></tr></tbody></table>

## Leds

OPSCO has a wide variety of leds format, but they are designed to work at 5V, which would require a logic shifter.

[Components lists](https://jlcpcb.com/parts/2nd/Optoelectronics/RGB_LEDs(Built-in_IC)_2115)

## Reverse voltage protection

## Power on / off

Sensitive touch power on and off

Turn on when plugged in USB

Auto turn off after 10 minutes unused

## Bluetooth pairing

Sensitife touch

## Linear hall sensor

The supply current is usually measured with no load on the output, and would require additional measurements.

The [query](https://jlcpcb.com/parts/2nd/Magnetic_Sensors/Linear_Hall_Sensors_2150) 

<table><tbody><tr><td>&nbsp;</td><td>Price</td><td>Polarity</td><td>Supply Voltage</td><td>Supply Current</td><td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>SS39ET</td><td>.16€</td><td>Bipolar</td><td>2.7V - 6.5V</td><td>6mA @5V ⚠️</td><td>&nbsp;</td><td>&nbsp;</td></tr><tr><td><a href="https://jlcpcb.com/partdetail/HUAXIN-HX6659ISOD/C2762618">HX6659ISO-D</a></td><td>.44€ ⚠️</td><td>Bipolar</td><td>2.8V - 6V</td><td>1.3 mA</td><td>&nbsp;</td><td>±800 Gs</td></tr><tr><td>DRV5056</td><td>.56€</td><td>Unipolar</td><td>3.0V - 5.5V</td><td>6mA</td><td>&nbsp;</td><td>&nbsp;</td></tr><tr><td><a href="https://jlcpcb.com/partdetail/TexasInstruments-DRV5053VAQDBZR/C962159">DRV5053</a></td><td>.25€ , .19€ (50+)</td><td>Bipolar</td><td>2.5V - 8V</td><td>2.7 mA</td><td>&nbsp;</td><td>±730 Gs in OA version ⚠️</td></tr><tr><td><a href="https://jlcpcb.com/partdetail/DiodesIncorporated-AH8502_FDC7/C842152">AH8502</a></td><td>.51€</td><td>Bipolar</td><td>&nbsp;</td><td>13uA to 1mA</td><td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>AH49E</td><td>.36€ ⚠️</td><td>Bipolar</td><td>3.0V - 6.5V</td><td>3.5 mA @ 5V</td><td>&nbsp;</td><td>±650 (min) ±1000 (max) ⚠️</td></tr><tr><td><a href="https://jlcpcb.com/partdetail/GoChip_Elec_Tech_Shanghai-GH39FKSW/C266230">GH39FKSW</a></td><td>.11 €</td><td>Bipolar</td><td>3.0V - 6.5V</td><td>6 mA ⚠️</td><td>&nbsp;</td><td>±650 (min) ±1000 (max)</td></tr><tr><td><a href="https://jlcpcb.com/partdetail/GoChip_Elec_Tech_Shanghai-GH3562/C29781496">GH3562</a>&nbsp;</td><td>.15 €</td><td>Bipolar</td><td>1.6V - 3.6V</td><td>2.55 mA @ 3.3V</td><td>4.45 mV / Gs</td><td>&nbsp;</td></tr><tr><td><a href="https://jlcpcb.com/partdetail/MagnTek-MT9102ET/C5447698">MT9102ET</a></td><td>.26 €</td><td>Bipolar</td><td>3.0V -5.5V</td><td>6 mV @ 3.3V ⚠️</td><td>1.6 mV/Gs</td><td>±850 @3.3V</td></tr><tr><td><a href="https://jlcpcb.com/partdetail/Hallwee-HAL403SO/C42387470">HAL403SO ✅</a></td><td>.075 €</td><td>Bipolar</td><td>1.6V - 3.6V</td><td>1.4 mA</td><td>&nbsp;</td><td>±500Gs ⚠️</td></tr><tr><td><a href="https://jlcpcb.com/partdetail/Hallwee-HAL9303SW/C19089526">HAL9303SW</a>&nbsp;</td><td>.20 €, .16 € (50+)</td><td>Bipolar</td><td>1.6V - 3.6V</td><td>1.4 mA</td><td>4.7 mV/Gs ⚠️</td><td>±650 ±1000</td></tr><tr><td><a href="https://jlcpcb.com/partdetail/TOSHIBA-TCS40DLRLF/C146312">TCS40DLR,LF</a></td><td>0.06€</td><td><strong>Digital ❌</strong></td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td></tr></tbody></table>

In the worst case with [HAL403SO](https://jlcpcb.com/partdetail/Hallwee-HAL403SO/C42387470) and [Gateron Low Profile Magnetic Jade Pro](https://www.gateron.com/products/gateron-full-pom-low-profile-magnetic-jade-pro-switch-set), the magnetic field with be (680 + 70) Gs \* 4.1 mV/Gs = 3.0 V, which is bad because it is bipolar and has only 3.3V / 2 of available amplitude

SS39ET/SS49E/SS59ET

*   supply curent 6mA to 10 mA

## Battery

<table><tbody><tr><td>Model</td><td>Price</td><td>Capacity</td><td>Nominal</td><td>Discharged</td><td>Standard charging current</td><td>Cycles</td><td>&nbsp;</td></tr><tr><td>Cylindrical cells</td><td>29.95$</td><td>10 Ah</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td></tr><tr><td><a href="https://www.adafruit.com/product/353">Cylindrical cell</a></td><td>9.95$</td><td>2.2 Ah</td><td>3.7V</td><td>2.5V</td><td>.2 C5A 8h @4.2V</td><td>&gt;300</td><td>Overdischarge protection 2.5V</td></tr><tr><td><a href="https://www.adafruit.com/product/258">Flat pak</a></td><td>9.95$</td><td>1.2 Ah</td><td>3.7V</td><td>3.0V</td><td>.2 CSA @4.2V</td><td>&gt;300</td><td><a href="https://www.ablic.com/en/doc/datasheet/battery_protection/S8261_E.pdf">S-8261</a> discharge protection 2.5V</td></tr><tr><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td></tr><tr><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td><td>&nbsp;</td></tr></tbody></table>

<table><tbody><tr><td>&nbsp;</td><td><a href="https://fr.rs-online.com/web/p/piles-rechargeables-taille-speciale/1449405?gb=s">RS PRO 304-24-384</a></td><td><a href="https://fr.rs-online.com/web/p/piles-rechargeables-taille-speciale/1251266?gb=s">RS PRO 304-24-383</a></td></tr><tr><td>Price TTC</td><td>17,58&nbsp;€</td><td>28,46&nbsp;€</td></tr><tr><td>Volt</td><td>3.7V</td><td>3.7V</td></tr><tr><td>Capacity</td><td>1.8Ah</td><td>2.0Ah</td></tr></tbody></table>

## Power regulator

Adafruit Frather uses RT9080 voltage regulators uses 10nA when EN pin is down, and uses 2μA when idle.

*   Input voltage: 1.2V to 5.5V
*   Output load: 600mA

Senspr