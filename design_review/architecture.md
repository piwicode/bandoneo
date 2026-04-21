# Architecture

## Motherboard MCU Selection

The motherboard uses a pre-certified STM32 wireless module to avoid custom antenna design and RF certification. The STM32WB5MMGH6TR is selected as interim: it covers BLE-MIDI and USB-MIDI today, and shares the STM32 ecosystem with the STM32WBA6 that will replace it. Motherboard PCB redesign should be expected at migration.

STM32WBA5M is not eligible because it lacks USB.
STM32WBA6M is not  release expected end of 2026.

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

Wing is slave, motherboard is master.
Master initiate the cycle: 
  1x 16 bits: request id = 0x0001
Slave repsonds with:
  1 x 16 bits: board identifier = 0x0001
40 x 16 bits: sample value

1Khz sampling rate requires 672 kbit/s < 4000 kbit/s max slave bus speed.

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

**Ship mode** cuts all system power. It is used for storage and transport to prevent battery drain. The device cannot be used or charged while in ship mode; connecting USB exits ship mode and enters charge mode.

## Programming Port

Each board (motherboard and both wing boards) exposes a dedicated 8-wire programming port for firmware flashing and debugging. The port combines an SWD debug interface, a trace output line, a UART console, and board control:

- **GND** — common ground
- **VCC** — reference voltage (board-powered; used by the probe for level sensing, not to power the board)
- **SWDIO** — SWD bidirectional data
- **SWCLK** — SWD clock
- **SWO** — single-wire trace output (ITM/ETM); optional but available for real-time tracing
- **RESET** — forces MCU reset when asserted low; allows the probe to halt the CPU before it executes
- **TX** — UART transmit from MCU; used for log output and debug console
- **RX** — UART receive into MCU; used for debug console input

## ESD Protection

All external-facing ports (USB, internal bus, programming ports, pedal inputs) are protected with a TVS diode array placed close to the connector, before any MCU pin or active device.

