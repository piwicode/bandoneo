# MainBoard Design Review Plan

Sources inspected: `export/BOM_MainBoard_MainBoard.csv`, `export/Netlist_MainBoard.net` (all nets),
`hardware.md`, `architecture.md`, `datasheet_summaries.md`, `requirements.md`.

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
**PASS** — adequate bulk and bypass on both rails.

### 1.4 LDO VIN Headroom
VBUS min 4.40 V; TLV755P dropout at 500 mA max 215 mV → min VIN needed 3.515 V.  
4.40 V − 0.215 V = 4.19 V output → regulated at 3.3 V with 0.89 V margin.  
**PASS** — comfortable headroom even at worst-case USB cable IR drop.

### 1.5 Hall-sensor Gate (Q1, AO3401A P-ch)
Netlist: `Q1-2(S) → VCC`, `Q1-3(D) → VDDH`, `Q1-1(G) → R5(1 kΩ) → PA2` and `R6(100 kΩ) → VCC`.  
P-FET: gate pull-up to VCC = OFF by default at power-on (sensors de-energised). Firmware drives PA2 low through R5 to turn on.  
**PASS** — correct default-off topology; gate resistors protect against inrush.

### 1.6 VDDH Bypass
Netlist: `CP1(100 nF 0603) + CP2(100 nF 0603)` on VDDH.  
**PASS** — two 100 nF bypasses on the gated hall rail.

---

## 2. MCU Decoupling (STM32G474CBT6, U1)

### 2.1 VDD Pin Bypass Caps
G474 LQFP48 has **3 VDD pins** (24, 36, 48) plus **VBAT** (pin 1).  
Netlist VCC net has bypass caps: **C8, C9, C10** (three × 100 nF 0402) — one per VDD pin.  
VBAT does **not** require a dedicated bypass cap: the G474 datasheet Figure 16 ("Power supply scheme") shows no cap on VBAT, and the caution note only mandates decoupling for the supply pairs listed in that figure (VDD, VDDA, VREF). With VBAT tied to VDD and no backup battery, the VDD decoupling covers it.  
`hardware.md` (lines ~84 and ~120) still references the phantom designators C50–C53 — corrected below.  
**PASS** — cap count is correct. Doc fix applied to `hardware.md`.

### 2.2 VDD Bulk Cap
Netlist: `C11 (4.7 µF 0805)` on VCC.  
**PASS** — adequate bulk reservoir.

### 2.3 VDDA Filter
Netlist: `VCC → L1 (ferrite GZ1608D601TF 600 Ω/100 MHz) → VDDA`. VDDA has `C16(10 nF) + C17(1 µF) + C12(1 µF) + C18(100 nF)` to GND. VSSA tied to GND.  
**PASS** — ferrite + multi-value VDDA decoupling per AN4488.

### 2.4 NRST Cap
Netlist: `C4 (100 nF 0603)` on M_NRST.  
**PASS** — per AN4488 recommendation.

### 2.5 VBAT Pin
Netlist: `U1-1 (VBAT) → VCC`. No backup battery fitted, VBAT tied to VCC.  
**PASS** — acceptable; RTC / backup registers not used.

### 2.6 No External Crystal
Netlist: PF0/PF1 (OSC_IN/OSC_OUT) are repurposed as R_SPI_NSS and R_SPI_SCK respectively (right-wing SPI).  
USB uses HSI48 + CRS; no LSE fitted.  
**PASS** — crystal-less USB design confirmed; PF0/PF1 re-use as SPI is valid on G474.

---

## 3. ESD and Protection Coverage

### 3.1 VBUS Clamp (D5)
See §1.1. **PASS**.

### 3.2 USB D+/D− (TV8, SRV05-4)
Netlist: `TV8-1 → USB_DP(PA12, USB1-3)`, `TV8-3 → USB_DM(PA11, USB1-2)`, `TV8-2 → GND`, **`TV8-5 → VBUS`**.  
Pin 5 (V+) tied to VBUS is correct: no PD negotiation means VBUS is always 5 V, so the upper steering diodes are safe to activate. ESD charge steers to VBUS (held by SMF5.0A) or GND — full bidirectional clamping. `hardware.md` and `datasheet_summaries.md` updated to remove the erroneous "floating" language.  
**PASS**

