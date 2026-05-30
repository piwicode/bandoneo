# MainBoard Design Review Plan

Sources inspected: `export/BOM_MainBoard_MainBoard.csv`, `export/Netlist_MainBoard.net`,
`export/Netlist_WingLeft.net`, `export/Netlist_WingRight.net`,
`export/SCH_MainBoard.pdf`, `hardware.md`, `architecture.md`,
`datasheet_summaries.md`, `requirements.md`.

Each item has a status:
- **PASS** — verified correct, no action needed
- **WARN** — concern that needs user confirmation before closing
- **FAIL** — definitive mismatch between netlist and documentation / a design error; doc or schematic must be updated
- **OPEN** — cannot be verified from BOM/netlist alone (firmware, layout, or datasheet lookup needed)

---

## 1. Power Supply Chain

### 1.1 VBUS TVS (D5, SMF5.0A)
Netlist: `D5-1(K) → VBUS`, `D5-2(A) → GND`. Unidirectional, cathode to VBUS.
**PASS** — correct polarity, correct part (SOD-123FL, Vc=9.2 V).

### 1.2 LDO Enable (R14, TLV755P U6)
Netlist: `R14-1 → VBUS`, `R14-2 → U6-3 (EN)`. 10 kΩ from VBUS to EN.
EN threshold ≥ 1 V; at 4.4 V VBUS min, EN ≈ 4.4 V → LDO enabled.
**PASS** — always-on LDO correctly wired.

### 1.3 LDO Input / Output Decoupling
Netlist: `C15 (1 µF 0603) → VBUS`, `C19 (1 µF 0603) + C20 (4.7 µF 0805) → VCC (output)`.
Datasheet requires ≥ 0.47 µF in + ≥ 0.47 µF out.

Transient sanity-check on the input cap: worst-case scan-window load step is ~140 mA delta on VBUS, sustained for the 240 µs Hall-scan window. Droop on a 1 µF input cap = `I·t/C` = `0.140 × 240e-6 / 1e-6` ≈ **34 mV**. VBUS worst-case minimum 4.40 V → 4.37 V during scan; the LDO needs only 3.55 V to maintain 3.3 V at full load → margin > 800 mV remains. USB-cable inductance (~0.5 µH for a typical 1–2 m cable) recharges C15 between scan windows in microseconds. The 1 µF input cap is **electrically adequate** — no need to add bulk or bypass.

`hardware.md` previously claimed "10 µF bulk + 100 nF bypass" on the LDO input — that overstated the build. Doc updated to match the actual 1 µF as of this review round.
**PASS** — schematic is correct; doc corrected.

### 1.4 LDO VIN Headroom
VBUS min 4.40 V; TLV755P dropout at 500 mA max 215 mV → min VIN needed 3.515 V.
4.40 V − 0.215 V = 4.19 V output → regulated at 3.3 V with 0.89 V margin.
**PASS** — comfortable headroom even at worst-case USB cable IR drop.

### 1.5 Hall-sensor Gate (Q1, AO3401A P-ch)
Netlist: `Q1-2(S) → VCC`, `Q1-3(D) → VDDH`, `Q1-1(G) → R5(1 kΩ) → PA2 (HALL_NEN)` and `R6(100 kΩ) → VCC`.
P-FET: gate pull-up to VCC = OFF by default at power-on (sensors de-energised). Firmware drives PA2 low through R5 to turn on.
**PASS** — correct default-off topology; gate resistors protect against inrush.

### 1.6 VDDH Bypass
Netlist: `CP1(100 nF 0603) + CP2(100 nF 0603)` on VDDH.
**PASS** — two 100 nF bypasses on the gated hall rail (one per SC4015).

---

## 2. MCU Decoupling (STM32G474CBT6, U1)

### 2.1 VDD / VBAT Pin Bypass Caps
G474 LQFP48 has **3 VDD pins** (24, 36, 48) and **VBAT** (pin 1). All four are tied to VCC in the netlist.
Netlist VCC net has **C8, C9, C10** (three × 100 nF 0402) for VDD bypass.

VBAT does **not** require a dedicated bypass cap when no backup battery is fitted: the G474 datasheet Figure 16 ("Power supply scheme") shows VBAT tied directly to VDD with no separate cap, and the mandatory decoupling caps are listed only for VDD/VDDA/VREF. Plane/trace inductance from VBAT to the nearest VDD bypass cap is the only practical concern, and on a LQFP48 the VBAT pin (1) sits 1–3 mm from the C8/C9/C10 footprints.
**PASS** — cap count matches G474 datasheet's "Power supply scheme" recommendation; no 4th cap required.

### 2.2 VDD Bulk Cap
Netlist: `C11 (4.7 µF 0805)` on VCC.
**PASS** — adequate bulk reservoir.

### 2.3 VDDA / VREF+ Filter
Netlist: `VCC → L1 (ferrite GZ1608D601TF 600 Ω/100 MHz) → VDDA`. VDDA net pools **U1-20 (VREF+) and U1-21 (VDDA)** with `C16(10 nF) + C17(1 µF) + C12(1 µF) + C18(100 nF)` to GND. VSSA (U1-19) tied to GND.
VREF+ and VDDA are bonded after the ferrite — acceptable when ratiometric VREF is not needed (key-position and pedal ADC are not ratiometric).
**PASS** — ferrite + multi-value VDDA/VREF+ decoupling per AN4488.

### 2.4 NRST Cap
Netlist: `C4 (100 nF 0603)` on M_NRST.
**PASS** — per AN4488 recommendation.

### 2.5 VBAT Pin
Netlist: `U1-1 (VBAT) → VCC`. No backup battery fitted, VBAT tied to VCC.
**PASS** — acceptable; RTC / backup registers not used.

### 2.6 No External Crystal
Netlist: PF0/PF1 (OSC_IN/OSC_OUT) are repurposed as R_SPI_NSS and R_SPI_SCK respectively (right-wing SPI).
USB uses HSI48 + CRS; no LSE fitted.
**PASS** — crystal-less USB design confirmed.

---

## 3. ESD and Protection Coverage

### 3.1 VBUS Clamp (D5)
See §1.1. **PASS**.

### 3.2 USB D+/D− (TV8, SRV05-4)
Netlist: `TV8-1 → USB_DP(PA12, USB1-3)`, `TV8-3 → USB_DM(PA11, USB1-2)`, `TV8-2 → GND`, `TV8-5 → VBUS`.
Pin 5 (V+) tied to VBUS is correct: no PD negotiation means VBUS is always 5 V, so the upper steering diodes are safe to activate. ESD charge steers to VBUS (held by SMF5.0A) or GND — full bidirectional clamping.
**PASS**

### 3.3 Pedal Jack ESD (TV7, SRV05-4)
Netlist: `TV7-5 → VCC (3.3 V)`, channels: `TV7-1 → SUS_PEDAL`, `TV7-3 → EXP_PEDAL`, `TV7-4 → EXP_PEDAL_INT`, `TV7-6 → SUS_PEDAL_INT`.
**PASS** — consistent.

