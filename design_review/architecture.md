# Architecture

## Motherboard MCU Selection

With wireless dropped (see [requirements.md](requirements.md)), the motherboard only needs:

- USB-FS device controller (USB-MIDI),
- Two independent SPI masters — one full-duplex bus per wing, each with its own NSS,
- 2× ADC channels for the spring-blade push/pull hall sensors (see [hardware.md](hardware.md) §"Push/Pull Sensor"),
- 2× ADC channels for the expression and sustain pedals,
- ~20 GPIOs for status LEDs, presence-sense contacts, function buttons, and the hall-bank power gate.

The **STM32G474CBT6** (LQFP48) is the choice — and the same part runs on both wings:

- **Single SKU across all three boards.** One toolchain, one flash procedure, one decoupling pattern (documented in [hardware.md](hardware.md)) for motherboard and both wings. Consolidating to a single STM32 part across the three boards removes a class of bring-up and supply risk.
- **Crystal-less USB-FS device.** G474 uses HSI48 with the Clock Recovery System (CRS), so USB enumerates without an external HSE crystal — fewer parts, smaller layout, no crystal-load-cap tuning. There is no external PHY and no companion MCU.
- **Two independent SPI peripherals.** SPI1 and SPI2 each serve one wing; no shared-bus arbitration, no NSS-tristate gymnastics — see "Internal Bus" below.
- **Headroom.** Cortex-M4F at 170 MHz with 128 KB flash / 128 KB RAM is comfortably over-spec for the USB-MIDI workload, leaving margin for future on-device processing (note-on smoothing, gesture recognition, expression curves).
- **No RF, no antenna, no module certification.** Wireless is out (see [requirements.md](requirements.md)), so no module premium is paid.

Trade-off: STM32G474 is an *extended* part on JLCPCB rather than basic, so it carries a small per-board surcharge — accepted in exchange for the single-SKU simplification across all three boards. A G431 would be basic-part and slightly cheaper, but at the cost of running a different MCU than the wings.

## Internal Bus: Motherboard to Wing Boards

Each wing board connects to the motherboard over its **own dedicated full-duplex SPI bus** — one SPI peripheral per wing on the motherboard (SPI1 → left wing, SPI2 → right wing). The two buses are electrically independent: no shared SCK/MOSI/MISO, no bus arbitration. The connector (2×5, 2.54 mm IDC) carries 10 wires:

| Pin | Net | Direction |
|---|---|---|
| 1 | VCC (3.3 V from motherboard) | main → wing |
| 2 | SPI_MOSI | main → wing |
| 3 | GND | — |
| 4 | SPI_MISO | wing → main |
| 5 | GND | — |
| 6 | SPI_SCK | main → wing |
| 7 | GND | — |
| 8 | SPI_NSS | main → wing |
| 9 | NRST | main → wing (open-drain via BAT54C) |
| 10 | READY | wing → main |

Signals are interleaved with GND returns (pins 3, 5, 7) for signal integrity on the ribbon cable.

- **NRST** is driven open-drain from the motherboard through a BAT54C to 3.3 V; the wing's internal NRST pull-up provides idle-high.
- **READY** is driven actively high by the wing once its MCU has initialised; the motherboard configures its READY pin as input with internal pull-down so that, if a wing is absent or still booting, the line resolves to "not ready" without anyone sourcing current into the wing.

Each SPI transaction carries a wing identification code, a complete raw keyboard state frame, and a checksum. The wing ID encodes the keyboard layout (e.g. Rheinische Lage, Einheitsgriff), so the motherboard applies the correct key-to-note mapping at runtime — the same firmware supports multiple layouts without recompilation.

Motherboard is master, wing is slave. The master initiates each cycle:

- Master → slave: `1 × 16 bits` — request ID (`0x0001`)
- Slave → master: `1 × 16 bits` — board identifier + `40 × 16 bits` — sample values

40 sample slots match the largest wing (WingRight, 5 × 74HC4052 × 8 = 40 channels; WingLeft uses 32 and pads the rest). 1 kHz polling → **672 kbit/s per bus**, well under the STM32G474 SPI ceiling. Because each wing has its own peripheral, the two transactions can run concurrently rather than serialised.

## System Power

The motherboard is bus-powered from the USB VBUS pin (5 V, ≤ 500 mA at the instrument — well within the USB 2.0 default budget). There is no internal battery, no charger IC, no ship mode, no charge mode: when the cable is plugged the instrument runs, when it is unplugged the instrument is off. Auto-power-off and battery-preservation logic are no longer relevant.

| State | Power source | Boards powered | MCU activity |
|---|---|---|---|
| **Powered** | USB VBUS | All | Full operation |
| **Unpowered** | — | None | Off |

The on-board 3.3 V LDO is fed directly from VBUS through the USB ESD/OVP front-end (see [hardware.md](hardware.md)). The USB connector is **Type-B** (USB1, right-angle SMD) — chosen for mechanical robustness and cable retention on stage; see [hardware.md](hardware.md) §"USB Connector Choice". The wings are powered directly from the motherboard's 3.3 V rail through their respective connectors; there is no per-wing power switch, no gated wing rail, and no SPI back-feed buffer. With the battery removed there is no runtime motivation to cut wings independently of the motherboard — if VBUS is present the whole instrument is powered, if VBUS is gone everything is off.

## Programming Port

Each board (motherboard and both wing boards) exposes **14 bare pogo-pin landing pads** laid out per the **STDC14** pinout — no connector body on the BOM. The mating tool is a **Tag-Connect TC2070-IDC-050** spring-loaded pogo jig pressed against the pads during programming or debug. The jig is a workshop tool, not a BOM component.

STDC14 carries the full debug interface: SWD (SWDIO/SWCLK), SWO trace, NRST, UART VCP (TX/RX), target VCC reference, and several GND returns — everything needed for flashing, halting, real-time tracing, and serial console without falling back to test points.

**Why STDC14, not the legacy 20-pin Cortex headers.** STDC14 is ST's modern compact debug standard and is the native footprint of the STLINK-V3 family. It packs the same SWD + SWO + VCP signals into a much smaller area, which is the deciding factor for these boards.

**Why ST-Link, not J-Link.** Segger's affordable J-Link variants (EDU, EDU Mini) are licensed for **non-commercial use only**; the cheapest commercially-licensed J-Link is roughly an order of magnitude more expensive. ST-Link probes — in particular the STLINK-V3 series, which exposes STDC14 directly — are inexpensive, commercially licensable out of the box, and natively supported by the STM32 toolchain used on every board in this project.

## ESD Protection

All external-facing ports (USB, internal bus, programming ports, pedal inputs) are protected with a TVS diode array placed close to the connector, before any MCU pin or active device.