### 3.3 Pedal Jack ESD (TV7, SRV05-4)
Netlist: `TV7-5 → VCC (3.3 V)`, channels: `TV7-1 → SUS_PEDAL`, `TV7-3 → EXP_PEDAL`, `TV7-4 → EXP_PEDAL_INT`, `TV7-6 → SUS_PEDAL_INT`.  
`datasheet_summaries.md` says "pin 5 tied to 3.3 V". ✓  
**PASS** — consistent.

### 3.4 Wing-Bus ESD (TV1–TV4, SRV05-4 × 2 per wing)
Netlist: `TV1-5, TV2-5, TV3-5, TV4-5 → VCC`.  
**PASS** — pin 5 on 3.3 V rail (same domain as wings).

### 3.5 Programming Port ESD (TV5–TV6, SRV05-4)
See §8 (programming port wiring) — signal assignment issue is covered there.  
ESD devices are present; coverage is architecturally correct pending wiring verification.

### 3.6 Function-Button ESD (D1–D4, H5VND5BA)
Netlist: D1–D4 each have pin 1 → GND, pin 2 → respective button net.  
**PASS** — bidirectional clamp on each GPIO line.

### 3.7 VBUS Polyfuse / eFuse
Deliberately not fitted for v1. Rationale: USB 2.0 host port is spec-required to limit to 500 mA; SMF5.0A clamps transient surges; TLV755P has internal foldback + thermal shutdown. A polyfuse or eFuse would be a JLCPCB extended part with no basic-part substitute, adding cost and assembly risk for marginal protection gain in this power envelope. `hardware.md` updated accordingly.  
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
**PASS**.

### 4.3 USB Crystal-less Enumeration
HSI48 + CRS per architecture.md. No crystal in BOM. VDDA ≥ 2.4 V (3.3 V VCC) as required by G474 ADC and USB.  
**PASS**.

---

## 5. Wing Bus

### 5.1 Connector Part vs. Architecture Documentation
BOM: `CN1, CN2 = S062100026`, which is **2×6 (12-pin), 1.27 mm pitch, SMD vertical**.  
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

**PASS** — SPI1 (PA4–PA7) correctly serves left wing.

### 5.3 Wing Bus Signal-to-Pin Mapping (CN1 = right wing)
| Net | CN1 pin | MCU GPIO |
|---|---|---|
| VCC | 1, 12 | — |
| R_READY | 2 | PA8 (via R4 100 Ω) |
| R_SPI_NSS | 3 | PF0 (OSC_IN repurposed) |
| R_NRST | 4 | PB11 (via R8 100 Ω, BAT54C U3-A2) |
| R_SPI_SCK | 5 | PF1 (OSC_OUT repurposed) |
| GND | 6, 8, 10 | — |
| R_SPI_MISO | 7 | PB14 |
| R_SPI_MOSI | 9 | PB15 |
| R_BOOT0 | 11 | PB13 |

**WARN** — PF0/PF1 are used for R_SPI_NSS and R_SPI_SCK. G474 can remap SPI to alternate pins but PF0/PF1 are not standard SPI2 pins. Verify PF0/PF1 are valid GPIO-mode outputs that the firmware bit-bangs for SPI2, or that SPI2 remapping to PF0/PF1 is supported in the G474's AF table.

### 5.4 Remote BOOT0 on Wing Connector
Netlist: `CN2-11 = L_BOOT0 → PA3`, `CN1-11 = R_BOOT0 → PB13`.  
Documented in `architecture.md` bus table and `hardware.md` §"Remote Wing Programming via BOOT0": motherboard asserts BOOT0+NRST sequence to force either wing into system bootloader, enabling USB-to-wing firmware updates without physical button access.  
**PASS**

### 5.5 Wing NRST Open-Drain Topology (BAT54C)
Netlist `$1N16140` net: `U2-3(K) + U3-3(K) + SW1-2 + D1-2 + C1-1`.  
BAT54C common-cathode: at idle the cathode is pulled toward GND by D1/C1 leakage, leaving NRST lines undisturbed (reverse-biased). When the cathode net is pulled to GND (by SW1 or firmware), both wings' NRST anodes are pulled to ~Vf ≈ 0.3 V → wings held in reset.  
**PASS** — topology is valid (see also §6.4 for SW1 function).

### 5.6 Series Resistors on READY/NRST Wing Signals
R2, R3, R4, R8 are all 100 Ω. They sit between the MCU GPIO and the ESD device / connector.  
**PASS** — standard 100 Ω series termination for slow bus signals; protects MCU outputs from connector events.

---