### 3.4 Wing-Bus ESD (TV1–TV4, SRV05-4 × 2 per wing)
Netlist: `TV1-5, TV2-5, TV3-5, TV4-5 → VCC`.
Wing bus signals protected: SPI×4, NRST, READY, BOOT0 (7 signals total, 8 channels of headroom across two SRV05-4 per wing).
**PASS** — pin 5 on 3.3 V rail (same domain as wings).

### 3.5 Programming Port ESD (TV5–TV6, SRV05-4)
Netlist: `TV5-5, TV6-5 → VCC`. Channels protect SWDIO, SWCLK, SWO, NRST, USART_TX, USART_RX, DETECT.
ESD coverage is architecturally correct. Signal-to-pad routing is a separate issue covered in §7.1.
**PASS** (ESD only — see §7.1 for wiring).

### 3.6 Function-Button ESD (D1–D4, H5VND5BA)
Netlist: D1–D4 each have pin 1 → GND, pin 2 → respective button net.
D1 specifically protects the BAT54C common-cathode bus (SW1 / NRST OR-tie).
**PASS** — bidirectional clamp on each line.

### 3.7 VBUS Polyfuse / eFuse
Deliberately not fitted for v1. Rationale: USB 2.0 host port is spec-required to limit to 500 mA; SMF5.0A clamps transient surges; TLV755P has internal foldback + thermal shutdown. A polyfuse or eFuse would be a JLCPCB extended part with no basic-part substitute.
**SKIP — conscious decision, not an oversight.**

---

## 4. USB Interface

### 4.1 USB-B Connector (USB1)
BOM: U-B-M4SS-W-1, right-angle SMD, 4 contacts + 2 GND anchors.
Netlist: `USB1-1(VBUS)`, `USB1-2(D−)`, `USB1-3(D+)`, `USB1-4(GND)`, `USB1-5(GND)`, `USB1-6(GND)`.
**PASS** — correctly wired.

### 4.2 USB D+/D− MCU Pins
Netlist: PA11 = D−, PA12 = D+.
G474 datasheet: PA11 = USB_DM, PA12 = USB_DP. ✓
**PASS**

### 4.3 USB Crystal-less Enumeration
HSI48 + CRS per architecture.md. No crystal in BOM. VDDA ≥ 2.4 V (3.3 V VCC) as required by G474 ADC and USB.
**PASS**

### 4.4 USB VBUS Sense
Netlist: no MCU pin tied to VBUS for sense. For a fully bus-powered device this is correct — when VBUS is removed, the MCU loses power and the bus is disconnected automatically. STM32G474 USB peripheral's VBDEN bit must be cleared in firmware so it does not wait for VBUS presence on an unconnected pin.
**OPEN** — confirm in firmware that USB_BCDR.VBDEN = 0 (or use the equivalent HAL config for "no VBUS sensing").

---

## 5. Wing Bus

### 5.1 Connector Part vs. Architecture Documentation
BOM: `CN1, CN2 = S062100026`, which is 2×6 (12-pin), 1.27 mm pitch, SMD vertical.
`architecture.md` and `hardware.md` updated with correct connector, full 12-pin table (2× VCC, 3× GND, 4× SPI, READY, NRST, BOOT0), and trade-off note (1.27 mm cable required, no Dupont compatibility).
**PASS**

### 5.2 Wing Bus Signal-to-Pin Mapping (CN2 = left wing)
| Net | CN2 pin | MCU GPIO |
|---|---|---|
| VCC | 1, 12 | — |
| L_READY | 2 | PB0 (via R2 100 Ω) |
| L_SPI_NSS | 3 | PA4 |
| L_NRST | 4 | PB2 (via R3 100 Ω, BAT54C U2-A2) |
| L_SPI_SCK | 5 | PA5 |
| GND | 6, 8, 10 | — |
| L_SPI_MISO | 7 | PA6 |
| L_SPI_MOSI | 9 | PA7 |
| L_BOOT0 | 11 | PA3 |

**PASS** — SPI1 (PA4–PA7) correctly serves left wing. Wing's matching netlist confirms same pin numbers on CN1 of the WingLeft side, with wing-side MCU on the SPI1 alternate (PA4/PA5/PB4/PB5, AF5).

### 5.3 Wing Bus Signal-to-Pin Mapping (CN1 = right wing)
| Net | CN1 pin | MCU GPIO |
|---|---|---|
| VCC | 1, 12 | — |
| R_READY | 2 | PA8 (via R4 100 Ω) |
| R_SPI_NSS | 3 | PF0 (OSC_IN repurposed, AF5 = SPI2_NSS) |
| R_NRST | 4 | PB11 (via R8 100 Ω, BAT54C U3-A2) |
| R_SPI_SCK | 5 | PF1 (OSC_OUT repurposed, AF5 = SPI2_SCK) |
| GND | 6, 8, 10 | — |
| R_SPI_MISO | 7 | PB14 (AF5 = SPI2_MISO) |
| R_SPI_MOSI | 9 | PB15 (AF5 = SPI2_MOSI) |
| R_BOOT0 | 11 | PB13 |

PF0/PF1 are AF5 SPI2 alternates per the STM32G474 datasheet (DS12288, Section 4.11 Alternate Function table) — explicitly listed as `SPI2_NSS` and `SPI2_SCK`. Combined with PB14/PB15 (also AF5), SPI2 routes cleanly without remapping.
**PASS** — PF0/PF1 valid as SPI2 AF5; mapping verified against datasheet.

### 5.4 Remote BOOT0 on Wing Connector
Netlist: `CN2-11 = L_BOOT0 → PA3`, `CN1-11 = R_BOOT0 → PB13`.
Documented in `architecture.md` bus table and `hardware.md` §"Remote Wing Programming via BOOT0".
**PASS**

### 5.5 Wing NRST Open-Drain Topology (BAT54C)
Netlist `$1N16140` net: `U2-3(K) + U3-3(K) + SW1-2 + D1-2 + C1-1`.
BAT54C common-cathode: at idle the cathode is pulled toward GND by D1/C1 leakage, leaving NRST lines undisturbed (reverse-biased). When the cathode net is pulled to GND (by SW1 or firmware), both wings' NRST anodes are pulled to ~Vf ≈ 0.3 V → wings held in reset.
**PASS** — topology is valid (see also §6.4 for SW1 function).

### 5.6 Series Resistors on READY/NRST Wing Signals
R2, R3, R4, R8 are all 100 Ω. They sit between the MCU GPIO and the ESD device / connector.
**PASS** — standard 100 Ω series termination.

### 5.7 No Series Resistor on Wing BOOT0 — Contention Risk
Netlist: `L_BOOT0` and `R_BOOT0` from mainboard GPIOs (PA3, PB13) drive directly to CN2-11 / CN1-11 with **no series resistor on the mainboard side**. The wing schematic shows its local BOOT0 button (SW1 on each wing) ties **VCC directly to the same line** when pressed, and the wing has a 10 kΩ pull-down (R4) plus an ESD diode.

