# Architecture

## Motherboard MCU Selection

The motherboard uses a pre-certified STM32 wireless module to avoid custom antenna design and RF certification. The **STM32WB5MMGH6TR** is selected as an interim choice: it covers BLE-MIDI and USB-MIDI today and shares the STM32 ecosystem with the STM32WBA6M that is expected to replace it. A motherboard PCB redesign should be expected at that migration — BLE-Audio (A2DP) support then becomes viable, which is why audio output is deferred for v1.

- **STM32WBA5M** — not eligible: lacks a USB front-end; adding a companion MCU just for USB-MIDI is too disruptive.
- **STM32WBA6M** — not yet released; general availability expected end of 2026.

## Internal Bus: Motherboard to Wing Boards

Each wing board connects to the motherboard over a full-duplex SPI bus (motherboard master, one NSS per wing). The connector carries 8 wires:

- **GND** — common ground
- **VCC** — 3.3 V power supply from motherboard
- **RESET** — driven by motherboard; low resets the wing.
- **MOSI** — SPI data from motherboard to wing. Reserved. Unused.
- **MISO** — SPI data from wing to motherboard
- **SCLK** — SPI clock driven by motherboard
- **NSS** — SPI chip select, one per wing, active low
- **READY** — asserted by wing once its MCU has initialized; motherboard holds SPI idle until this line is active, preventing backfeed current into an unpowered or booting wing

Each transaction carries a wing identification code, a complete raw keyboard state frame, and a checksum. The wing ID encodes the keyboard layout (e.g. Rheinische Lage, Einheitsgriff), so the motherboard applies the correct key-to-note mapping at runtime — the same firmware supports multiple layouts without recompilation.

Motherboard is master, wing is slave. The master initiates each cycle:

- Master → slave: `1 × 16 bits` — request ID (`0x0001`)
- Slave → master: `1 × 16 bits` — board identifier + `40 × 16 bits` — sample values

40 sample slots match the largest wing (WingRight, 5 × 74HC4052 × 8 = 40 channels; WingLeft uses 32 and pads the rest). 1 kHz polling → **672 kbit/s**, well under the 4 Mbit/s slave-mode ceiling of the STM32F103 SPI.

## System Power States

The system has four power states:

| State | Power source | Boards powered | MCU activity |
|---|---|---|---|
| **On battery** | Battery | All | Full operation |
| **On charging** | USB | All | Full operation |
| **Charge mode** | USB | All | Charging indicator only |
| **Ship mode** | None | None | Off |

**On battery** and **on charging** are the normal operating states. The instrument is fully functional in both; the difference is the power source.

**Charge mode** is entered when USB is connected but the user has not powered the instrument on. All boards are powered (wing boards included), but the motherboard MCU only drives the charging status indicator. Powering the wings in this state is a simplification justified by their low power consumption relative to the USB supply current budget.

**Ship mode** cuts all system power. It is used for storage and transport to prevent battery drain, and is also the target state for the auto-power-off inactivity timer. The device cannot be used or charged while in ship mode; connecting USB (or pulsing BQ25895 QON) exits ship mode and enters charge mode.

## Programming Port

Each board (motherboard and both wing boards) exposes **14 pogo-pin pads laid out per the STDC14 pinout** for firmware flashing and debugging — no through-hole header, no shroud, no board area spent on a connector that is touched only at the factory and during occasional rework. A spring-loaded pogo jig matching the STDC14 footprint is pressed against the pads when needed.

STDC14 carries the full debug interface: SWD (SWDIO/SWCLK), SWO trace, NRST, UART VCP (TX/RX), target VCC reference, and several GND returns — everything needed for flashing, halting, real-time tracing, and serial console without falling back to test points.

**Why STDC14, not the legacy 20-pin Cortex headers.** STDC14 is ST's modern compact debug standard and is the native footprint of the STLINK-V3 family. It packs the same SWD + SWO + VCP signals into a much smaller area, which is the deciding factor for these boards.

**Why ST-Link, not J-Link.** Segger's affordable J-Link variants (EDU, EDU Mini) are licensed for **non-commercial use only**; the cheapest commercially-licensed J-Link is roughly an order of magnitude more expensive. ST-Link probes — in particular the STLINK-V3 series, which exposes STDC14 directly — are inexpensive, commercially licensable out of the box, and natively supported by the STM32 toolchain used on every board in this project.

## ESD Protection

All external-facing ports (USB, internal bus, programming ports, pedal inputs) are protected with a TVS diode array placed close to the connector, before any MCU pin or active device.