## 6. Function and Boot Buttons

### 6.1 Switch-to-Designator Mapping (Netlist vs. Documentation)
From netlist nets:
| Designator | Net | Connected to |
|---|---|---|
| SW1 | `$1N16140` | BAT54C U2-K + U3-K (NRST cathode bus) |
| SW2 | SW_FN1 | PB6 (FN1) |
| SW3 | SW_FN2 | PB4 (FN2) |
| SW4 | SW_FN0_BOOT0 | PB8/BOOT0 |

`hardware.md` and `datasheet_summaries.md` updated with correct mapping (SW1=NRST, SW2=FN1/PB6, SW3=FN2/PB4, SW4=BOOT0/FN0/PB8). FN3 removed.  
**PASS**

### 6.2 External Pull-Down on BOOT0 (R1)
`hardware.md` button table updated: SW4/BOOT0 has external 10 kΩ pull-down (R1) for defined idle-low; FN1/FN2 use MCU internal pull-ups. The incorrect blanket "no external pull resistors" claim removed.  
**PASS**

### 6.3 SW1 = Global Reset Button
Documented in `hardware.md` §"Function and Boot Buttons": SW1 grounds the BAT54C common cathode, forward-biasing both diodes and pulling all three MCU NRST lines to ~0.3 V simultaneously.  
**PASS**

### 6.4 LED_FN0
LED_FN0 (PB9, R9 330 Ω) is the status LED behind the SW4 (BOOT0/FN0) button cap — same pattern as FN1 (PB7/SW2) and FN2 (PB5/SW3). `hardware.md` button table updated to make PB9 explicit.  
**PASS**

### 6.5 FN Button Logic Levels
SW2 (FN1) and SW3 (FN2): pin 1 → GND, pin 2 → GPIO. Active-low via MCU internal pull-up. ✓  
SW4 (BOOT0): pin 1 → VCC, pin 2 → PB8. Active-high. ✓  
**PASS**.

---

## 7. Programming Port (STDC14, CN3)

### 7.1 SWD Signal Wiring — CRITICAL
Mapping extracted from netlist (`TV5`, `TV6`, `CN3` pins):

| Net | MCU pin | CN3 pad label | Expected MCU pin |
|---|---|---|---|
| PGM_SWDIO | PA13 (SWDIO) | T_JCLK/T_SWCLK (≈ pad 6) | PA13 ✓ but wrong pad |
| PGM_USART_RX | PA10 (USART1_RX) | T_JTMS/T_SWDIO (≈ pad 4) | PA13 should be here |
| PGM_SWCLK | PA14 (SWCLK) | T_JTDO/T_SWO (≈ pad 8) | PA14 ✓ but wrong pad |
| PGM_SWO | PB3 (SWO) | #RST (≈ pad 12) | PB3 should be at SWO pad |
| M_NRST | NRST | GNDDetect (≈ pad 11) | NRST should be at NRST pad |
| PGM_USART_TX | PA9 (USART1_TX) | T_VCP_RX (≈ pad 13) | Correct (T_VCP_RX = receive from target = MCU TX) |
| PGM_DETECT | PA15 | T_VCP_TX (≈ pad 14) | T_VCP_TX should be MCU RX (PA10), PA15 should not be here |

The SWD signals (SWDIO, SWCLK, SWO) appear to be routed to the **wrong pads** of the STDC14 connector symbol. If this reflects the actual schematic, the STLINK-V3 jig will not make contact with the correct MCU signals and the board **will not program or debug** via the pogo pad interface.  
**FAIL — CRITICAL** — verify the CN3 STDC14 symbol pin numbers against the physical STDC14 land pattern. If the schematic symbol has its own internal numbering (not matching STDC14 physical pad order), map signal-by-signal. Compare:
- Physical STDC14 pad 2 → SWDIO → must reach PA13
- Physical STDC14 pad 4 → SWCLK → must reach PA14
- Physical STDC14 pad 6 → SWO → must reach PB3
- Physical STDC14 pad 10 → NRST → must reach MCU NRST pin

This is the single highest-priority item before first power-on.

### 7.2 STDC14 VCC Reference
Netlist: `CN3-3 (T_VCC) → VCC`. STLINK-V3 uses this pin to reference signal levels.  
**PASS** — correctly tied to 3.3 V.

### 7.3 GND Pins on STDC14
Netlist: `CN3-5, CN3-7 → GND`.  
**PASS** — GND returns present.