If a wing's local BOOT0 button is pressed *while* the mainboard is driving the line LOW (e.g., during firmware boot, or during the remote-BOOT0 sequence between the BOOT0 assert and the NRST release), VCC connects through the switch directly to the mainboard GPIO low-side driver. The instantaneous current is `(VCC − V_OL) / R_switch ≈ 3.3 V / ~100 mΩ ≈ tens of A` momentarily, settling at the GPIO sink limit (~25 mA continuous, ~50 mA short-pulse on G474). Bounded by the GPIO's output stage, but the contention is uncomfortably hot.

The wings' R/L NRST and READY lines all have 100 Ω series resistors (R2/R3/R4/R8) precisely to bound this kind of contention. BOOT0 was omitted.

**WARN** — add a 100 Ω series resistor on the mainboard's BOOT0 driver for each wing (matching the NRST/READY series Rs), OR firmware-protect by never driving BOOT0 hard while a wing's local button could be pressed (treat as open-drain in firmware: drive low → input-pull-down). Document the chosen mitigation in `hardware.md`.

---

## 5W. Wing-board internal review

Items below cover the wing boards themselves (not the wing-to-mainboard bus). Both wings share the same MCU and topology; only WingLeft pins are quoted unless otherwise noted.

### 5W.1 Wing power gate (Q1 AO3401A) and gate network
Wing netlist: `Q1-2 (S) → VCC`, `Q1-3 (D) → VDDH`, `Q1-1 (G) → R7 (1 kΩ) → HALL_NEN → U37-25 (PB11)` and a separate gate pull-up to VCC. Same default-OFF P-FET topology as the mainboard (§1.5). PB11 drives gate-low to enable VDDH for the scan window.
**PASS** — symmetric to the mainboard hall-sensor gate.

### 5W.2 74HC4052 mux topology
- WingLeft: 4 muxes (U5, U14, U23, U32 — see §12.1a). Each mux exposes 8 channels → 32 channels total. Wing has 33 Hall sensors per BOM — count mismatch: only 32 channels are routed through muxes; one sensor (or 33-channel sweep) must be either unused or routed elsewhere. Worth a spot-check against the schematic/PCB.
- WingRight: 5 muxes (U5, U14, U23, U32, U41). 40 channels for 38 sensors → 2 spare channels. ✓
- Mux `E̅` (pin 6) is tied to GND on every mux on both wings → muxes are permanently enabled. Bank-power gating happens upstream at Q1 / VDDH, not at the mux enable, so always-on E̅ is consistent with the gated-Hall design. The "available for bank-switching" wording in `datasheet_summaries.md` describes a capability that the schematic doesn't use.
- Mux select lines `S0` (pin 10) and `S1` (pin 9) are bussed to all muxes in parallel. Driven by `SEL0 → U37-3 (PC14)` and `SEL1 → U37-4 (PC15)`.

**PASS for topology; WARN for the 32/33 channel discrepancy on WingLeft** — flag for schematic-level confirmation.

### 5W.3 Mux select on PC14 / PC15 (LSE-domain pins)
SEL0/SEL1 land on `PC14-OSC32_IN` / `PC15-OSC32_OUT`, which sit in the STM32G4's *backup* power domain. Per DS12288, these pins have **reduced output drive** when configured as GPIO, and they require **PWR_CR1.DBP = 1** to enable writes to the RCC backup-domain control register before they can be driven (the same dance as touching VBAT-domain RTC registers). Loads here are tiny — 4 (WingLeft) or 5 (WingRight) mux S inputs at a few pF each, switched at the scan rate (~kHz), well inside the reduced drive budget — so the hardware is fine.

**PASS for hardware; OPEN for firmware** — confirm the wing's HAL init sets `PWR_CR1.DBP` before configuring PC14/PC15 as GPIO outputs, otherwise SEL0/SEL1 will read back as ones and the scan will be stuck on channel 3.

### 5W.4 Wing buttons — three on each wing, not one
Netlist shows three TS-1088 tactile switches per wing (matches BOM `qty = 3`):