### 7.4 No Connector Body on BOM
Confirmed: CN3 has no BOM entry. Mating jig = TC2070-IDC-050 (workshop tool).  
**PASS** — documented in `hardware.md` and `architecture.md`.

---

## 8. Hall Sensor Subsystem

### 8.1 Motherboard Hall Sensors (U4, U5)
Netlist: `U4-1(VDD) + U5-1(VDD) → VDDH`, `U4-2(GND) + U5-2(GND) → GND`, `U4-3(OUT) → HALL0 → PB1`, `U5-3(OUT) → HALL1 → PB12`.  
**PASS** — push/pull sensor pair on PB1 and PB12 (ADC1 channels, per hardware.md).

### 8.2 Hall Output Filter Caps
Netlist: `C6 (1 nF 0603) → HALL0`, `C7 (1 nF 0603) → HALL1`. Both pin-1 to HALL net, pin-2 to GND.  
**PASS** — matches the SC4015 datasheet recommendation "optional 1 nF on OUT for noise filtering."

### 8.3 ADC Sample Time (Cannot Verify from Netlist)
74HC4052 Ron ≈ 80 Ω (not present on motherboard — only wing boards have muxes). Motherboard Hall sensors drive ADC directly.  
SC4015 output impedance is low; no series mux Ron.  
For wings: Ron = 80 Ω, ADC sampling cap ≈ 4 pF (G474), time constant = 320 ps — negligible. Any ADC sample time > 1 µs is more than adequate.  
**OPEN** — confirm in firmware that ADC sampling time ≥ 1 µs (several × T_RC = 320 ps → even the minimum 2.5-cycle window at 170 MHz ≈ 15 ns is sufficient). Document the firmware ADC config in `hardware.md`.

---

## 9. Pedal Subsystem

### 9.1 Expression Pedal ADC Path
Netlist: `EXPR_PEDAL1-5 → EXP_PEDAL → R7-1(3.9 kΩ) → VCC` and `TV7-3 → U1-8(PA0)`.  
Pull-up 3.9 kΩ to 3.3 V, ADC on PA0.  
**PASS** — matches hardware.md description.

### 9.2 Sustain Pedal Path
Netlist: `SUS_PEDAL1-5 → SUS_PEDAL → R13-1(3.9 kΩ) → VCC` and `TV7-1 → U1-9(PA1)`.  
**PASS** — symmetric to expression pedal.

### 9.3 PJ-603A Pin 4 = VCC — Needs Verification
Netlist: `EXPR_PEDAL1-4 → VCC (3.3 V)` and `SUS_PEDAL1-4 → VCC (3.3 V)`.  
If pin 4 is the **sleeve** of the 6.35 mm TRS jack, this would short to GND through any inserted pedal cable (sleeve = cable shield = GND), creating a direct VCC-to-GND short.  
**WARN** — verify PJ-603A pin 4 in the datasheet before fabrication. Acceptable outcomes:
  - Pin 4 = a normally-open switching contact (closes only when a specific plug is inserted) — safe.
  - Pin 4 = the sleeve conductor — **fatal error**, must remove VCC connection.

### 9.4 Insertion Detect Caps (C13, C14)
Netlist: `C13-1 → VCC`, `C13-2 → EXP_PEDAL_INT (PC14)`. Similarly C14 for SUS_PEDAL_INT.  
A 100 nF cap from VCC to the insertion-detect GPIO is unusual. It blocks DC (no pull-up function) and acts only as an AC bypass. If insertion detect relies solely on the MCU internal pull-up, C13/C14 are harmless but may slow edge detection. If the intent was a pull-up resistor, the wrong passive type (C vs. R) was placed.  
**WARN** — confirm intent of C13/C14. If they are debounce caps they should go from the net to GND, not to VCC. If they are bypass caps for the VCC supply feeding pin 4 (§9.3), they should be labelled as such.

### 9.5 Sustain Pedal Insertion Detect MCU Pin
Netlist: `SUS_PEDAL_INT → PC15-OSC32_OUT`. PC15 is the LSE output pin. No LSE is fitted, so PC15 is available as GPIO.  
**PASS** — confirmed by architecture (no LSE crystal in design).

---

## 10. LED Circuitry

### 10.1 FN Status LEDs (LED_FN1, LED_FN2)
Netlist: `LED_FN1 cathode → GND`, `R11(330 Ω) → PB7 → U1`. `LED_FN2`: R12(330 Ω) → PB5.  
At 3.3 V − 2.0 V Vf = 1.3 V across 330 Ω → ~3.9 mA. Consistent with hardware.md value of ~4 mA behind the button cap diffuser.  
**PASS**.

### 10.2 LED_FN0 (PB9)
LED behind SW4 (BOOT0/FN0) cap. See §6.4.  
**PASS**

### 10.3 Power LED (LED_POW1, KT-0805Y yellow)
Netlist: `LED_POW1 anode → R10(1 kΩ) → VCC`. Cathode → GND.  
At 3.3 V − 2.1 V / 1 kΩ ≈ 1.2 mA. Always on when VCC is live.  
**PASS**.

### 10.4 LED Polarity
All LEDs: anode connects to series resistor; cathode to GND (for LEDPOW) or cathode to GND (for FN LEDs). MCU drives the resistor side high/low.  

Wait — re-checking FN LED nets:
- `LED_FN0` net = `R9-1 → U1-46(PB9)` and `LED_FN0-2(+)` connects to R9-2. LED anode (+) → R9 → MCU.
- LED cathode (−) → GND.

MCU drives high → LED on. MCU drives low → LED off.  
**PASS** — common-anode-to-resistor topology with MCU active-high drive.

---

## 11. Wing Bus Connector Part Class

CN1 and CN2 = S062100026 (2×6 1.27 mm SMD) — **Extended Part** (JLCPCB).  
This adds cost and lead-time risk vs. basic-part connectors.  
**WARN** — confirm that a basic-part 2×6 1.27 mm SMD header is not available on JLCPCB. If a direct replacement (e.g., a standard pin-header in the same footprint) exists as a basic part, consider substituting.

---

## 12. BOM Cross-Checks

### 12.1 Footprint vs. Part
| Designator | Part | BOM Footprint | Netlist Footprint |
|---|---|---|---|
| C6, C7 | CL10B102KB8NNNC (1 nF) | C0603 | C0603 |
| C8–C10 | CL05B104KO5NNNC (100 nF) | C0402 | C0402 |
| C11, C20 | CL21A475KAQNNNE (4.7 µF) | C0805 | C0805 |
| D5 | SMF5.0A | SOD-123FL | SOD-123FL |
| D1–D4 | H5VND5BA | SOD-523 | SOD-523 |
| U1 | STM32G474CBT6 | LQFP48 | LQFP48 |
| U2, U3 | BAT54C | SOT-23-3 | SOT-23-3 |
| U4, U5 | SC4015SO-N-TR | SOT-23-3 | SOT-23-3 |
| U6 | TLV75533PDBVR | SOT-23-5 | SOT-23-5 |
| Q1 | AO3401A | SOT-23-3 | SOT-23-3 |

**PASS** — all footprints cross-check.

### 12.2 BAT54 Variant (Open Item from Previous Review)
BOM: BAT54C (common-cathode). Netlist confirms `U2-3 / U3-3` = K (cathode), pins 1 and 2 = A1, A2 (anodes).  
Common-cathode topology used to share a single SW1-driven cathode bus. SOT-23-3 footprint is correct for BAT54C.  
**PASS — CLOSED**.

### 12.3 Pedal Jack SRV05-4 Pin 5 (Open Item from Previous Review)
Netlist: `TV7-5 → VCC (3.3 V)`. Confirmed tied to 3.3 V rail.  
**PASS — CLOSED**. Update `datasheet_summaries.md` open item 3 to closed.

### 12.4 Voltage Ratings
| Part | Rating | Rail | Margin |
|---|---|---|---|
| C6, C7 (1 nF) | 50 V | VCC 3.3 V / VBUS | > 10× |
| C11, C20 (4.7 µF) | 25 V | VCC 3.3 V | > 7× |
| SW1–SW4 | 12 V DC | 3.3 V GPIO | > 3× |
| USB1 | 30 V | VBUS 5 V max | 6× |

**PASS** — all voltage ratings comfortable.

### 12.5 STDC14 / CN3 — No BOM Entry
Confirmed: not in BOM. Bare copper pads only.  
**PASS**.

---

## 13. Documentation Consistency

### 13.1 architecture.md — Wing Bus Table
Updated to 12-pin 2×6 SMD 1.27 mm with BOOT0 row. See §5.1.  
**PASS**

### 13.2 hardware.md — Function and Boot Buttons
Section rewritten: SW1=NRST, SW4=BOOT0/FN0, R1 pull-down documented. See §6.  
**PASS**