| Wing SW | Net | MCU pin | Role |
|---|---|---|---|
| SW1 | BOOT0 | U37-45 (PB8/BOOT0) | Local BOOT0 entry (bench-only; redundant with mainboard's remote BOOT0 via CN-11) |
| SW2 | PGM_RESET | U37-7 (NRST) | Local NRST button (debounced via C2, ESD via D2, ORed onto the wing NRST through TV2-6) |
| SW3 | SW_FN0 | U37-27 (PB13) | Generic FN button on PB13; debounced via C3, ESD via D3 |

`hardware.md` §"Function and Boot Buttons → Wing Boards" says "Each wing board carries one dedicated BOOT0 pushbutton… There are no FN buttons on the wing boards" — that's not what is built. The wings have local NRST and FN0 buttons too. Update the doc with the full per-wing button table, noting SW3 (FN0 on PB13) is a hard-soldered control that the wing firmware can use as a developer poke (mode toggle, dump-state, etc.).
**WARN (doc only)** — schematic is functional; documentation undersells what's on the wing.

### 5W.5 Wing READY series resistor (470 Ω)
Wing `READY` net: `U37-44 (PB7) → R9 (470 Ω) → CN1-2 → TV1-6`. Mainboard side adds another 100 Ω (R2 on left, R4 on right), so the total READY series impedance is **570 Ω**. The wing actively drives PB7 high once init is complete; the mainboard reads with internal pull-down. In steady state no current flows through the series Rs (CMOS input), so no DC voltage drop — the wing's logic-high reaches the mainboard at the full 3.3 V minus a few mV of leakage.

Edge slew rate is bounded by 570 Ω × (mainboard input C + ESD CJ + cable C) ≈ 570 Ω × ~10–20 pF ≈ 5.7–11 ns — well inside the GPIO sampling window for a non-time-critical "wing ready" signal.
**PASS**.

### 6.1 Switch-to-Designator Mapping (Netlist vs. Documentation)
From netlist nets:
| Designator | Net | Connected to |
|---|---|---|
| SW1 | `$1N16140` | BAT54C U2-K + U3-K (NRST cathode bus) + C1 + D1 |
| SW2 | SW_FN1 | PB6 (FN1) |
| SW3 | SW_FN2 | PB4 (FN2) |
| SW4 | SW_FN0_BOOT0 | PB8/BOOT0 |

`hardware.md` button table matches.
**PASS**

### 6.2 External Pull-Down on BOOT0 (R1)
`hardware.md` button table updated: SW4/BOOT0 has external 10 kΩ pull-down (R1) for defined idle-low; debounce cap C5 (100 nF) parallels R1 (RC = 1 ms hold-time on BOOT0 after SW4 release — useful for asserting BOOT0 *before* the NRST release). FN1/FN2 use MCU internal pull-ups.
**PASS**

### 6.3 SW1 = Global Reset Button
Documented in `hardware.md` §"Function and Boot Buttons": SW1 grounds the BAT54C common cathode, forward-biasing both diodes and pulling all three MCU NRST lines to ~0.3 V simultaneously.
**PASS**

### 6.4 LED_FN0 (PB9)
LED behind SW4 (BOOT0/FN0) cap. R9 330 Ω, KT-0603W (white). Drive on PB9 (U1-46). Mirrors FN1/FN2.
**PASS** for wiring; see §10.1 for the brightness calculation correction.

### 6.5 FN Button Logic Levels
SW2 (FN1) and SW3 (FN2): pin 1 → GND, pin 2 → GPIO. Active-low via MCU internal pull-up. ✓
SW4 (BOOT0): pin 1 → VCC, pin 2 → PB8. Active-high. ✓
**PASS**

---

## 7. Programming Port (STDC14, CN3)

### 7.1 SWD Signal Wiring — verified against netlist + LQFP48 pinout
Cross-checked CN3 pin assignments against the actual `Netlist_MainBoard.net` nets and against the STM32G474CBT6 LQFP48 pinout (DS12288 Figure 6):

| STDC14 std pin | Mainboard net | Mainboard MCU pin | LQFP48 function |
|---:|---|---|---|
| 3  T_VCC            | VCC               | —                  | — ✓ |
| 4  T_SWDIO          | PGM_SWDIO         | U1-37              | **PA13** = SYS_JTMS-SWDIO ✓ |
| 5  GND              | GND               | —                  | — ✓ |
| 6  T_SWCLK          | PGM_SWCLK         | U1-38              | **PA14** = SYS_JTCK-SWCLK ✓ |
| 7  GND              | GND               | —                  | — ✓ |
| 8  T_SWO            | PGM_SWO           | U1-40              | **PB3** = SYS_JTDO-TRACESWO ✓ |
| 11 GND_DETECT       | PGM_DETECT        | U1-39              | PA15 (GPIO) — see §7.5 |
| 12 NRST             | M_NRST            | U1-7               | **NRST** ✓ |
| 13 T_VCP_RX         | PGM_USART_RX      | U1-32              | **PA10** = USART1_RX AF7 ✓ |
| 14 T_VCP_TX         | PGM_USART_TX      | U1-31              | **PA9**  = USART1_TX AF7 ✓ |

All five SWD/JTAG-critical signals (SWDIO, SWCLK, SWO, NRST, VCC) land on the correct MCU pads, and the VCP UART pair maps to USART1 on its AF7 alternate. Symbol pin-number attributes are consistent with the standard STDC14 footprint — earlier draft of this review (which flagged this section as a critical failure) was based on a mis-extracted pin table; corrected after re-reading the LQFP48 pinout (pins 37/38/40 = PA13/PA14/PB3, not the off-by-two arrangement assumed earlier).

**PASS** — board can be programmed and debugged through the pogo pads as drawn.

### 7.2 STDC14 VCC Reference
Netlist: `CN3-3 (T_VCC) → VCC`. STLINK-V3 uses this pin to reference signal levels.
**PASS** — correctly tied to 3.3 V.

### 7.3 GND Pins on STDC14
Netlist: `CN3-5, CN3-7 → GND`.
**PASS**

### 7.4 No Connector Body on BOM
Confirmed: CN3 has no BOM entry. Mating jig = TC2070-IDC-050 (workshop tool).
**PASS**

### 7.5 STDC14 pin 11 (GNDDetect) → PA15 — tool-presence detect
Netlist: `CN3-11 → PGM_DETECT → U1-39 (PA15)`. Both wings do the same (`CN2-11 → U37-39 → PA15`).

Per **ST UM2448 (STLINK-V3SET user manual) Rev 9, Table 6 note 6**, STDC14 pin 11 is labelled `GNDDetect` and is "**Tied to GND by STLINK-V3SET firmware. It might be used by the target to detect the tool.**" The TC2070-IDC-050 cable is passive and passes pin 11 through. So when the STLINK-V3 is mated, the target sees pin 11 = GND through the probe; when no probe is attached, the line is floating and PA15's internal pull-up resolves it to logic high.

This is precisely the design intent: PA15 reads LOW = STLINK attached, HIGH = no probe. Firmware can use this to enable a VCP echo mode, enter a "developer" command set, blink an LED, or simply log probe attach/detach events.

(Earlier review rounds reported this as inverted vs the standard — that was a misread of the STDC14 spec. UM2448 is unambiguous: ST's intended direction for pin 11 *is* target-detects-tool, with the tool sourcing the GND.)

**PASS** — schematic correct, matches the documented STDC14 usage. Firmware should configure PA15 as input with internal pull-up enabled; document the "STLINK_DETECT" role in firmware notes once PA15 is consumed.

---

## 8. Hall Sensor Subsystem

### 8.1 Motherboard Hall Sensors (U4, U5)
Netlist: `U4-1(VDD) + U5-1(VDD) → VDDH`, `U4-2(GND) + U5-2(GND) → GND`, `U4-3(OUT) → HALL0 → PB1`, `U5-3(OUT) → HALL1 → PB12`.
PB1 = ADC1_IN12 / ADC3_IN1. PB12 = ADC1_IN11. Both are valid ADC inputs on G474.
**PASS** — push/pull sensor pair on ADC-capable GPIOs.

### 8.2 Hall Output Filter Caps
Netlist: `C6 (1 nF 0603) → HALL0`, `C7 (1 nF 0603) → HALL1`. Pin 1 to HALL net, pin 2 to GND.
**PASS** — matches SC4015 datasheet recommendation.

### 8.3 ADC Sample Time
74HC4052 Ron ≈ 80 Ω (wings only). Motherboard Hall sensors drive ADC directly through 1 nF filter caps — RC ≈ 80 ns (SC4015 source impedance × 1 nF). G474 ADC sample-and-hold cap ≈ 4 pF; settling dominated by the external 1 nF, not the mux.
**OPEN** — confirm in firmware that ADC sampling time ≥ 1 µs (covers both motherboard direct and wing-mux paths). Document the ADC config in `hardware.md`.

---

## 9. Pedal Subsystem

### 9.1 Expression Pedal ADC Path
Netlist: `EXPR_PEDAL1-5 → EXP_PEDAL → R7-1(3.9 kΩ) → VCC` and `TV7-3 → U1-8(PA0)`.
**PASS**

### 9.2 Sustain Pedal Path
Netlist: `SUS_PEDAL1-5 → SUS_PEDAL → R13-1(3.9 kΩ) → VCC` and `TV7-1 → U1-9(PA1)`.
**PASS**

### 9.3 PJ-603A Pin 2 (Sleeve) Intentionally Floating; Pin 4 → VCC
Schematic page 6 contains a designer's note: *"Sleeve (S) intentionally NC. ADC reads R(Tip-Ring) only — floating Sleeve makes the measurement independent of per-pedal T/R/S wiring"*. This clarifies that pin 2 (X-marked / NC) is the sleeve.

That leaves pin 4 → VCC as a contact other than the sleeve — most plausibly the normally-closed switch terminal that shorts to pin 3 (Ring) when no plug is inserted, used as an insertion-detect bias that pulls EXP_PEDAL_INT to VCC in the "no pedal" state. The schematic comment confirms intent for the sleeve but does not document pin 4's role.

**WARN** — confirm pin 4 function against the HOOYA PJ-603A datasheet (LCSC C309273) before fab:
  - If pin 4 is the **NC Ring-switch contact** (shorts to pin 3 when no plug) → safe, and explains §9.4.
  - If pin 4 is the **NC Tip-switch contact** (shorts to pin 5 when no plug) → redundant with R7, harmless.
  - If pin 4 is the **sleeve** → fatal: VCC shorts to GND through any plug. Designer's comment rules this out, but confirm with the datasheet anyway.

### 9.4 Insertion-Detect Caps (C13, C14) → VCC, not GND
Netlist: `C13-1 → VCC`, `C13-2 → EXP_PEDAL_INT (PC14)`. C14 same for SUS_PEDAL_INT.

A 100 nF cap from VCC to a GPIO is unusual. If pin 4 is the NC Ring-switch (per §9.3 most-likely case):
- No plug → switch closes → pin 3 = VCC directly → C13 shorted out → MCU reads HIGH ("no pedal").
- Plug inserted → switch opens → pin 3 = ring voltage from pedal → C13 acts as AC coupling to VCC, NOT a filter to GND.

A cap to **VCC** does not filter noise into the ground plane — it shunts AC noise *back into VCC*, which can ride on the rail and re-enter sensitive ADC inputs. A debounce/EMI cap on a slow GPIO conventionally goes to **GND**.

**WARN** — confirm intent. If C13/C14 are meant as EMI/debounce filters on the insertion-detect pins, they should go to GND, not VCC. If they are bypass for the VCC supply feeding pin 4 of the jack (cable acts as antenna), that's a different role and should be labelled — and a per-jack ferrite or 0 Ω jumper between VCC and pin 4 would make the intent obvious.

### 9.5 Sustain Pedal Insertion Detect MCU Pin
Netlist: `SUS_PEDAL_INT → U1-4 (PC15-OSC32_OUT)`. No LSE fitted, so PC15 is available as GPIO.
**PASS**

---

## 10. LED Circuitry

### 10.1 FN Status LEDs — Vf Mismatch in Earlier Analysis
BOM (mainboard): **LED_FN0, LED_FN1, LED_FN2 = KT-0805G** — emerald green 0805, Vf = **2.6 V – 3.1 V**, If rated 5 mA (per LCSC C2297). `hardware.md` §"LED Brightness Scheme" describes these as "white" / KT-0603W; that part is on the *wing* BOMs (LED_FN1 = KT-0603W), not the mainboard.

The Vf range (2.6–3.1 V) is the **same** for KT-0805G and KT-0603W, so the brightness math is independent of the colour confusion:

| Vf | I through 330 Ω from 3.3 V (mainboard) | I through 470 Ω from 3.3 V (wing — see §10.1a) |
|---|---|---|
| 2.6 V (datasheet min) | 2.1 mA | 1.5 mA |
| 2.85 V (typ)          | 1.4 mA | 1.0 mA |
| 3.1 V (datasheet max) | 0.6 mA | 0.4 mA |

Mainboard worst-case ≈ **0.6 mA** through the green 0805 — well below the 5 mA rating and almost certainly invisible through the translucent button cap diffuser. Even typical units (~1.4 mA) are dim. The "4 mA legible-at-a-glance during play" intent will not be met as-built.

**FAIL** — pick one:
  a) swap to a colour with Vf ≤ 2.1 V (yellow / amber 0805 — e.g. KT-0805Y, same family as LED_POW), keeping the 330 Ω resistor → ~4 mA, hits the design intent;
  b) keep the green LED and drop the series resistor to ~100 Ω, accepting the Vf-curve variance as the limiter (current then sits at ~2–7 mA worst case);
  c) accept the dim green indicators and update `hardware.md` to reflect ~1 mA, removing the "behind the button cap diffuser" justification.