### 13.3 hardware.md / datasheet_summaries.md — USB SRV05-4 Pin 5
Both files corrected to VBUS. See §3.2.  
**PASS**

### 13.4 datasheet_summaries.md — Protection Coverage Table
Wing-bus ESD row updated to list all 7 signals (SPI×4 + NRST + READY + BOOT0, 8 channels total).  
**PASS**

### 13.5 datasheet_summaries.md — Open Items
- Item 2 (BAT54C variant): **CLOSED** (§12.2 above).
- Item 3 (pedal-jack SRV05-4 pin 5): **CLOSED** (§12.3 above).
- Item 1 (ADC sample time): still **OPEN** (§8.3 above).

---

## Summary Table

| § | Item | Status |
|---|---|---|
| 1.1–1.6 | Power supply chain | **PASS** |
| 2.1 | MCU VDD bypass cap count (3 of 4) | **FAIL** |
| 2.2–2.6 | Rest of MCU decoupling | **PASS** |
| 3.1 | VBUS TVS | **PASS** |
| 3.2 | USB SRV05-4 pin 5 documented as floating | **FAIL** |
| 3.3–3.6 | Other ESD coverage | **PASS** |
| 3.7 | Missing VBUS polyfuse | **OPEN** |
| 4.1–4.3 | USB interface | **PASS** |
| 5.1 | Wing connector part mismatch in docs | **FAIL** |
| 5.2–5.3 | Wing bus signal mapping | **PASS** |
| 5.3 note | PF0/PF1 as SPI pins — verify AF table | **WARN** |
| 5.4 | Remote BOOT0 on wing connector undocumented | **FAIL** |
| 5.5–5.6 | NRST topology, series resistors | **PASS** |
| 6.1 | SW1/SW4 roles wrong in docs | **FAIL (CRITICAL)** |
| 6.2 | R1 pull-down on BOOT0 undocumented | **FAIL** |
| 6.3 | SW1 = global reset undocumented | **FAIL** |
| 6.4 | LED_FN0 purpose not documented | **WARN** |
| 6.5 | FN button logic levels | **PASS** |
| 7.1 | SWD signal-to-pad assignment | **FAIL (CRITICAL)** |
| 7.2–7.4 | STDC14 VCC, GND, no-BOM | **PASS** |
| 8.1–8.2 | Hall sensors and output caps | **PASS** |
| 8.3 | ADC sample time | **OPEN** |
| 9.1–9.2 | Pedal ADC path | **PASS** |
| 9.3 | PJ-603A pin 4 = VCC | **WARN** |
| 9.4 | C13/C14 cap from VCC to INT pin | **WARN** |
| 9.5 | Insertion detect MCU pin | **PASS** |
| 10.1–10.4 | LED circuitry | **PASS** (WARN on LED_FN0) |
| 11 | Wing connector part class (extended) | **WARN** |
| 12.1–12.5 | BOM cross-checks | **PASS** |
| 13.1–13.5 | Documentation consistency | multiple **FAIL** |

### Prioritised Action List

**P0 — Must fix before ordering boards:**
1. **§7.1** Verify SWD signal-to-STDC14-pad wiring. If wrong, reroute in schematic.
2. **§9.3** Verify PJ-603A pin 4 function. If sleeve, remove VCC connection.

**P1 — Fix before first power-on:**
3. **§6.1–6.3** Rewrite Function/Boot Buttons docs (SW1=RESET, SW4=BOOT0, R1 pull-down).
4. **§2.1** Add 4th 100 nF bypass cap for the unbypassed VDD/VBAT pin.

**P2 — Documentation fixes (can accompany P1 fix):**
5. **§5.1, 5.4, 13.1** Update architecture.md bus table (12-pin connector, BOOT0 line).
6. **§3.2, 13.3** Correct USB SRV05-4 pin 5 = VBUS (not floating).
7. **§13.4** Update protection table (add BOOT0 to wing bus row).
8. **§6.4** Document LED_FN0 (PB9) purpose.

**P3 — Investigate / confirm:**
9. **§5.3** Confirm PF0/PF1 can serve as SPI2 SCK/NSS on G474.
10. **§9.4** Confirm intent of C13/C14 (VCC-to-INT caps).
11. **§3.7** Decide on VBUS polyfuse.
12. **§8.3** Document ADC sample-time configuration in firmware notes.