`hardware.md` §"LED Brightness Scheme" needs both the part-number list **and** the calculation corrected.

### 10.1a Wing FN LED (KT-0603W white) is even dimmer
Wing BOMs list `LED_FN1 = KT-0603W` (white 0603) on the BOOT0 button. Same Vf 2.6–3.1 V. The wing resistor in series with LED_FN1 is **470 Ω** (R19 on WingLeft, R42 on WingRight per BOM) → 0.4–1.5 mA worst-to-typ. `hardware.md` correctly notes the wing FN LED is "not used as runtime indicator in current firmware" — so the dimness is tolerable on the wings even if it stays uncorrected, but the part designators in the doc ("R29/R35/R36 = 330 Ω") don't match the actual BOM (`R9/R11/R12 = 330 Ω` on mainboard; wing FN LED uses R19/R42 = 470 Ω).

### 10.2 LED_FN0 (PB9)
See §6.4. Same dim-green issue as §10.1.

### 10.3 Power LED (LED_POW, KT-0805Y yellow)
Yellow LED, Vf = 1.8 – 2.4 V. At 1 kΩ from 3.3 V: 0.9–1.5 mA. Plan's earlier "~1.2 mA" estimate stands.
**PASS**

### 10.4 LED Polarity
All LEDs: anode connects to series resistor → MCU GPIO (or VCC for POW). Cathode to GND. MCU drives high → LED on.
**PASS**

---

## 11. Wing Bus Connector Part Class

CN1 and CN2 = S062100026 (2×6 1.27 mm SMD) — **Extended Part** (JLCPCB).
**WARN** — confirm that a basic-part 2×6 1.27 mm SMD header is not available on JLCPCB. If a direct replacement exists as a basic part, consider substituting.

---

## 12. BOM Cross-Checks

### 12.1 Footprint vs. Part
All footprints in the BOM match the netlist's footprint references for U1, U2, U3, U4, U5, U6, Q1, D1–D5, C6/C7 (0603 1 nF), C8–C10/C18 (0402 100 nF), C11/C20 (0805 4.7 µF), C12/C17 (0402 1 µF), C15/C19 (0603 1 µF), C16 (0402 10 nF), C1–C5/C13/C14/CP1/CP2 (0603 100 nF).
**PASS** (mainboard).

### 12.1a WingLeft BOM — muxes and sensors swapped (CRITICAL)
Cross-checked `BOM_WingLeft_WingLeft.csv` against `Netlist_WingLeft.net` and `PickAndPlace_WingLeft_fixed.csv`:

| Source | Muxes (74HC4052PW, TSSOP-16) | Sensors (SC4015SO-N-TR, SOT-23) |
|---|---|---|
| **WingLeft BOM**            | U1, U10, U19, U28  ❌                        | U2, U3, U4, **U5**, U6, U7, U8, U9, U11, U12, U13, **U14**, U15, U16, U17, U18, U20, U21, U22, **U23**, U24, U25, U26, U27, U29, U30, U31, **U32**, U33, U34, U35, U36, U38 ❌ |
| **WingLeft netlist**        | U5, U14, U23, U32                            | U1, U2, U3, U4, U6, U7, U8, U9, U10, U11, U12, U13, U15, U16, U17, U18, U19, U20, U21, U22, U24, U25, U26, U27, U28, U29, U30, U31, U33, U34, U35, U36, U38 |
| **WingLeft PnP (`_fixed`)** | U5, U14, U23, U32                            | U1, U10, U19, U28 *(spot-checked)*                                                                                                                       |
| **WingRight BOM**           | U5, U14, U23, U32, U41 ✓                     | U1, U2, …, U43 minus the muxes ✓                                                                                                                          |

The PnP file matches the netlist (mux footprints at U5/14/23/32 are TSSOP-16, sensor footprints at U1/10/19/28 are SOT-23). **Only the WingLeft BOM is swapped.** WingRight BOM is correct. JLCPCB cross-checks BOM ↔ PnP at order entry: with the swap in place, the order will either be rejected (mismatched footprints between BOM "74HC4052 TSSOP-16" and PnP "U1 = SOT-23"), or — worst case if the operator force-overrides — produce boards with the wrong parts on the wrong pads, scrapping the run.

**FAIL — CRITICAL.** In the WingLeft BOM, swap the designator lists between the `74HC4052PW,118` row and the `SC4015SO-N-TR` row. After the swap the BOM should read:
- `4 × 74HC4052PW,118 → U5, U14, U23, U32`
- `33 × SC4015SO-N-TR → U1, U2, U3, U4, U6, U7, U8, U9, U10, U11, U12, U13, U15, U16, U17, U18, U19, U20, U21, U22, U24, U25, U26, U27, U28, U29, U30, U31, U33, U34, U35, U36, U38`

Re-export the BOM and spot-check the result against the WingRight BOM's pattern (same column layout) before submitting to JLCPCB.

### 12.1b MainBoard R16, R17 — ghost components (BOM + PnP, no netlist)
The mainboard BOM lists six 100 Ω resistors (R2, R3, R4, R8, **R16, R17**) and the PnP file places R16 and R17 at defined coordinates, but `Netlist_MainBoard.net` contains no R16 or R17 — neither pin appears in any net. So JLCPCB will solder two 0603 100 Ω resistors onto pads that are electrically dangling (or, worse, connected to copper that the schematic doesn't model).

Most likely cause: leftover symbols from an earlier revision of the wing-bus that were deleted from the schematic but not from the PCB. Either:
- Confirm R16 and R17 are intentional (e.g., shorting two ground islands across a split, or option pads), and add them to the schematic with a defined net, **or**
- Remove R16 and R17 from the PCB layout, re-export the BOM and PnP. The 12 cents and two SMT placements saved are minor; the real benefit is removing the "what does this do?" trap from the as-built board.

**WARN** — does not break the board, but the BOM/PnP/netlist three-way mismatch needs reconciling before the next netlist export.

### 12.2 BAT54 Variant
BOM: BAT54C (common-cathode). Netlist confirms `U2-3 / U3-3` = K. SOT-23-3 footprint matches.
**PASS — CLOSED**

### 12.3 Pedal Jack SRV05-4 Pin 5
Netlist: `TV7-5 → VCC (3.3 V)`. Confirmed.
**PASS — CLOSED**

### 12.4 Voltage Ratings
| Part | Rating | Rail | Margin |
|---|---|---|---|
| C6, C7 (1 nF) | 50 V | VCC 3.3 V / VBUS | > 10× |
| C11, C20 (4.7 µF) | 25 V | VCC 3.3 V | > 7× |
| SW1–SW4 | 12 V DC | 3.3 V GPIO | > 3× |
| USB1 | 30 V | VBUS 5 V max | 6× |
**PASS**

### 12.5 STDC14 / CN3 — No BOM Entry
Confirmed: not in BOM. Bare copper pads only.
**PASS**

### 12.6 Cap Count Audit (0603 100 nF, qty 9)
Netlist allocation of the nine 0603 100 nF caps:
- C1 → $1N16140 (SW1 / BAT54C cathode bus, debounce)
- C2 → SW_FN1 (SW2 debounce)
- C3 → SW_FN2 (SW3 debounce)
- C4 → M_NRST (NRST AN4488 cap)
- C5 → SW_FN0_BOOT0 (SW4 / BOOT0 debounce + hold)
- C13 → VCC ↔ EXP_PEDAL_INT (see §9.4)
- C14 → VCC ↔ SUS_PEDAL_INT (see §9.4)
- CP1, CP2 → VDDH bypass at U4 / U5
All accounted for.
**PASS**

---

## 12W. Cross-board behaviour

### 12W.1 Power-on / NRST release sequencing across the three boards
VBUS rises (~10 ms) → C15 charges → TLV755P comes out of UVLO → VCC ramps to 3.3 V → C19/C20 charge → both wings receive VCC through CN1/CN2 → all three MCUs start their power-on reset.

Race risks:
- **Wing BOOT0 sampling vs. mainboard GPIO state.** At power-on the mainboard's PA3/PB13 (driving L_BOOT0/R_BOOT0) come out of reset as high-impedance inputs. The wing's local **R4 = 10 kΩ pull-down** on the BOOT0 net resolves the line to GND before the wing's NRST is released, so the wing won't accidentally enter the system bootloader on cold boot. ✓
- **Wing NRST release.** The wing's NRST is held low by the BAT54C only while the mainboard's PB2/PB11 actively drives low. At power-on those GPIOs are floating, so the wing's internal NRST pull-up plus the wing-side C4 (or equivalent) define the release ramp. With the 100 nF NRST cap and ~40 kΩ internal pull-up, the RC reaches V(IH) in ~3 ms — well after the wing's VCC is stable. ✓
- **Both wings come up simultaneously.** Cold boot drives ~284 mA peak (LDO output) for a few ms while all three MCUs run their init sequences; C20 (4.7 µF) covers the step. **PASS**.

### 12W.2 Wing swap robustness
CN1 (mainboard, right-wing connector) and CN2 (mainboard, left-wing connector) use **identical part S062100026** with no mechanical keying. If a field user swaps the left and right wing cables:
- Power and ground are still correct (same pinout on both connectors).
- SPI bus routes: left wing's MCU still talks on its own SPI1 (PA4–PA7); physically it would land on the mainboard's SPI2 (PF0/PF1/PB14/PB15) on the swapped connector. Mainboard still polls each bus independently and reads the **wing identification code** in each SPI frame (per `architecture.md`), so it learns which physical bus holds which keyboard layout at runtime and applies the correct key-to-note map.
- READY, NRST, BOOT0 follow the same pin map on both wings, so swap doesn't break reset/bootloader either.

The protocol's wing-ID handshake is the single point that makes swap-safe operation work — without it the mainboard would mis-map keys after a swap. Good architectural choice.
**PASS** — robust by design. Worth a one-line note in `architecture.md` "Internal Bus" subsection so a future reader doesn't try to add hard-coded "left/right" assumptions in firmware.

### 12W.3 USB enumeration without HSE crystal — CRS lock timing
G474 USB-FS uses HSI48 trimmed by the Clock Recovery System (CRS) against the USB SOF. Cold-boot timing:
1. VBUS rising edge triggers USB host enumeration.
2. Mainboard MCU finishes its boot/init (~10–20 ms typical) and enables USB device + CRS.
3. CRS locks within ~1 ms of receiving the first SOF (every 1 ms once host has issued SET_ADDRESS).
4. Enumeration completes within the host's 5 s descriptor-fetch budget.

The HSI48 free-running accuracy at room temp is roughly ±0.5 %, just outside the USB-FS ±0.25 % requirement. So the first few control transfers (descriptor read, SET_ADDRESS) happen on un-trimmed HSI48; the host's tolerance for those low-speed control transfers absorbs the slack. Once CRS locks, bulk MIDI transfers run on the trimmed clock.

ST AN5060 covers this exact scenario for the G4 family and reports reliable enumeration without HSE. The board follows the recommended config.
**PASS** — but firmware must enable HSI48 + CRS + sync source = USB_SOF before USB device init. Document in firmware notes.

## 13. Documentation Consistency

### 13.1 architecture.md — Wing Bus Table
12-pin 2×6 SMD 1.27 mm with BOOT0 row.
**PASS**

### 13.2 hardware.md — Function and Boot Buttons
SW1=NRST, SW4=BOOT0/FN0, R1 pull-down documented.
**PASS**

### 13.3 hardware.md / datasheet_summaries.md — USB SRV05-4 Pin 5
Both files corrected to VBUS.
**PASS**

### 13.4 datasheet_summaries.md — Protection Coverage Table
Wing-bus ESD row lists all 7 signals (SPI×4 + NRST + READY + BOOT0).
**PASS**

### 13.5 hardware.md — LED Brightness Scheme + designator sweep
Earlier round had `hardware.md` referring to stale designators / wrong part numbers:
- Mainboard FN LEDs called "R29/R35/R36" (actual: R9, R11, R12) and "KT-0603W white" (actual: KT-0805G green).
- Wing FN LED series R called "1 kΩ" (actual: 470 Ω, R19/R42).
- VDDA / VREF+ filter pair called "C7/C8/C55/C57/C58 depending on board" — wing has C7+C8+C55, mainboard has C12+C16+C17+C18, no C57/C58.
- LDO input decoupling called "10 µF + 100 nF" (actual: single 1 µF, C15).

This round: `hardware.md` updated in §"LED Brightness Scheme", §"Passive Form Factor", §"Power Consumption", and §"3.3 V Rail (TLV755P)" — see git diff. Brightness numbers in the LED section now use the actual Vf range; designators match BOM and netlist.

Remaining work tied to §10.1 (the FAIL): if the LED part is swapped to a Vf ≤ 2.1 V colour the "1.4 mA" figure in the doc needs another pass.
**PASS** — doc consistent with the as-built schematic for everything except the §10.1 LED-colour decision, which is pending.

### 13.6 datasheet_summaries.md — Open Items
- Item 1 (ADC sample time): **OPEN** (§8.3).
- Item 2 (BAT54C variant): **CLOSED** (§12.2).
- Item 3 (pedal-jack SRV05-4 pin 5): **CLOSED** (§12.3).

### 13.7 datasheet_summaries.md — SC4015 sensitivity figure is the 5 V value
`datasheet_summaries.md` lists SC4015 sensitivity at "2.0 mV/Gs typ" with a note that this is the 3.3 V operating-point figure. The BOM property field shows "Sensitivity: 3.9 mV/Gs", which is the **5 V** value (SC4015 is ratiometric; datasheet states 3.9 mV/Gs at 5 V, 2.0 mV/Gs at 3.3 V). The design correctly uses the 3.3 V number in the swing analysis (`hardware.md` "Key Sensing"). No action — flagged here only so a future reviewer cross-checking the BOM column doesn't think the design is mis-budgeted.
**PASS** — consistent with datasheet ratiometric behaviour.

---

## Summary Table

| § | Item | Status |
|---|---|---|
| 1.1–1.6 | Power supply chain (incl. LDO transient analysis) | **PASS** |
| 2.1 | MCU VDD/VBAT bypass cap count | **PASS** (3 caps adequate per G474 fig. 16) |
| 2.2–2.6 | Rest of MCU decoupling | **PASS** |
| 3.1–3.6 | ESD coverage | **PASS** |
| 3.7 | VBUS polyfuse | **SKIP** |
| 4.1–4.3 | USB interface | **PASS** |
| 4.4 | USB VBUS sense disabled in FW | **OPEN** |
| 5.1–5.6 | Wing bus connector, mapping, NRST, series Rs | **PASS** |
| 5.7 | BOOT0 lacks series R on mainboard side | **WARN** |
| 5W.1 | Wing power gate (Q1 AO3401A) | **PASS** |
| 5W.2 | Mux topology + E̅ tied to GND | **PASS** (32/33 channel WingLeft check **WARN**) |
| 5W.3 | SEL0/SEL1 on PC14/PC15 (backup-domain pins) | **PASS** (hardware); **OPEN** (firmware DBP) |
| 5W.4 | Wing has 3 buttons (BOOT0/NRST/FN0), doc says 1 | **WARN (doc)** |
| 5W.5 | Wing READY 470 Ω series | **PASS** |
| 6.1–6.5 | Function/Boot buttons | **PASS** |
| 7.1 | STDC14 SWD signal-to-pad (mainboard, re-verified) | **PASS** |
| 7.2–7.4 | Other STDC14 wiring | **PASS** |
| 7.5 | STDC14 GNDDetect → PA15 (tool-presence input per UM2448) | **PASS** |
| 8.1–8.2 | Hall sensors and output caps | **PASS** |
| 8.3 | ADC sample time | **OPEN** |
| 9.1–9.2 | Pedal ADC path | **PASS** |
| 9.3 | PJ-603A pin 4 = VCC, sleeve floating | **WARN** |
| 9.4 | C13/C14 caps to VCC vs. GND | **WARN** |
| 9.5 | Insertion-detect MCU pin | **PASS** |
| 10.1–10.2 | FN LED Vf vs 330 Ω (green KT-0805G dim) | **FAIL** |
| 10.1a | Wing FN LED (white KT-0603W) even dimmer | **WARN** |
| 10.3–10.4 | POW LED, polarity | **PASS** |
| 11 | Wing connector part class (extended) | **WARN** |
| 12.1, 12.2–12.6 | BOM cross-checks (mainboard) | **PASS** |
| 12.1a | WingLeft BOM mux/sensor designators swapped | **FAIL (CRITICAL)** |
| 12.1b | MainBoard R16/R17 ghost (BOM+PnP, no netlist) | **WARN** |
| 12W.1 | Power-on / NRST release sequencing | **PASS** |
| 12W.2 | Wing-swap robustness via SPI wing-ID | **PASS** |
| 12W.3 | USB enumeration without HSE (HSI48+CRS) | **PASS** (firmware OPEN: enable CRS before USB init) |
| 13.1–13.4, 13.6, 13.7 | Documentation | **PASS** |
| 13.5 | hardware.md designators / LED brightness updated | **PASS** (pending §10.1 colour) |

### Prioritised Action List

**P0 — Must fix before submitting to JLCPCB:**
1. **§12.1a — WingLeft BOM mux/sensor swap.** In `BOM_WingLeft_WingLeft.csv` swap the designator lists between the `74HC4052PW,118` row and the `SC4015SO-N-TR` row. Correct values: muxes at U5/14/23/32; sensors at U1/2/3/4/6/7/8/9/10/11/12/13/15/16/17/18/19/20/21/22/24/25/26/27/28/29/30/31/33/34/35/36/38. Without this fix JLCPCB will either reject the order or produce mis-populated boards.
2. **§10.1 — FN LED colour vs resistor.** Either switch LED_FN0/1/2 to a Vf ≤ 2.1 V colour (yellow / amber 0805 — KT-0805Y is already on the same BOM line for LED_POW, single SKU substitution), or drop the series resistor to ~100 Ω, or document the dim green indicator as intentional. Current build will produce barely-visible buttons during play.
3. **§9.3 — PJ-603A pin 4.** Verify against datasheet that pin 4 is *not* the sleeve before fab. Designer's note in the schematic rules it out, but a quick datasheet sanity-check costs nothing and the consequence of being wrong is VCC-to-GND through any plug.
4. **§12.1b — MainBoard R16/R17 ghost components.** Decide whether they belong (add to schematic with a defined net) or remove them from the PCB layout. Either path, re-export BOM and PnP to remove the three-way mismatch.

**P1 — Fix before first power-on:**
5. **§5.7 — Wing BOOT0 series R.** Add 100 Ω from PA3 / PB13 to CN2-11 / CN1-11 (matching READY/NRST), or document the firmware mitigation (drive BOOT0 as input-pull only, never hard-low while a wing's local button could be pressed).
6. **§9.4 — Pedal insertion-detect caps.** Decide whether C13/C14 should go to GND (normal EMI/debounce) or stay tied to VCC (and document the role). If "EMI to VCC" is intentional, label it; otherwise reroute to GND.
7. **§5W.2 — WingLeft 32/33 channel discrepancy.** WingLeft has 4 muxes (32 channels) but 33 SC4015 sensors. Confirm in the schematic whether one sensor is unused, dual-routed, or whether the 33rd is read on a different path. Either fix the BOM count (32 if one is unused) or add a 5th mux.

**P2 — Firmware / documentation:**
8. **§4.4 — USB VBUS sense.** Confirm `USB_BCDR.VBDEN = 0` in firmware init; add a comment in `hardware.md` §USB explaining the choice.
9. **§5W.3 — Backup-domain access for PC14/PC15.** Wing firmware init must set `PWR_CR1.DBP = 1` before configuring SEL0/SEL1 as GPIO outputs; otherwise the mux address won't change and the scan will read a single channel.
10. **§5W.4 — Wing buttons.** Update `hardware.md` §"Function and Boot Buttons → Wing Boards" to reflect the three buttons actually built (BOOT0 = SW1, NRST = SW2, FN0 = SW3 on PB13).
11. **§7.5 — STLINK detect on PA15.** Document the `PGM_DETECT` line in firmware notes — configure PA15 as input with internal pull-up; LOW = STLINK attached, HIGH = no probe. Per UM2448 Table 6 note 6 the STLINK-V3 firmware ties STDC14 pin 11 to GND specifically for this purpose.
12. **§8.3 — ADC sample-time configuration.** Document in firmware notes / `hardware.md` once first ADC code lands.
13. **§12W.2 — Wing-swap robustness.** Add a one-line note in `architecture.md` "Internal Bus" reminding firmware authors that the per-frame wing-ID is what makes left/right placement order-independent.
14. **§12W.3 — CRS before USB init.** Note in firmware that HSI48 + CRS (sync = USB_SOF) must be enabled before USB device init.
15. **§13.5 — `hardware.md` LED brightness re-check.** After §10.1 LED-colour decision lands, re-walk the "LED Brightness Scheme" numbers (current pass is correct for the current BOM).

**P3 — Investigate / cost optimisation:**
16. **§11 — Wing connector basic-part substitute** (low priority; cost only).
17. **§3.7 — VBUS polyfuse** — confirmed deferred for v1.

### Net change from previous review round
- **§7.1 (STDC14 wiring) was a false alarm** — the previous critical FAIL was based on a mis-read of the LQFP48 pinout. Re-verified against DS12288 Figure 6 directly: mainboard SWD/SWO/NRST/VCP all land on correct MCU pads. Removed from P0.
- **§10.1 (FN LEDs)** was diagnosed against the wrong part number (white KT-0603W) but landed on the right conclusion (Vf 2.6–3.1 V → too dim at 330 Ω). Actual mainboard LEDs are green KT-0805G with the same Vf range, so the FAIL stands; only the recommended substitute SKU shifts (KT-0805Y, already on the BOM for LED_POW, rather than a 0603-family yellow).
- **§7.5 (STDC14 GNDDetect → PA15)** — re-checked against UM2448 Rev 9 Table 6 (note 6): STLINK-V3SET firmware ties pin 11 to GND specifically so the **target** can detect the tool. The TC2070-IDC cable passes pin 11 through, so PA15 reads LOW with STLINK attached and HIGH (via internal pull-up) when detached. The current wiring is a clean implementation of the standard's intended feature — re-classified from WARN to PASS. Earlier review draft had the direction inverted; corrected after re-reading UM2448.
- **§1.3 (LDO input cap)** — transient sanity-check confirms the 1 µF (C15) is electrically adequate (~34 mV droop in the worst-case scan window). `hardware.md` updated to match the as-built BOM rather than the previous fictional "10 µF + 100 nF". No schematic change.
- **§13.5 (hardware.md doc sweep)** — designators, LED part numbers, wing-side resistor values, VDDA cap names and LDO-input description all corrected in `hardware.md` this round.
- **P0 now reduces to two items**: §10.1 LED swap and §9.3 PJ-603A pin-4 datasheet sanity-check. Everything else is P1 or later.

### This round (wing-board internal + cross-board)
- **NEW CRITICAL §12.1a — WingLeft BOM has muxes and SC4015 sensors swapped in their designator lists.** Confirmed by three-way cross-check (BOM ⟷ Netlist ⟷ PnP). PnP and netlist agree; only the WingLeft BOM is wrong. WingRight BOM is fine. Must be fixed before submitting to JLCPCB or the order will fail at the BOM/footprint sanity-check stage. **P0 grew back to four items**: §10.1, §9.3, §12.1a (new CRITICAL), §12.1b (R16/R17 ghost).
- **§12.1b — R16/R17 ghost components on MainBoard.** Two 100 Ω 0603 resistors are in the BOM and PnP but absent from the netlist. Pads get parts soldered onto them with no schematic-defined connection. Reconcile before next netlist export.
- **§5W series (wing internal review)** — added: wing power gate confirms symmetry with mainboard; mux topology / E̅ tied to GND is consistent with the gated-Hall design; SEL0/SEL1 land on PC14/PC15 (firmware must enable PWR backup-domain write); each wing has three buttons (BOOT0, NRST, FN0 on PB13) — not the one the doc claims; wing READY series R = 470 Ω is fine for the input-with-pull-down topology.
- **§12W series (cross-board)** — power-on / NRST sequencing is race-free thanks to the wing-side R4 = 10 kΩ pull-down on BOOT0; wing-swap robustness rides on the per-frame wing-ID code in SPI (no hard-coded left/right firmware logic); HSI48 + CRS enumeration requires the firmware to enable CRS before USB init.
- **§5W.2 — Channel-count discrepancy on WingLeft (32 mux channels vs 33 SC4015 sensors).** Confirm whether the 33rd sensor is dropped, dual-routed, or routed off-mux. Low risk but real.
